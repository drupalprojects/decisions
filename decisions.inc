<?php

/**
 * @file Internal functions for decisions module
 *
 * $Id$
 */

function _decisions_get_mode($node) {
  if ($node->type) {
    $types = explode('_', $node->type, 2);
    return $types[1];
  }
  else {
    drupal_set_message('No type specified for node: ' . $node->nid, 'error');
    return '';
  }
}

/**
 * Callback function to see if a node is acceptable for poll menu items.
 */
function _decisions_votes_access($node, $perm) {
  return user_access($perm) && $node->showvotes;
}

function _decisions_electoral_list_access($node, $perm) {
  return user_access($perm) && $node->uselist;
}

/**
 * Function that tells if the given decision is open to votes.
 */
function _decisions_is_open($node) {
  $time = time();
  return ($node->active &&                    // node must be active
          // current time must be past start date and before end date
          ($time >= $node->startdate) &&
          ($node->runtime == DECISIONS_RUNTIME_INFINITY ||
           $time < ($node->startdate + $node->runtime)));
}

/**
 * Function that tells if the given user can vote on this decision.
 */
function _decisions_can_vote($node, $user = NULL) {
  return (_decisions_is_open($node) &&         // node must be open
          !$node->voted &&                    // user should not have already voted
          _decisions_eligible($node, $user));  // user must be eligible to vote
}

/**
 * Function that tells if the given decision meets the quorum.
 */
function _decisions_meets_quorum($node) {
  // compute number of people that have cast their vote
  $num_voters = _decisions_count_voters($node);
  $quorum = _decisions_get_quorum($node);
  return ($num_voters >= $quorum);
}


/**
 * Internal function factored out that just rings lots of bells when
 * we detect an unknown mode.
 */
function _decisions_panic_on_mode($mode, $function = '') {
  watchdog('decisions', 'Unknown decision mode : @mode in "@function".', array('@mode' => $mode, '@function' => $function, WATCHDOG_ERROR));
  drupal_set_message(t('Unknown decision mode : @mode in "@function".', array('@mode' => $mode, '@function' => $function), 'error'));
}

/**
 * Get all votes from the given node.
 */
function _decisions_votes($node) {
  $votes = array();
  // we bypass votingapi because we need ORDER BY value ASC lets us ensure no gaps
  $result = db_query("SELECT * FROM {votingapi_vote} v WHERE content_type='%s' AND content_id='%d' ORDER BY value ASC", 'decisions', $node->nid);
  while ($vobj = db_fetch_array($result)) {
    $votes[] = $vobj;
  }
  return $votes;
}

/**
 * Count the elligible voters for a given decision.
 */
function _decisions_count_eligible($node) {
  if ($node->uselist) {
    $result = db_fetch_object(db_query("SELECT COUNT(*) AS num FROM {decisions_electoral_list} WHERE nid=%d", $node->nid));
  } else {
    // check first if authenticated users have the right to vote, because
    // authenticated users are not added to the users_roles permission,
    // probably for performance reasons
    $roles = user_roles(FALSE, 'vote on decisions');
    if ($roles[DRUPAL_AUTHENTICATED_RID]) {
      // special case: any authenticated user can vote
      // consider all current to be elligible
      $result = db_fetch_object(db_query("SELECT COUNT(*) AS num FROM {users} u WHERE u.uid != 0"));
    } else {
      // only some roles are elligible, add relevant users only
      $result = db_fetch_object(db_query("SELECT COUNT(DISTINCT ur.uid) AS num FROM {users_roles} ur JOIN {permission} p ON ur.rid = p.rid WHERE FIND_IN_SET(' vote on decisions', p.perm) AND ur.uid != 0"));
    }
  }
  return $result->num;    
}

/**
 * Returns the quorum (minimum voters) of a node.
 */
function _decisions_get_quorum($node) {
  $num_eligible_voters = _decisions_count_eligible($node);
  $quorum = $node->quorum_abs + ceil(($node->quorum_percent / 100.0) * $num_eligible_voters);
  return min($quorum, $num_eligible_voters);
}

/**
 * Count the number of distinct voters.
 */
function _decisions_count_voters($node) {
  $num_voters = 0;
  if ($result = db_fetch_object(db_query("SELECT COUNT(DISTINCT CONCAT(uid,vote_source)) AS voters FROM {votingapi_vote} WHERE content_id=%d", $node->nid))) {
    $num_voters = $result->voters;
  }
  return $num_voters;
}

/**
 * Get all votes by uid in a an array, in a uid => votes fashion.
 */
function _decisions_user_votes($node) {
  $votes = _decisions_votes($node);
  
  // aggregate votes by user (uid if logged in, IP if anonymous)
  // in ascending order of value
  $userVotes = array();

  foreach ($votes as $vote) {
    $key = ($vote->uid == 0 ? $vote->vote_source: $vote->uid);
    $user_votes[$key][] = $vote;
  }
  
  return $user_votes;
}

/**
 * Check if user is eligible to this decision.
 */
function _decisions_eligible($node, $uid = NULL) {
  global $user;
  if (is_null($uid)) {
    $uid = $user->uid;
  }

  if ($node->uselist) {
    $can_vote = db_fetch_object(db_query("SELECT COUNT(*) AS eligible FROM {decisions_electoral_list} WHERE nid=%d AND uid=%d", $node->nid, $uid));
    $eligible = $can_vote->eligible;
  }
  else {
    $eligible = user_access('vote on decisions');
  }
  return $eligible;
}

/**
 * Constructs the time select boxes.
 * 
 * @ingroup event_support
 * @param $timestamp The time GMT timestamp of the event to use as the default
 *   value.
 * @return An array of form elements for month, day, year, hour, and minute
 */
function _decisions_form_date($timestamp) {
  // populate drop down values...
  // ...months
  $months = array(1 => t('January'), t('February'), t('March'), t('April'), t('May'), t('June'), t('July'), t('August'), t('September'), t('October'), t('November'), t('December'));
  // ...hours
  if (variable_get('event_ampm', '0')) {
    $hour_format = t('g');
    $hours = drupal_map_assoc(range(1, 12));
    $am_pms = array('am' => t('am'), 'pm' => t('pm'));
  }
  else {
    $hour_format = t('H');
    $hours = drupal_map_assoc(range(0, 23));
  }
  // ...minutes (with leading 0s)
  for ($i = 0; $i <= 59; $i++) $minutes[$i] = $i < 10 ? "0$i" : $i;

  // This is a GMT timestamp, so the _event_date() wrapper to display local times.
  $form['day'] = array(
    '#prefix' => '<div class="container-inline"><div class="day">',
    '#type' => 'textfield',
    '#default_value' => _decisions_date('d', $timestamp),
    '#maxlength' => 2,
    '#size' => 2,
    '#required' => TRUE);
  $form['month'] = array(
    '#type' => 'select',
    '#default_value' => _decisions_date('m', $timestamp),
    '#options' => $months,
    '#required' => TRUE);
  $form['year'] = array(
    '#type' => 'textfield',
    '#default_value' => _decisions_date('Y', $timestamp),
    '#maxlength' => 4,
    '#size' => 4,
    '#required' => TRUE);
  $form['hour'] = array(
    '#prefix' => '</div>&#8212;<div class="time">',
    '#type' => 'select',
    '#default_value' => _decisions_date($hour_format, $timestamp),
    '#options' => $hours,
    '#required' => TRUE);
  $form['minute'] = array(
    '#prefix' => ':',
    '#type' => 'select',
    '#default_value' => _decisions_date('i', $timestamp),
    '#options' => $minutes,
    '#required' => TRUE);
  if (isset($am_pms)) {
    $form['ampm'] = array(
      '#type' => 'radios',
      '#default_value' => _decisions_date('a', $timestamp),
      '#options' => $am_pms,
      '#required' => TRUE);
  }
  $form['close'] = array(
    '#type' => 'markup',
    '#value' => '</div></div>');

  return $form;
}

/**
 * Takes a time element and prepares to send it to form_date()
 * 
 * @param $time
 *   The time to be turned into an array. This can be:
 *   - a timestamp when from the database
 *   - an array (day, month, year) when previewing
 *   - null for new nodes
 * @returnn
 *   an array for form_date (day, month, year)
 */
function _decisions_form_prepare_datetime($time = '', $offset = 0){
  // if this is empty, get the current time
  if($time == ''){
    $time = time();
    $time = strtotime("+$offset days", $time);
  }
  // If we are previewing, $time will be an array so just pass it through
  $time_array = array();
  if(is_array($time)){
  	$time_array = $time;
  } 
  // otherwise build the array from the timestamp
  elseif(is_numeric($time)){
    $time_array = array(
      'day' => _decisions_date('j', $time),
      'month' => _decisions_date('n', $time),
      'year' => _decisions_date('Y', $time),
      'hour' => _decisions_date('H', $time),
      'min' => _decisions_date('i', $time),
      'sec' => _decisions_date('s', $time),
    );
  }
  // return the array
  return $time_array;
}

/**
 * Content of the block, as returned by decisions_block('view')
 */
function _decisions_block_mostrecent() {
  $output = '';
  $result = db_query('SELECT nid FROM {decisions} WHERE active=1 ORDER BY nid DESC LIMIT 1');
  // Check that there is an active decision
  if ($decision = db_fetch_object($result)) {
    $n = decisions_view(node_load($decision->nid), false, false, true);
    /* XXX: we have to do this because somehow the #printed settings lives across multiple node_load */
    unset($n->content['#printed']);
    $output = drupal_render($n->content);
  }
  else {
    $output = t('No active decisions.');
  }
  return $output;
}

/**
 * Returns true if the user can view the results of current node.
 */
function _decisions_can_view_results($node) {
  $view_results = variable_get('decisions_view_results', DECISIONS_DEFAULT_VIEW_RESULTS);
  return (_decisions_meets_quorum($node) && // node meets the quorum
          (!_decisions_is_open($node)       // node is closed
          || ($node->voted && $view_results == 'aftervote') // user voted
          || ($view_results == 'always'))); // all can view
}

/**
 * Insert the right users in the electoral list
 */
function _decisions_electoral_list_reset($node) {
  // check first if authenticated users have the right to vote, because authenticated users are not added to the users_roles permission, probably for performance reasons
  $result = db_fetch_object(db_query("SELECT COUNT(*) AS hit FROM {permission} JOIN role ON role.rid = permission.rid WHERE FIND_IN_SET(' vote on decisions', perm) AND role.name = 'authenticated user'"));
  if (isset($result) && $result->hit) {
    // special case: any authenticated user can vote
    // add all current users to electoral list
    return db_query("INSERT INTO {decisions_electoral_list} (nid, uid) SELECT '%d', u.uid FROM users u WHERE u.uid != 0", $node->nid);
  }
  else {
    // all users must not be allowed to vote, add relevant users only
    return db_query("INSERT INTO {decisions_electoral_list} (nid, uid) SELECT '%d', u.uid FROM users_roles u, permission p WHERE FIND_IN_SET(' view decisions', p.perm) AND u.rid = p.rid AND u.uid != 0", $node->nid);
  }

}