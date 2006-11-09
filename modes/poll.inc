<?php

/**
 * @file
 *
 * This file implements a "poll-like" voting mode
 *
 * $Id$
 */

/**
 * Implementation of the decisions_longname() hook
 */
function decisions_poll_longname() {
  return t("Poll");
}


/**
 * Implementation of the decisions_algorithms() hook
 */
function decisions_poll_algorithms() {
  return array('plurality');
}

/**
 * Implementation of the decisions_node_info() hook
 */
function decisions_poll_node_info() {
  return array(
      'name' => t('decisions-poll'),
      'description' => t('Creates a poll-like vote.')
    );
}

/**
 * Implementation of the decisions_voting_hook_form() hook for the poll module.
 * 
 * This creates a list of choices to allow the user to vote on choices.
 */
function decisions_voting_poll_form(&$node, $teaser, $page) {
  if ($node->choice) {
    $list = array();

    if ($node->maxchoices == 1) {
      // plurality voting
      foreach ($node->choice as $i => $choice) {
        if ($choice['label']) {
          $list[$i] = check_plain($choice['label']);
        }
      }

      $form['choice'] = array(
                              '#type' => 'radios',
                              '#title' => $page ? '' : check_plain($node->title),
                              '#default_value' => -1,
                              '#options' => $list
                              );
    }
    else {
      // approval voting
      foreach ($node->choice as $i => $choice) {
        if ($choice['label']) {
          $list[$i] = check_plain($choice['label']);
        }
      }
      $form['choice'] = array(
                              '#type' => 'checkboxes',
                              '#title' => $page ? '' : check_plain($node->title),
                              '#options' => $list,
                              );
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
  return $form;
}

/**
 * Implementation of the decisions_view_results() hook for the poll module
 *
 * TODO: implement (http://drupal.org/node/48249)
 */
function decisions_view_results_poll($node, $teaser, $page) {
  // TODO: use VotingAPI instead of SQL
  $result = db_query("SELECT COUNT(DISTINCT uid) AS voters FROM {votingapi_vote} WHERE content_id=%d GROUP BY uid", $node->nid);
  $total_votes = db_num_rows($result);
  $results = votingapi_get_voting_results('decisions', $node->nid);

  // Count the votes for each choice
  $votes = array();
  foreach ($results as $result) {

    // approval
    $voteval = $result->tag;
    
    if (!$votes[$voteval]) {
      $votes[$voteval] = 0;
    }
    // Not sure if this can just be = rather than +=
    $votes[$voteval] += $result->value;
  }

  if ($node->choice && $total_votes > 0) {
    // TODO: Those <div>s and <br />s should be in a theme function. First collect all the data in a structure, then theme it.
    // display results for each possible choice
    $output .= '<div class="poll">';
    foreach ($node->choice as $i => $ch) {
      if (!$votes[$i]) {
        $votes[$i] = 0;
      }
      $percentage = round(100 * $votes[$i] / $total_votes, 0);
      $output .= theme('decisions_bar',  check_plain($ch['label']), $percentage, format_plural($votes[$i], '1 vote', '@count votes'));
    }
    $output .= '</div>';
  }

  $output .= '<br />';

  return $output;
}

/**
 * implementation of the format_votes() hook.
 * 
 * formats how a user's votes should be displayed.
 *
 * @returns a formatted string
 */
function decisions_format_votes_poll($node, $votes) {
  $unordered_votes = array();
  foreach ($votes as $vote) {
    // Just need one dimensional results
    if ($vote->value > 0) {
      $unordered_votes[] = check_plain($node->choice[$vote->tag]['label']);
    }
  }
  return implode(', ', $unordered_votes);
}


/**
 * implementation of the submit() hook
 *
 * registers the vote as a key for this node using votingapi_set_vote()
 */
function decisions_poll_vote($node, $form_values) {
  $votes = array();
  if ($node->maxchoices == 1) {
    // plurality voting
    $vote->value = 1;
    $vote->tag = $form_values['choice'];
    $vote->value_type = VOTINGAPI_VALUE_TYPE_KEY;
    $votes[] = $vote;
  }
  else {
    // approval voting
    foreach ($node->choice as $key => $choice) {
      $vote->value = $form_values['choice'][$key];

      if (isset($vote->value)) {
        $vote->value_type = VOTINGAPI_VALUE_TYPE_KEY;
        $vote->tag = $key;
        $votes[] = $vote;
      }
    }
  }
  votingapi_set_vote('decisions', $node->nid, $vote);
}

/**
 * implementation of the vote_validate() hook
 *
 * check if the submitted key exists, just to make sure the form is not bypassed.
 *
 * @returns boolean true if the form is valid
 */
function decisions_poll_vote_validate($node, $form_id, $form_values) {
  if ($node->maxchoices == 1) {
    // plurality voting
    if (!array_key_exists($form_values['choice'], $node->choice)) {
      form_set_error('choice', 'At least one choice must be selected.');
      return FALSE;
    }
  }
  else {
    // approval voting

    // array used to check which values are set
    $setvalues = array();
    foreach ($node->choice as $key => $choice) {

      // need a minchoices choice to do this
      //if (empty($_POST['edit']['Choice_' . $key])) {
      //form_set_error('Choice_'.$key, "choice $key cannot be empty");
      //$ok = FALSE;
      //}

      // see if the box is checked
      if (!empty($_POST['edit']['choice'][$key])) {
        $numchoices++;
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
  }
  return $ok;
}

