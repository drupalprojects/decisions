<?php
// $Id$
/**
 * Implementation of the decisions_longname() hook
 */
function decisions_poll_longname() {
  return t("Poll");
}

/**
 * Implementation of the view_voting hook for the poll module.
 * 
 * This creates a list of choices to allow the user to vote on options.
 */
function decisions_view_voting_poll(&$node, $teaser, $page, $block) {
  if ($node->option) {
    $list = array();

		if ($node->maxchoices == 1) {
			// plurality voting
    	foreach ($node->option as $i => $option) {
      	$list[$i] = check_plain($option);
    	}

    	$form['option'] = array(
      	'#type' => 'radios',
      	'#title' => $page ? '' : check_plain($node->title),
      	'#default_value' => -1,
      	'#options' => $list
    	);
		}
		else {
			// approval voting
			foreach ($node->option as $key => $option) {
				$list[$i] = check_plain($option);

      	$form['option']['Option_' . $key] = array(
        	'#type' => 'checkbox',
        	'#title' => check_plain($option),
        	'#required' => FALSE,
        	'#weight' => $weight++,
      	);
    	}
		}
  }
  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $node->nid
  );
  $form['vote'] = array(
    '#type' => 'submit',
    '#value' => t('Vote')
  );
  $form['#action'] = url('node/'. $node->nid);
  return drupal_get_form('decisions_view_voting', $form);
}

/**
 * Implementation of the decisions_view_results() hook for the poll module
 *
 * TODO: implement (http://drupal.org/node/48249)
 */
function decisions_view_results_poll($node, $teaser, $page, $block) {
  $content_type = 'decisions';
	$content_id = $node->nid;

	// TODO: use VotingAPI instead of SQL
	$result = db_query("SELECT COUNT(DISTINCT uid) AS voters FROM {votingapi_vote} WHERE content_id=%d GROUP BY uid", $node->nid);
  $totalVotes = db_num_rows($result);


	$results = votingapi_get_voting_results($content_type, $content_id);

  // Count the votes for each option
	$votes = array();
	foreach ($results as $result) {

		if ($node->maxchoices == 1) {
			// plurality
			$voteval = $result->function;
		}
		else {
			// approval
			$voteval = $result->tag;
		}

		if (!$votes[$voteval]) {
			$votes[$voteval] = 0;
		}
		// Not sure if this can just be = rather than +=
		$votes[$voteval] += $result->value;
	}

	if ($node->option && $totalVotes > 0) {
	  // display results for each possible option
		$output .= '<div class="poll">';
		foreach ($node->option as $i => $opt) {
			if (!$votes[$i]) {
				$votes[$i] = 0;
			}
			$percentage = round(100 * $votes[$i] / $totalVotes, 0);
			$output .= theme('decisions_bar',  check_plain($opt), $percentage, format_plural($votes[$i], '1 vote', '%count votes'), $block);
		}
		$output .= '</div>';
	}

	$output .= '<br />';

	return $output;
}

/**
 * implementation of the vote() hook
 *
 * registers the vote as a key for this node using votingapi_set_vote()
 */
function decisions_vote_poll($node) {
	if ($node->maxchoices == 1) {
		// plurality voting
		$vote->value = $_POST['edit']['option'];
	  $vote->value_type = VOTINGAPI_VALUE_TYPE_KEY;
	  votingapi_set_vote('decisions', $node->nid, $vote);
	}
	else {
		// approval voting
  	foreach ($node->option as $key => $option) {
    	$vote->value = $_POST['edit']['Option_' . $key];

			// A zero value indicates they didn't rank that option
			if ($vote->value != 0) {
  			$vote->value_type = VOTINGAPI_VALUE_TYPE_KEY;
   			$vote->tag = $key;
   			votingapi_set_vote('decisions', $node->nid, $vote);
			}
		}
  }
}

/**
 * implementation of the vote_validate() hook
 *
 * check if the submitted key exists, just to make sure the form is not bypassed.
 *
 * @returns boolean true if the form is valid
 */
function decisions_vote_validate_poll($node) {
	$ok = true;
	if ($node->maxchoices == 1) {
		// plurality voting
  	if (!($ok = array_key_exists($_POST['edit']['option'], $node->option))) {
    	form_set_error('option', 'At least one option must be selected.');
			$ok = false;
  	}
	}
	else {
		// approval voting

  	// array used to check which values are set
  	$setvalues = array();
  	foreach ($node->option as $key => $option) {

			// need a minchoices option to do this
    	//if (empty($_POST['edit']['Option_' . $key])) {
      	//form_set_error('Option_'.$key, "option $key cannot be empty");
      	//$ok = FALSE;
    	//}

			// see if the box is checked
    	if ($_POST['edit']['Option_' . $key]) {
				$numchoices++;
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
	}
	return $ok;
}
