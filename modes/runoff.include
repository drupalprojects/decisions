<?php
// $Id$
/**
 * Implementation of the decisions_longname() hook
 */
function decisions_runoff_longname() {
  return t("Runoff");
}

/**
 * Implementation of the view_voting_() hook for the runoff module.
 *
 * This displays a textfield per options, that should be filled with a
 * ranking.
 *
 * TODO: make this a nicer UI (http://drupal.org/node/48255)
 */
function decisions_view_voting_runoff(&$node, $teaser, $page, $block) {

  $weight = 0;

  if ($node->option) {
    $list = array();

		$numOptions = count($node->option);

		// Generate the list of possible rankings
		$options[0] = '--';
		for ($i = 1; $i <= $numOptions; $i++) {
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
			$options[$i] = $val;
		}

    foreach ($node->option as $key => $option) {
      $list[$i] = check_plain($option);

      $form['option']['Option_' . $key] = array(
        '#type' => 'select',
        '#title' => check_plain($option),
        '#required' => FALSE,
        /* XXX: that's not the right way, we need to fetch the vote using votingapi */
/*        '#default_value' => $node->option[$key],*/
				'#options' => $options,
        '#weight' => $weight++,
      );
    }
  }

  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $node->nid,
    '#weight' => $weight++,
  );
  $form['vote'] = array(
    '#type' => 'submit',
    '#value' => t('Vote'),
    '#weight' => $weight++,
  );
  $form['#action'] = url('node/'. $node->nid);
  return drupal_get_form('decisions_view_voting', $form);
}

/**
 * implementation of the decisions_view_results() hook for the runoff
 * module
 *
 * TODO: fix display (http://drupal.org/node/48248)
 */
function decisions_view_results_runoff($node, $teaser, $page, $block) {
  $runoff = _decision_calculate_runoff($node);

	// If no one has voted, $runoff = array() and thus is empty
	if (!empty($runoff)) {
    $winning_option = $node->option[$runoff->winner];
    //$output = '<p>'.t("The winner is option %option", array('%option' => $winning_option)).'</p>';

		ob_start();
		//print_r($runoff->ranking);
		$output .= nl2br(ob_get_contents()) . '<br />';
		ob_end_clean();
		$output .= t('Final results: ') . '<ol>';

		for ($i = 0; $i < count($runoff->ranking); $i++) {
			$output .= '<li> ';
			$firstOne = true;
			$roundVotes = 0;
			foreach ($runoff->ranking[$i] as $opt) {
				$output .= ($firstOne? '' : ', ') . $node->option[$opt];
				$firstOne = false;
				$roundVotes += count($runoff->matrix[count($runoff->matrix) - $i - 1][$opt]);
			}

			// The winner
			if ($i == 0) {
				$output .= ' (' . $runoff->winnerPercent . '%)';
			}
			$opt .= '</li>';
		}
		$output .= '</ol>';

    // XXX: debugging code
		if (user_access('administer decisions')) {
    	$output .= "<br />Internal values, for debugging purposes:";
    	// output the table of runoff rounds
    	$output .= "<table border=\"1\"><tr><td>Round</td>";
    	foreach ($node->option as $key => $optionname) {
      	$output .= "<td>$optionname</td>";
    	}
    	$output .= "</tr>";
    	$round = 1;
			if (count($runoff->matrix) > 0)
			{
    		foreach ($runoff->matrix as $aRound) {
      		$output .= "<tr><td>$round.</td>";
      		foreach ($node->option as $key => $optionname) {
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
 *   -rounds : 2d array listing the per-option vote count for each round and
 *							a status message indicating who was eliminated
 *   -totalVoters : the total number of voters who participated
 */
function _decision_calculate_runoff($node) {
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



	// log of 1st-place votes per option in each round
	$roundLog = array();

	//
	$reverseRanking = array();


	
	// If we eliminate one option per round and have n options, we should
	// not be able to do more than n - 1 rounds
  $maxRounds = count($node->option); 
  for ($round = 0; $round < $maxRounds; $round++) {

		// Initialize curRound
		$curRound = array();
		$totalOptions = count($node->option);
		for ($opti = 1; $opti <= $totalOptions; $opti++) {
			$curRound[$opti] = array();
		}

		
		// Loop through each user
		foreach ($userVotes as $key => $userVote) {
			// $userVote[0] contains the user's first remaining preference
			$curRound[$userVote[0]][] = $key;
		}

		if ($round == 0) {
			// This is the first round
			// Any options with no first-place votes are considered eliminated
  		foreach ($curRound as $opt => $optionVotes) {
				if (count($optionVotes) == 0) {
					unset($curRound[$opt]);
					$reverseRanking[0][] = $opt;
				}
			}
		}


    // Add the current round to the matrix
    $roundLog[] = $curRound;

    //Calculate the min and max number of votes
    $minVotes = -1;
		$maxVotes = 0;

		// Number of options that have already been discarded
    $num_discarded = 0;

		// examine the number of votes each option received this round
    foreach ($curRound as $opt => $optionVotes) {
			$numVotes = count($optionVotes);

      if ($numVotes > $maxVotes) {
        $maxVotes = $numVotes;
			  $curWinner = $opt; // store current winner in case it has a majority
      }

			// This option has already been eliminated (theoretically)
			// so don't count it as the minimum
			if ($numVotes == 0) {
				$num_discarded++; // probably don't need this variable any more
			}
			else if ($numVotes != 0 && ($numVotes < $minVotes || $minVotes == -1)) {
        $minVotes = $numVotes;
      }
    }

		// If one option has a majority of remaining users it wins
		// Note: we use count($userVotes) because some users may have incomplete
		// ballots and may have already had all of their choices eliminated
    if ($maxVotes > count($userVotes) / 2) {
		
			// Prune out the winning option if it's still in there
			if (isset($curRound[$curWinner])) {
					unset($curRound[$curWinner]);
			}

			// Keep computing until we figure out all final rankings
			while (count($curRound)  > 0) {
				// Loop through non-winning options
				$currentPlace = array();
				$min = -1;
				foreach ($curRound as $opt => $optionVotes) {
					// Option has already been eliminated, just unset it
					if (count($optionVotes) == 0) {
						unset($curRound[$opt]);
					}
					else if ($min == -1
							|| count($optionVotes) < $min) {
						// New minimum
						$currentPlace = array($opt);
						$min = count($optionVotes);
						//drupal_set_message('New minimum: '. $opt .'('
							//. count($optionVotes) . ')');
					}
					else if (count($optionVotes) == $min) {
						// Tied for minimum
						$currentPlace[] = $opt;
					}
				}

				// currentPlace will be empty the first iteration if some
				// options had no first-place votes and were eliminated
				// at the beginning
				if (count($currentPlace) > 0) {
					$reverseRanking[] = $currentPlace;	
					// Remove all options that had the minimum
					foreach ($currentPlace as $optKey) {
						unset($curRound[$optKey]);
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
		// options with the lowest number of votes.

   	// Find all options with the minimum number of votes
		$minOptions = array();
		foreach ($curRound as $opt => $optionVotes) {
			if (count($optionVotes) == $minVotes) {
				$minOptions[] = $opt;
			}
	 	}

		// Randomly select the option to eliminate out of the available options
		// TODO: due to the randomness, this result must be cached after each vote
		$roundLoser = array_rand($minOptions);
		//drupal_set_message('Round ' . ($round + 1) . ' eliminated: '
			//. strval($minOptions[$roundLoser])
			//. ' (min = ' . $minVotes . ') ' . count($curRound));
		$reverseRanking[] = array($minOptions[$roundLoser]);
		
		// Loop through the users who voted for the loser and redistribute
		foreach ($curRound[$minOptions[$roundLoser]] as $userKey) {
			// Remove their current first preference
			array_shift($userVotes[$userKey]);

			// Keep eliminating first preference until we run out or find an option
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
 *   Array containing the per-option number of first-place votes for this round
 *
 * @param $votes
 *   Array containing each individual ranking for all options
 *
 * @param $min
 *   Integer with the minimum number of votes found out of all candidates
 *     this round.
 */
function _decision_calculate_runoff_nextround($aRound, &$votes, $min) {

   }

/**
 * Implementation of the vote hook for the runoff module.
 *
 * This takes care of registering the vote in runoff nodes.
 */
function decisions_vote_runoff($node) {
  foreach ( $node->option as $key => $option ) {
    $vote->value = $_POST['edit']['Option_' . $key];

		// A zero value indicates they didn't rank that option
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
function decisions_vote_validate_runoff($node) {
  $ok = TRUE;
  // array used to check which values are set
  $setvalues = array();
	
	$numchoices = 0;
  foreach ($node->option as $key => $option) {
		
		// count the number of options that are ranked
    if (!empty($_POST['edit']['Option_' . $key])) {
			$numchoices++;
		}
    $intvalue = intval($_POST['edit']['Option_' . $key]);
    // mark this value as seen
    $setvalues[$intvalue]++;
    // check range
    if ($intvalue > count($node->option) || $intvalue < 0) {
      form_set_error('Option_'.$key, "illegal rank for option $key: $intvalue (min: 1, max: " . count($node->option) . ")");
      $ok = FALSE;
    }

  }
	
	// too many choices ranked
	if ($node->maxchoices != 0 && $numchoices > $node->maxchoices) {
		form_set_error('option',
			t('%num choices were selected but only %max are allowed.',
				array('%num' => $numchoices, '%max' => $node->maxchoices)));
		$ok = false;
	}

	// not enough choices ranked
	$minchoices = 1;
	if ($numchoices < $minchoices) {
		form_set_error('option', t('At least one option must be selected.'));
		$ok = false;
	}

	// Check that multiple options are not set to the same value
	foreach ($setvalues as $val => $count) {
		if ($val != 0 && $count > 1) {
			form_set_error('option',
				t('Multiple options given the rank of %val.', array('%val' => $val)));
			$ok = false;
		}
	}


  return $ok;
}
