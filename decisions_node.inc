<?php

/**
 * @file Implementation of node hooks
 */

// $Id$

/**
 * Implementation of hook_load().
 *
 * Load the votes and decision-specific data into the node object.
 */
function decisions_load($node) {
  $decision = db_fetch_object(db_query("SELECT * FROM {decisions} WHERE nid = %d", $node->nid));
  $result = db_query("SELECT vote_offset, label FROM {decisions_choices} WHERE nid = %d ORDER BY vote_offset", $node->nid);
  while ($choice = db_fetch_array($result)) {
    $decision->choice[$choice['vote_offset']] = $choice;
  }
  $decision->choices = count($decision->choice);

  // See if user has voted
  $criteria = votingapi_current_user_identifier();
  $criteria['content_type'] = 'decisions';
  $criteria['content_id'] = $node->nid;
  $decision->voted = count(votingapi_select_votes($criteria)) > 0;

  return $decision;
}

/**
 * Implementation of hook_delete().
 *
 */
function decisions_delete($node) {
  db_query("DELETE FROM {decisions} WHERE nid = %d", $node->nid);
  db_query("DELETE FROM {decisions_choices} WHERE nid = %d", $node->nid);
  db_query("DELETE FROM {decisions_electoral_list} WHERE nid = %d", $node->nid);

  // Note: this should be converted to a votingapi method eventually
  db_query("DELETE FROM {votingapi_vote} WHERE content_id = %d", $node->nid);
}

/**
 * Implementation of hook_insert()
 *
 * This is called upon node creation
 */
function decisions_insert($node) {
  // Compute startdate and runtime.
  $startdate = _decisions_translate_form_date($node->settings['date']['startdate']['date']);
  if ($node->settings['date']['noenddate']) {
    $runtime = DECISIONS_RUNTIME_INFINITY;
  }
  else {
    $enddate =  _decisions_translate_form_date($node->settings['date']['enddate']['date']);
    if ($enddate < $startdate) {
      form_set_error('enddate', t('The specified close date is less than the opening date, setting it to the same for now.'));
      $enddate = $startdate;
    }
    $runtime = $enddate - $startdate;
  }
  
  // just create an empty entry for now
  $mode = _decisions_get_mode($node);

  $algs = decisions_algorithms($mode);
  db_query("INSERT INTO {decisions} (nid, mode, quorum_abs, quorum_percent, uselist, active, runtime, maxchoices, algorithm, startdate) VALUES (%d, '%s', %d, %f, %d, %d, %d, %d, '%s', %d)", $node->nid, $mode, $node->settings['quorum_abs'], $node->settings['quorum_percent'], $node->settings['uselist'], $node->settings['active'], $node->settings['runtime'], $node->settings['maxchoices'], $algs[0], $startdate);

  // create the electoral list if desired

  if ($node->settings['uselist']) {
    _decisions_electoral_list_reset($node);
  }

  // insert the choices, same sequence than update
  decisions_update($node);
}

/**
 * Implementation of hook_validate().
 *
 * XXX: No validation yet.
 */
function decisions_validate(&$node) {
  // Use form_set_error for any errors
  $node->choice = array_values($node->choice);

  // Start keys at 1 rather than 0
  array_unshift($node->choice, '');
  unset($node->choice[0]);

  // Check for at least two choices
  $realchoices = 0;
  foreach ($node->choice as $i => $choice) {
    if ($choice['label'] != '') {
      $realchoices++;
    }
  }

  if ($realchoices < 2) {
    form_set_error("choice][$realchoices][label", t('You must fill in at least two choices.'));
  }

  $startdate = _decisions_translate_form_date($node->settings['date']['startdate']['date']);
  $enddate =  _decisions_translate_form_date($node->settings['date']['enddate']['date']);
  if ($enddate < $startdate) {
    form_set_error('enddate', t('The specified close date is less than the opening date.'));
  }

}

/**
 * Implementation of hook_update().
 *
 * This is called upon node edition.
 */
function decisions_update($node) {
  // Compute startdate and runtime.
  $startdate = _decisions_translate_form_date($node->settings['date']['startdate']['date']);
  if ($node->settings['date']['noenddate']) {
    $runtime = DECISIONS_RUNTIME_INFINITY;
  }
  else {
    $enddate =  _decisions_translate_form_date($node->settings['date']['enddate']['date']);
    if ($enddate < $startdate) {
      form_set_error('enddate', t('The specified close date is less than the opening date, setting it to the same for now.'));
      $enddate = $startdate;
    }
    $runtime = $enddate - $startdate;
  }
    
  $uselist = db_result(db_query("SELECT uselist FROM {decisions} WHERE nid = %d", $node->nid));
  if ($node->settings['uselist'] != $uselist) {
    // create the electoral list if activated
    if ($node->settings['uselist']) {
      _decisions_electoral_list_reset($node);
    }
    // remove otherwise
    else {
      db_query("DELETE FROM {decisions_electoral_list} WHERE nid = %d", $node->nid);
    }
  } 

  db_query("UPDATE {decisions} SET quorum_abs=%d, quorum_percent=%f, active=%d, runtime=%d, maxchoices=%d, algorithm='%s', uselist=%d, showvotes=%d, startdate=%d WHERE nid = %d", $node->settings['quorum_abs'], $node->settings['quorum_percent'], $node->settings['active'], $runtime, $node->settings['maxchoices'], $node->settings['algorithm'], $node->settings['uselist'], $node->settings['showvotes'], $startdate, $node->nid);
  // XXX: should update decisions here, when it has some parameters
  // XXX: ... but before doing so, the code below must be factored out in a seperate function for usage in decisions_insert()
  db_query('DELETE FROM {decisions_choices} WHERE nid = %d', $node->nid);

  // Start at one rather than 0 due to Drupal FormAPI
  $i = 1;
  foreach ($node->choice as $key => $choice) {
    // XXX: this is ugly. For some reason, formapi was passing a value
    // at the end of the array in which $choice = 0 *and*
    // isset($choice['label']) returns true. so we use this whole ugly
    // string.
    if (is_array($choice) && array_key_exists('label', $choice) && $choice['label'] != '') {
      db_query("INSERT INTO {decisions_choices} (nid, label, vote_offset) VALUES (%d, '%s', %d)", $node->nid, $choice['label'], $i++);
    }
  }
}

function decisions_submit(&$node) {
  $node->choice = array_values($node->choice);
  // Start keys at 1 rather than 0
  array_unshift($node->choice, '');
  unset($node->choice[0]);
}

/**
 * Add new choices to decisions form when AHAH isn't available
 */
function decisions_add_choices_submit($form, &$form_state) {
  // Set the form to rebuild and run submit handlers.
  node_form_submit_build_node($form, $form_state);
  if (!isset($form_state['values']['choice'])) return;

  // Add new choices 
  if ($form_state['values']['choice']['morechoices']) {
    $form_state['values']['choice']['choices'] = count($form_state['values']['choice']) + 1; // adding 1 + 2 elements
  }
}

/**
 * Add a new choice to decisions form using AHAH menu callback
 */
function decisions_add_choices_js() {

  $form_build_id = $_POST['form_build_id'];
  $form_id = $_POST['form_id'];
  
  $form_state = array('submitted' => FALSE);
  $form = form_get_cache($form_build_id, $form_state);  
  
  // Update related form data 
  $count = ++$form['choice']['choices']['#default_value'];
  $form['settings']['maxchoices']['#options'] = _decisions_choice_list($count);
  $form['choice'][$count]['label'] = _decisions_new_choice($count);

  form_set_cache($form_build_id, $form, $form_state);
  
  $form['#post'] = $_POST; 
  $form['#redirect'] = FALSE; 
  $form['#programmed'] = FALSE; 

  $form = form_builder($form_id, $form, $form_state);

  $newchoice = $form['choice'][$count];
  $output = theme('status_messages') . drupal_render($newchoice);

  drupal_json(array('status' => TRUE, 'data' => $output)); 
}

/**
 * Implementation of hook_view().
 */
function decisions_view(&$node, $teaser = FALSE, $page = FALSE) {
  $mode = _decisions_get_mode($node);

  // Since we have a body (the decision's description), we need to
  // include that in the $node->content array, too.
  $node = node_prepare($node, $teaser);
  $node->content['body']['#weight'] = -1;

  // print status messages
  $status_messages = "";
  if (!_decisions_meets_quorum($node)) {
    $status_messages .= theme_decisions_status(t("This decision is not valid. You will not be able to see it until the quorum is met. The quorum is set at @quorum.", array('@quorum' => _decisions_get_quorum($node))));
  }
  if (!$node->active) {
    $status_messages .= theme_decisions_status(t("This decision is currently closed."));
  }
  else {
    $time = time();
    if ($time < $node->startdate) {
      $status_messages .= theme_decisions_status(t("This decision is not yet opened."));
    }
    else if ($node->runtime != DECISIONS_RUNTIME_INFINITY && $time >= $node->startdate + $node->runtime) {
      $status_messages .= theme_decisions_status(t("This decision is now closed."));
    }
  }
  $node->content['decisions']['status']['#value'] = $status_messages;
  $node->content['decisions']['#weight'] = 1;  

  if (arg(2) != 'results' && _decisions_can_vote($node)) {
    // User hasn't voted and we're not on the results tab
    $node->content['decisions']['form']['#value'] = drupal_get_form('decisions_voting_form', $node, $teaser, $page);
    $node->content['decisions']['list']['#value'] = theme_decisions_view_header($node, $teaser);
  }
  elseif (_decisions_can_view_results($node)) {
    // show results only if the user has voted or decision is closed
    $node->content['decisions']['results']['#value'] = decisions_view_results($node, $teaser, $page);
    $node->content['decisions']['list']['#value'] = theme_decisions_view_header($node, $teaser);
  }
  if (isset($node->voted) && $node->voted && user_access('cancel own vote') && $node->active) {
    $node->content['decisions']['cancel']['#value'] = drupal_get_form('decisions_cancel_form', $node->nid);
  }

  return $node;
}

/**
 * Generate a new choice element in decisions form
 * 
 * @param $index position of choice item in list
 * @param $value default value of choice element 
 * @return new choice element
 */
function _decisions_new_choice($index, $value = NULL) {

  $newchoice = array(
    '#type' => 'textfield',
    '#title' => t('Choice @n', array('@n' => $index)),
    '#default_value' => "$value",
    '#attributes' => array('class' => 'choices'),
  );

  return $newchoice;
}
/*
 * Generate maxchoices value list
 * 
 * @param $choices number of choices to generate
 * @return array of choices
 */
function _decisions_choice_list($choices) {
  $max_choice_list = array();
  for ($i = 0; $i <= $choices; $i++) {
    $max_choice_list[$i] = ($i == 0? 'No limit' : $i);
  }
  return $max_choice_list;
}

/**
 * Implementation of hook_form().
 *
 * This hook displays the form necessary to edit the *node* (ie. not the votes).
 */
function decisions_form(&$node, &$form_state) {
  $mode = _decisions_get_mode($node);
  if (array_key_exists('values', $form_state)) {
    $form_values = $form_state['values']; # adapt to D6 FormsAPI
  }
  else {
    $form_values = array();
  }

  $form['title'] = array(
                         '#type' => 'textfield',
                         '#title' => ucfirst($mode) .' '. t('Question'),
                         '#required' => TRUE,
                         '#default_value' => $node->title,
                         );

  $form['body'] = array(
                        '#type' => 'textarea',
                        '#title' => t('Description'),
                        '#required' => FALSE,
                        '#default_value' => $node->body,
                        );
                       
  if (isset($form_state['values']['choice']['choices']) && is_numeric($form_state['values']['choice']['choices'])) {
    $choices = max(2, $form_state['values']['choice']['choices']);
  } 
  else {
    $choices = max(2, isset($node->choice) && count($node->choice) ? count($node->choice) : 5);
  }

  $form['choice']['js'] = array(
    '#type' => 'markup',
    '#theme' => 'decisions_morechoices',
  );  
  
  $form['choice']['choices'] = array(
    '#type' => 'hidden',
    '#default_value' => $choices,  
  );
      
  $form['choice']['morechoices'] = array(
    '#type' => 'submit',
    '#value' => t('Add more choices'),
    '#description' => t("Click this button to add more choices to this form."),
    '#weight' => 2,
    '#submit' => array('decisions_add_choices_submit'), // falback method for when JS is turned off
    '#ahah' => array(
      'path' => 'decisions/add_choices_js',
      'wrapper' => 'edit-choice-morechoices',
      'method' => 'decisions_addChoice',
      'effect' => 'slide',
    ),
  );

  // Decisions choices
  $form['choice'] += array(
                           '#type' => 'fieldset',
                           '#title' => t('Decision choices'),
                           '#description' => t('Enter all available choices here. Leave text box empty to delete a choice.'),
                           '#collapsible' => TRUE,
                           '#prefix' => '<div class="poll-form">',
                           '#suffix' => '</div>', 
                           '#tree' => TRUE,
                           '#weight' => 1,
                           );

  for ($a = 1; $a <= $choices; $a++) {
    $form['choice'][$a]['label'] = _decisions_new_choice($a, (isset($node->choice) ? $node->choice[$a]['label'] : NULL));
  }

  $form['settings'] = array(
                            '#type' => 'fieldset',
                            '#tree' => TRUE,
                            '#title' => t('Decision settings'),
                            '#collapsible' => TRUE,
                            '#collapsed' => TRUE,
                            '#weight' => 2,
                            );
 
  $form['settings']['maxchoices'] = array(
                                          '#type' => 'select',
                                          '#title' => t('Maximum Choices'),
                                          '#default_value' => (isset($node->maxchoices)? $node->maxchoices : 0),
                                          '#options' => _decisions_choice_list($choices),
                                          '#description' => t('Limits the total number of choices voters may select.'),
                                          );

  $algs = decisions_algorithms($mode);
  
  foreach ($algs as $alg) {
    $voting_algorithms[$alg] = ucwords($alg);
  }

  $defaultalg = (isset($node->algorithm) ? $node->algorithm : (isset($algs[0]) ? $algs[0] : ''));
  if (count($voting_algorithms) > 1) {
    $form['settings']['algorithm'] = array(
                                         '#type' => 'select',
                                         '#title' => t('Algorithm'),
                                         '#options' => $voting_algorithms,
                                         '#default_value' => $defaultalg,
                                         '#description' => 'Voting algorithm to use to calculate the winner.',
                                         );
  }
  else {
    $form['settings']['algorithm'] = array('#type' => 'hidden', '#value' => $defaultalg);
  }

  $active = array(1 => t('Active'), 0 => t('Closed'));
  $form['settings']['active'] = array(
                                      '#type' => 'radios',
                                      '#title' => t('Decision Status'),
                                      '#options' => $active,
                                      '#default_value' => (isset($node->active)? $node->active : 1),
                                      '#description' => t('When a decision is closed users may no longer vote on it.'),
                                      );

  $enabled = array(0 => t('Disabled'), 1 => t('Enabled'));
  $form['settings']['uselist'] = array(
                                       '#type' => 'checkbox',
                                       '#title' => t('Restrict voting to electoral list'),
                                       '#description' => t('If enabled, a list of eligible voters will be created and only that group will be able to vote in the decision.'),
                                       '#default_value' => isset($node->uselist)? $node->uselist : variable_get('decisions_default_electoral_list', DECISIONS_DEFAULT_ELECTORAL_LIST),
                                       '#options' => $enabled,
                                       );

  $form['settings']['showvotes'] = array(
                                         '#type' => 'checkbox',
                                         '#title' => t('Show individual votes'),
                                         '#description' => t('Users with the appropriate permissions will be able to see how each person voted.'),
                                         '#default_value' => isset($node->showvotes)? $node->showvotes : 1,
                                         '#options' => $enabled,
                                         );

  $form['settings']['date'] = array(
    '#type' => 'fieldset',
    '#title' => t('Date options'),
    '#collapsed' => TRUE,
    '#collapsible' => TRUE,
  );
  
  $startdate = !is_null($node->startdate) ? $node->startdate : time();
  $runtime = !is_null($node->runtime) ? $node->runtime : variable_get('decisions_default_runtime', 24 * 60 * 60);
  if ($runtime == DECISIONS_RUNTIME_INFINITY) {
    $enddate = $startdate; // by default
  }
  else {
    $enddate = $startdate + $runtime;
  }
  
  $form['settings']['date']['startdate'] = array(
    '#type' => 'fieldset',
    '#collapsed' => FALSE,
    '#description' => t('Opening date')
  );  
  $form['settings']['date']['startdate']['date'] = _decisions_form_date($startdate);
  
  $form['settings']['date']['noenddate'] = array(
    '#type' => 'checkbox',
    '#title' => t('No closing date'),
    '#default_value' => ($runtime == DECISIONS_RUNTIME_INFINITY),
    '#description' => t('Check this box if you do not want the vote to close on a specific date.'),
  );

  $form['settings']['date']['enddate'] = array(
    '#type' => 'fieldset',
    '#collapsed' => FALSE,
    '#description' => t('Closing date')
  );  
  $form['settings']['date']['enddate']['date'] = _decisions_form_date($enddate);

  
  $form['settings']['quorum'] = array(
                            '#type' => 'fieldset',
                            '#tree' => TRUE,
                            '#title' => t('Quorum'),
                            '#collapsible' => TRUE,
                            '#collapsed' => TRUE,
                            '#weight' => 2,
                            '#description' => t('These settings allow for a decision to be valid only if a certain number of eligible voters have cast their ballot. It can be configured both with an absolute number and a percentage of eligible voters. For example, a common setting is "50% + 1", which would be expressed as "@percent: 50" and "@number: 1". Note that the quorum has no impact on the winning conditions.', array('@percent' => t('Percentage (%)'), '@number' => t('Number'))),
                            );

  $form['settings']['quorum']['quorum_abs'] = array(
                                       '#type' => 'textfield',
                                       '#size' => 10,
                                       '#title' => t('Number'),
                                       '#default_value' => ($node->quorum_abs? $node->quorum_abs : 0),
                                       '#required' => TRUE,
                                       '#description' => t('Minimum number of voters required to cast their ballot for this decision to be valid, in addition to the percentage.')
                                       );

  $form['settings']['quorum']['quorum_percent'] = array(
                                       '#type' => 'textfield',
                                       '#size' => 10,
                                       '#title' => t('Percentage (%)'),
                                       '#default_value' => ($node->quorum_percent? $node->quorum_percent : 0),
                                       '#required' => TRUE,
                                       '#description' => t('Minimum numbers of voters required for this decision to be valid, expressed as a percentage of the eligible voters rounded up.'),
                                       '#weight' => -1,
                                       );
                                                                           
  return $form;
}

function theme_decisions_morechoices(&$element) {
    
    drupal_add_js('misc/jquery.form.js');
    drupal_add_js('misc/ahah.js');
    drupal_add_js(drupal_get_path('module', 'decisions') .'/decisions.js', 'module');

    $output = drupal_render($element);     
    return $output;
}
