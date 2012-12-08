<?php

/**
 * @file - telemo.routines.inc
 *
 * Contains the general-access telephony function calls that will map to the
 * appropriate telephony service API calls as needed. Also contains the code
 * for the TeleMo scheduler, to allow actions to take place at later times.
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */
 

//require_once("cf_sms.optin.inc");
//require_once("cf_sms.response.inc");

define('TELEMO_OLD_ACTION_THRESHOLD', 259200); // three days
define('TELEMO_DEAD_ACTION_THRESHOLD', 604800); // one week
 

/*
 * Executes the next scheduled actions that are within 60 seconds of needing
 * to occur. Also executes all actions that are scheduled but their time has
 * passed, up to the threshold time later (originally 3 days.) Any scheduled
 * actions older than that are either updated to the next recurrence, or if
 * once-only (or older than the dead threshold) are deleted.
 */
function telemo_execute_next_scheduled_actions() {

	$t = time();
	$t_plus_one_min = $t + 60;
	$t_minus_old_threshold = $t - TELEMO_OLD_ACTION_THRESHOLD; // originally three days
	
	// first, lock the table so other instances of this
	// function don't cause concurrency problems
	db_lock_table('telemo_future_actions');
	
	// first thing: delete all scheduled actions older than the dead threshold
	db_query("DELETE FROM {telemo_future_actions} WHERE action_time < %d", $t - TELEMO_DEAD_ACTION_THRESHOLD);

	// now, pull out the rest of the sends for now or the past; we either need to
	// do them now, or if they're old (not dead), schedule them for the next recurrence
	$data = db_query("SELECT * FROM {telemo_future_actions} WHERE action_time < %d ORDER BY action_time", $t_plus_one_min);
	
	// since we will be doing updates & deletions while looping,
	// let's store the data in an array instead of a DB object
	$records = array();
	while ($record = db_fetch_array($data)) {
		$records[] = $record;
	}
	
	// loop through and deal with the actions we pulled
	foreach ($records as $record) {
	
		$delete = FALSE;
		
		// if the scheduled action isn't too old, we try to execute it
		if ($record['action_time'] > $t_minus_old_threshold) {
		
			$funcname = 'tm_' . $record['service_id'] . '_feature_' . $record['feature_id'];
			if (function_exists($funcname)) {
				// call the function to do the action for the feature
				if ($record['from_num'] == '') {
					$from_num = FALSE;
				}
				else {
					$from_num = $record['from_num'];
				}
				call_user_func($funcname, $from_num, $record['message'], $record['minor_data'], $record['major_data']);
			}
			else {
				// if function doesn't exist, it may be a removed external function;
				// in any case, a future action that can't be executed should be removed
				// (but we'll just let it get too old to keep, and let it get removed
				// that way, on the off chance a module is just about to be re-enabled.)
				//$delete = TRUE;
			}
		}
		
		// if we aren't already sure we're going to delete the action, just
		// advance it to a future date (unless it's a non-recurring action)
		if (!$delete) {
			// get the next scheduled time to perform the action based on
			// recurrence, advancing multiple times until we get a future time
			$new_time = $record['action_time'];
			$recurrence = $record['recurrence'];
			do {
				$new_time = _update_time_based_on_recurrence($new_time, $recurrence);
			}
			while (($new_time != 0) && ($new_time <= $t));
			
			if ($new_time == 0) {
				$delete = TRUE;
			}
		}
		
		// now we can confidently delete flagged actions
		if ($delete) {
			db_query("DELETE FROM {telemo_future_actions} WHERE id = %d", $record['id']);
		}
		// otherwise, update the DB with the next scheduled time
		else {
			db_query("UPDATE {telemo_future_actions} SET action_time = %d WHERE id = %d", $new_time, $record['id']);
		}
		
	} // end foreach - loop through all pulled records of current/old (but not dead) actions
	
	// all actions have now been dealt with;
	// unlock the table, then return how long
	// it all took, on the chance we ever
	// want/need to check execution times
	db_unlock_tables();
	return (time() - $t);
	
} // end function - telemo_execute_next_scheduled_actions


/**
 * Given a timestamp, and a code indicating the frequency of recurrence ('daily',
 * 'weekly', etc.) returns the timestamp for the event to be scheduled to occur next
 */
function _update_time_based_on_recurrence($t, $recur) {

	switch ($recur) {
	
		case 'annually':

			// add one year to the time; if they try to recur on
			// Feb. 29 annually, they'll get whatever they get
			$d = getdate($t);
			$updated_t = mktime($d['hours'], $d['minutes'], $d['seconds'], $d['mon'], $d['mday'], $d['year'] + 1);
			break;

		case 'monthly':
		
			// add one month to the time
			// (rollover so we can validate)
			$d = getdate($t);
			$month = $d['mon'] + 1;
			if ($month > 12) {
				$month = 1;
				$year = $d['year'];
			}
			else {
				$year = $d['year'];
			}
			
			// make sure it's a real date (that the month has enough days)
			$time = $d; // only 'hours', 'minutes', & 'seconds' will be extracted
			$updated_t = _is_it_a_real_date($d['mday'], $month, $year, $time);
			if ($updated_t == FALSE) {
				// if it's not a real date, we can safely advance two months w/ mktime and be guaranteed a valid date
				$updated_t = mktime($d['hours'], $d['minutes'], $d['seconds'], $d['mon'] + 2, $d['mday'], $d['year']);
			}
			break;
			
		case 'weekday-month':
		
			// cycles through the Xth weekday of the month; for example,
			// the 2nd Thursday of each month, or even the 5th Sunday of each
			// month (skipping months as necessary.)
			
			// get the date, including weekday, and determine
			// which of the weekdays it is (1st, 2nd, 3rd, etc.)
			$d = getdate($t);
			$md = $d['mday'];
			$xth = 1;
			while ($md > 7) {
				$md -= 7;
				$xth++;
			}

			// store these for use later
			$month = $d['mon'];
			$year = $d['year'];
			$hour = $d['hours'];
			$min = $d['minutes'];
			$sec = $d['seconds'];
			$weekday = $d['wday'];
			$time = array('hours' => $hour, 'minutes' => $min, 'seconds' => $sec);
			
			// we'll just do this in a straightforward fashion; advance to
			// the next month, then find the Xth weekday of that month. If
			// none, skip ahead another month until a valid one is found.
			do {
				// advance to the next month (wrap around after December)
				$month++;
				if ($month > 12) {
					$month = 1;
					$year++;
				}
				// determine the day of the week of the first day of that month & year
				$d = getdate(mktime($hour, $min, $sec, $month, 1, $year));
				$wday = $d['wday'];
				// find out what day of the month the 1st weekday of the month is
				if ($wday <= $weekday) {
					$mday = $weekday - $wday + 1;
				}
				elseif ($wday > $weekday) {
					$mday = $weekday - $wday + 8;
				}
				// now $mday represents the first weekday of the month;
				// advance to the Xth weekday of the month
				$mday_of_xth_wday = $mday + (7 * ($xth - 1));
				// check to make sure it's a real date;
				// we'll get a timestamp if it is, or else FALSE
				$updated_t = _is_it_a_real_date($mday_of_xth_wday, $month, $year, $time);
			}
			while ($updated_t == FALSE);

			break;

		case 'biweekly':
		
			// add 1209600 seconds to advance two weeks
			$updated_t = $t + 1209600;
			break;

		case 'weekly':
		
			// add 604800 seconds to advance to the next week
			$updated_t = $t + 604800;
			break;
		
		case 'weekdays':
		
			// add 86400 seconds to advance to the next day; then, if it's
			// not a weekday, keep adding 86400 seconds until it is
			$updated_t = $t + 86400;
			$d = getdate($updated_t);
			while (($d['wday'] == 0) || ($d['wday'] == 6)) {
				$updated_t += 86400;
				$d = getdate($updated_t);
			}
			break;

		case 'daily':
		
			// just add 86400 seconds to advance to the next day
			$updated_t = $t + 86400;
			break;
		
		case 'hourly':
		
			// just add 3600 seconds to advance to the next hour
			$updated_t = $t + 3600;
			break;
		
		case 'none':
		default:
		
			// there is not supposed to be a recurrence; there is no updated time
			$updated_t = 0;
	}
	
	return $updated_t;

} // end function - _update_time_based_on_recurrence


/**
 * Displays an unformatted mobile number (just a string of digits) with dashes, etc.
 * for ease of visual recognition. Strongly biased toward U.S. mobile numbers.
 */
function _telemo_display_mobile_num($num) {

	// check if the international 011 prefix is present
	if (substr($num, 0, 3) == '011') {
		// international prefix is present, so separate that, then show the rest of the digit string
		$ret_num = '+011 ' . substr($num, 3);
	}
	else {
		// if 11+ digits, don't format; just return the digit string
		$L = strlen($num);
		if ($L >= 11) {
			$ret_num = $num;
		}
		// if 10 digits, show area code separate, and standard U.S. 7-digit format
		elseif ($L == 10) {
			$ret_num = '(' . substr($num, 0, 3) . ') ' . substr($num, 3, 3) . '-' . substr($num, 6, 4);
		}
		// if less than 10 digits, don't format, just return the digit string
		else {
			$ret_num = $num;
		}
	}
	
	return $ret_num;
}


/**
 * Takes an entered mobile number, formatted in any way desired, and just extracts the
 * number itself by stripping out any non-digits and returning only a string of digits
 */
function _telemo_extract_mobile_num($num) {

	$ret_num = '';
	
	for ($i = 0; $i < strlen($num); $i++) {
	
		$d = $num[$i];
		
		if (($d >= '0') && ($d <= '9')) {
			$ret_num .= $d;
		}
	}
	
	return $ret_num;
}


/*
 * Determines if the given number string is "empty" (no non-zero digits)
 */
function _mobile_num_is_empty($num) {

	static $non_zero_digits = "123456789";
	
	$no_non_zero_digits_found = TRUE;
	
	for ($i=0; $i < strlen($num); $i++) {
		$p = strpos($non_zero_digits, $num[$i]);
		if ($p !== FALSE) {
			$no_non_zero_digits_found = FALSE;
			break;
		}
	}
	
	return $no_non_zero_digits_found;
}

	
/**
 * Returns non-boolean TRUE if the date is real (specifically, the actual timestamp of the
 * date matches,) otherwise FALSE for "fake" dates (for ex.: Feb. 29, 2013 or 2012/13/-1)
 * Just like PHP's time functions, day should be a number 1-31, month is 1-12, year is four
 * digits. May optionally include an array with elements 'hours', 'minutes' and 'seconds'.
 */
function _is_it_a_real_date($day, $month, $year, $time = FALSE) {

	if (($time !== FALSE) && is_array($time)) {
		$t = mktime($time['hours'], $time['minutes'], $time['seconds'], $month, $day, $year);
	}
	else {
		$t = mktime(0, 0, 0, $month, $day, $year);
	}
	
	$d = getdate($t);
	$real = $t;
	if (($d['mday'] != $day) || ($d['mon'] != $month) || ($d['year'] != $year)) {
		$real = FALSE;
	}
	
	return $real;
}


/**
 * Given an array of arrays, and a key present in each of the subarrays,
 * returns the values of those subelements with the given key.
 */
/*function _array_subvalues($arr, $key) {

	$ret = array();
	
	foreach ($arr as $row) {
		$ret[] = $row[$key];
	}
	
	return $ret;
}*/
