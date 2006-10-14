<?php
// $Id$
/**
 * Implementation of the decisions_longname() hook
 */
function decisions_ranking_longname() {
  return t("Runoff");
}

/**
 * Implementation of the view_voting_() hook for the runoff module.
 *
 * This displays a textfield per choice, that should be filled with a
 * ranking.
 *
 * TODO: make this a nicer UI (http://drupal.org/node/48255)
 */
function decisions_view_voting_ranking(&$node, $teaser, $page, $block) {

  $weight = 0;
	$form = array();

  if ($node->choice) {
    $list = array();

		$numChoices = count($node->choice);

		// Generate the list of possible rankings
		$choices[0] = '--';
		for ($i = 1; $i <= $numChoices; $i++) {
			if ($i == 1) {
				$val = t('1st');
			}
			elseif ($i == 2) {
				$val = t('2nd');
			}
			elseif ($i == 3) {
				$val = t('3rd');
			}
			else {
				$val = t(strval($i) . 'th');
			}
			$choices[$i] = $val;
		}

		$form['choice'] = array();
    foreach ($node->choice as $key => $choice) {
      $list[$i] = check_plain($choice);

      $form['choice']['Option_' . $key] = array(
        '#type' => 'select',
        '#title' => check_plain($choice),
        '#required' => FALSE,
        /* XXX: that's not the right way, we need to fetch the vote using votingapi */
/*        '#default_value' => $node->choice[$key],*/
				'#options' => $choices,
        '#weight' => $weight++,
      );
    }
  }

  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $node->nid,
    '#weight' => $weight++,
  );

	if (!$node->in_preview) {
  	$form['vote'] = array(
    	'#type' => 'submit',
    	'#value' => t('Vote'),
    	'#weight' => $weight++,
  	);
	}

  $form['#action'] = url('node/'. $node->nid);
  return drupal_get_form('decisions_view_voting', $form);
}

/**
 * implementation of the decisions_view_results() hook for the runoff
 * module
 *
 * TODO: fix display (http://drupal.org/node/48248)
 */
function decisions_view_results_ranking($node, $teaser, $page, $block) {
  $runoff = _decision_calculate_ranking($node);

	// If no one has voted, $runoff = array() and thus is empty
	if (!empty($runoff)) {
    $winning_choice = $node->choice[$runoff->winner];
    //$output = '<p>'.t("The winner is choice %choice", array('%choice' => $winning_choice)).'</p>';

		ob_start();
		//print_r($runoff->ranking);
		$output .= nl2br(ob_get_contents()) . '<br />';
		ob_end_clean();
		$output .= t('Final results: ') . '<ol>';

		for ($i = 0; $i < count($runoff->ranking); $i++) {
			$output .= '<li> ';
			$firstOne = true;
			$roundVotes = 0;
			foreach ($runoff->ranking[$i] as $choice) {
				$output .= ($firstOne? '' : ', ') . $node->choice[$choice];
				$firstOne = false;
				$roundVotes += count($runoff->matrix[count($runoff->matrix) - $i - 1][$choice]);
			}

			// The winner
			if ($i == 0) {
				$output .= ' (' . $runoff->winnerPercent . '%)';
			}
			$output .= '</li>';
		}
		$output .= '</ol>';

    // XXX: debugging code
		if (user_access('administer decisions')) {
    	$output .= "<br />Internal values, for debugging purposes:";
    	// output the table of runoff rounds
    	$output .= "<table border=\"1\"><tr><td>Round</td>";
    	foreach ($node->choice as $key => $choicename) {
      	$output .= "<td>$choicename</td>";
    	}
    	$output .= "</tr>";
    	$round = 1;
			if (count($runoff->matrix) > 0)
			{
    		foreach ($runoff->matrix as $aRound) {
      		$output .= "<tr><td>$round.</td>";
      		foreach ($node->choice as $key => $choicename) {
        		$output .= '<td>' . count($aRound[$key]) . '</td>';
      		}
      		$output .= "</tr>";
      		$round++;
    		}
			}

    	$output .= "</table>";
		}

    return $output;
	}
}

/**
 * Calculate the results (aggregate preference) for an IRV election
 * 
 * @param $node
 *	The node object for the current decision
 *
 * @return 
 * 	Should return an object that include the following attributes
 *   -results : 2d array listing the aggregate preference, including ties
 *   -rounds : 2d array listing the per-choice vote count for each round and
 *							a status message indicating who was eliminated
 *   -totalVoters : the total number of voters who participated
 */
function _decision_calculate_ranking($node) {
 	$votes = array();
  // ORDER BY value ASC lets us ensure no gaps
  $result = db_query("SELECT * FROM {votingapi_vote} v WHERE content_type='%s' AND content_id=%d ORDER BY value ASC", 'decisions', $node->nid);
  while ($vobj = db_fetch_object($result)) {
    $votes[] = $vobj;
  }

	if (count($votes) == 0) {
		// no votes yet
		return array();
	}

	// aggregate votes by user (uid if logged in, IP if anonymous)
	// in ascending order of value
	$userVotes = array();

  foreach ($votes as $vote) {
		if ($vote->uid == 0) {
			// anonymous user
			$key = $vote->hostname;
		}
		else {
			// logged-in user
			$key = $vote->uid;
		}

		// Note: relies on ORDER BY value ASC in vote-getting SQL query
		// Otherwise a later vote might have a lower value
		$userVotes[$key][] = $vote->tag;
	}

	$totalVotes = count($userVotes);

/*
    if ($vote->value == 1) {
    	$curRound[$vote->tag]++;
			// TODO: This method of counting total votes is inaccurate because users
			// may vote but not choose a 1st-place vote
			$totalvotes++;
    }
		*/



	// log of 1st-place votes per choice in each round
	$roundLog = array();

	//
	$reverseRanking = array();


	
	// If we eliminate one choice per round and have n choices, we should
	// not be able to do more than n - 1 rounds
  $maxRounds = count($node->choice); 
  for ($round = 0; $round < $maxRounds; $round++) {

		// Initialize curRound
		$curRound = array();
		$totalChoices = count($node->choice);
		for ($chi = 1; $chi <= $totalChoices; $chi++) {
			$curRound[$chi] = array();
		}

		
		// Loop through each user
		foreach ($userVotes as $key => $userVote) {
			// $userVote[0] contains the user's first remaining preference
			$curRound[$userVote[0]][] = $key;
		}

		if ($round == 0) {
			// This is the first round
			// Any choices with no first-place votes are considered eliminated
  		foreach ($curRound as $ch => $choiceVotes) {
				if (count($choiceVotes) == 0) {
					unset($curRound[$ch]);
					$reverseRanking[0][] = $ch;
				}
			}
		}


    // Add the current round to the matrix
    $roundLog[] = $curRound;

    //Calculate the min and max number of votes
    $minVotes = -1;
		$maxVotes = 0;

		// Number of choices that have already been discarded
    $num_discarded = 0;

		// examine the number of votes each choice received this round
    foreach ($curRound as $ch => $choiceVotes) {
			$numVotes = count($choiceVotes);

      if ($numVotes > $maxVotes) {
        $maxVotes = $numVotes;
			  $curWinner = $ch; // store current winner in case it has a majority
      }

			// This choice has already been eliminated (theoretically)
			// so don't count it as the minimum
			if ($numVotes == 0) {
				$num_discarded++; // probably don't need this variable any more
			}
			else if ($numVotes != 0 && ($numVotes < $minVotes || $minVotes == -1)) {
        $minVotes = $numVotes;
      }
    }

		// If one choice has a majority of remaining users it wins
		// Note: we use count($userVotes) because some users may have incomplete
		// ballots and may have already had all of their choices eliminated
    if ($maxVotes > count($userVotes) / 2) {
		
			// Prune out the winning choice if it's still in there
			if (isset($curRound[$curWinner])) {
					unset($curRound[$curWinner]);
			}

			// Keep computing until we figure out all final rankings
			while (count($curRound)  > 0) {
				// Loop through non-winning choices
				$currentPlace = array();
				$min = -1;
				foreach ($curRound as $ch => $choiceVotes) {
					// Choice has already been eliminated, just unset it
					if (count($choiceVotes) == 0) {
						unset($curRound[$ch]);
					}
					else if ($min == -1
							|| count($choiceVotes) < $min) {
						// New minimum
						$currentPlace = array($ch);
						$min = count($choiceVotes);
						//drupal_set_message('New minimum: '. $ch .'('
							//. count($choiceVotes) . ')');
					}
					else if (count($choiceVotes) == $min) {
						// Tied for minimum
						$currentPlace[] = $ch;
					}
				}

				// currentPlace will be empty the first iteration if some
				// choices had no first-place votes and were eliminated
				// at the beginning
				if (count($currentPlace) > 0) {
					$reverseRanking[] = $currentPlace;	
					// Remove all choices that had the minimum
					foreach ($currentPlace as $chKey) {
						unset($curRound[$chKey]);
					}
				}
			}

			// Save a reversed version of the round log to help compute winnerPercent
			$revmat = array_reverse($roundLog);

			// The winner finally gets added
			$reverseRanking[] = array($curWinner);

    	$resultObj->winner = $curWinner;
			$resultObj->winnerPercent = round(count($revmat[0][$curWinner]) * 100 / count($userVotes), 1);
      $resultObj->matrix = $roundLog;
			$resultObj->totalVotes = $totalVotes;
			$resultObj->ranking = array_reverse($reverseRanking);
      return $resultObj;
    }
		
		// Since we're still here, no one has won, so eliminate one of the
		// choices with the lowest number of votes.

   	// Find all choices with the minimum number of votes
		$minChoices = array();
		foreach ($curRound as $ch => $choiceVotes) {
			if (count($choiceVotes) == $minVotes) {
				$minChoices[] = $ch;
			}
	 	}

		// Randomly select the choice to eliminate out of the available choices
		// TODO: due to the randomness, this result must be cached after each vote
		$roundLoser = array_rand($minChoices);
		//drupal_set_message('Round ' . ($round + 1) . ' eliminated: '
			//. strval($minChoices[$roundLoser])
			//. ' (min = ' . $minVotes . ') ' . count($curRound));
		$reverseRanking[] = array($minChoices[$roundLoser]);
		
		// Loop through the users who voted for the loser and redistribute
		foreach ($curRound[$minChoices[$roundLoser]] as $userKey) {
			// Remove their current first preference
			array_shift($userVotes[$userKey]);

			// Keep eliminating first preference until we run out or find an choice
			// that hasn't been eliminated
			while ($curRound[$userVotes[$userKey][0]] == array()
				&& count($userVotes[$userKey]) > 0)
			{
				array_shift($userVotes[$userKey]);
			}

			// If they have no more preferences, remove from list for simplicity
			if (count($userVotes[$userKey]) == 0) {
				unset($userVotes[$userKey]);
			}
		}
	}
  // loop detected. signal user and record.
  watchdog('decisions', "Could not reach a decision within $maxiter iterations", WATCHDOG_ERROR);
  drupal_set_message("Could not reach a decision within $maxiter iterations");
  $resultObj->matrix = $roundLog;
	$resultObj->totalVotes = $totalVotes;
  return $resultObj;

}

/**
 * Eliminates the candidate with the lowest number of ballots and redistributes
 * those votes to the voters' next preference.
 *
 * @param $aRound
 *   Array containing the per-choice number of first-place votes for this round
 *
 * @param $votes
 *   Array containing each individual ranking for all choices
 *
 * @param $min
 *   Integer with the minimum number of votes found out of all candidates
 *     this round.
 */
function _decision_calculate_ranking_nextround($aRound, &$votes, $min) {

   }

/**
 * Implementation of the vote hook for the runoff module.
 *
 * This takes care of registering the vote in runoff nodes.
 */
function decisions_vote_ranking($node) {
  foreach ( $node->choice as $key => $choice ) {
    $vote->value = $_POST['edit']['Option_' . $key];

		// A zero value indicates they didn't rank that choice
		if ($vote->value != 0) {
    	$vote->value_type = VOTINGAPI_VALUE_TYPE_KEY;
    	$vote->tag = $key;
    	votingapi_set_vote('decisions', $node->nid, $vote);
		}
  }
}

/**
 * implementation of the vote validation hook for the runoff module.
 *
 * This checks if the submitted values are within range, if they are
 * not empty, and if they are not repeated.
 *
 * @returns boolean false on invalid forms, true otherwise.
 */
function decisions_vote_validate_ranking($node) {
  $ok = TRUE;
  // array used to check which values are set
  $setvalues = array();
	
	$numchoices = 0;
  foreach ($node->choice as $key => $choice) {
		
		// count the number of choices that are ranked
    if (!empty($_POST['edit']['Option_' . $key])) {
			$numchoices++;
		}
    $intvalue = intval($_POST['edit']['Option_' . $key]);
    // mark this value as seen
    $setvalues[$intvalue]++;
    // check range
    if ($intvalue > count($node->choice) || $intvalue < 0) {
      form_set_error('Option_'.$key, "illegal rank for choice $key: $intvalue (min: 1, max: " . count($node->choice) . ")");
      $ok = FALSE;
    }

  }
	
	// too many choices ranked
	if ($node->maxchoices != 0 && $numchoices > $node->maxchoices) {
		form_set_error('choice',
			t('%num choices were selected but only %max are allowed.',
				array('%num' => $numchoices, '%max' => $node->maxchoices)));
		$ok = false;
	}

	// not enough choices ranked
	$minchoices = 1;
	if ($numchoices < $minchoices) {
		form_set_error('choice', t('At least one choice must be selected.'));
		$ok = false;
	}

	// Check that multiple choices are not set to the same value
	foreach ($setvalues as $val => $count) {
		if ($val != 0 && $count > 1) {
			form_set_error('choice',
				t('Multiple choices given the rank of %val.', array('%val' => $val)));
			$ok = false;
		}
	}


  return $ok;
}
