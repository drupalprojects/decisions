<?php

/**
 * @file
 *
 * Date and time routines for use with decisions module.
 * Based on quiz module (which is, in turn, based on the event.module)
 * All references to event variables should be optional 
 * 
 */

/**
 * Handles the start and end times in a node form submission.
 * - Takes the array from form_date() and turns it into a timestamp
 * - Adjusts times for time zone offsets.
 * - Adapted from quiz.module
 *
 * @param $node The submitted node with form data.
 * @param $date The name of the date ('decisions_open' or 'decisions_close') to translate.
 */
function _decisions_translate_form_date($array) {
  return _decisions_mktime($array['hour'], $array['minute'], 59, $array['month'], $array['day'], $array['year'], 0);
}

/**
 * Formats local time values to GMT timestamp using time zone offset supplied.
 * All time values in the database are GMT and translated here prior to insertion.
 *
 * Time zone settings are applied in the following order:
 * 1. If supplied, time zone offset is applied
 * 2. If user time zones are enabled, user time zone offset is applied
 * 3. If neither 1 nor 2 apply, the site time zone offset is applied
 *
 * @param $format The date() format to apply to the timestamp.
 * @param $timestamp The GMT timestamp value.
 * @param $offset Time zone offset to apply to the timestamp.
 * @return gmdate() formatted date value
 */
function _decisions_mktime($hour, $minute, $second, $month, $day, $year, $offset = NULL) {
  global $user;
  //print $user->timezone. " and ". variable_get('date_default_timezone', 0);
  $timestamp = gmmktime($hour, $minute, $second, $month, $day, $year);
  if (variable_get('configurable_timezones', 1) && $user->uid && strlen($user->timezone)) {
    return $timestamp - $user->timezone;
  }
  else {
    return $timestamp - variable_get('date_default_timezone', 0);
  }
}

/**
 * Formats a GMT timestamp to local date values using time zone offset supplied.
 * All timestamp values in event nodes are GMT and translated for display here.
 *
 * Pulled from event
 *
 * Time zone settings are applied in the following order
 * 1. If supplied, time zone offset is applied
 * 2. If user time zones are enabled, user time zone offset is applied
 * 3. If neither 1 nor 2 apply, the site time zone offset is applied
 *
 * @param $format The date() format to apply to the timestamp.
 * @param $timestamp The GMT timestamp value.
 * @param $offset Time zone offset to apply to the timestamp.
 * @return gmdate() formatted date value
 */
function _decisions_date($format, $timestamp, $offset = null) {
  global $user;

  if (!is_null($offset)) {
    $timestamp += $offset;
  }
  elseif (variable_get('configurable_timezones', 1) && $user->uid && strlen($user->timezone)) {
    $timestamp += $user->timezone;
  }
  else {
    $timestamp += variable_get('date_default_timezone', 0);
  }

  // make sure we apply the site first day of the week setting for dow requests
  $result = gmdate($format, $timestamp);
  return  $result;
}

?>