<?php

/**
 * @file
 * Contains the functions for the default, included CallFire SMS opt-in features
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */


/**
 * Executes the code for the SMS response feature 'stop'.
 * This feature has three variations: STOP, STOP feature,
 * and STOP ALL. If STOP is received, the user associated
 * with the number the SMS was received from will be
 * removed from the signup list of the feature that last
 * sent that user a text message. If STOP ALL is received,
 * the user will be removed from all feature signups. If
 * STOP is received, followed by the 'id' of one or more
 * specific features, the user will be removed from those
 * specific feature's signup lists.
 */
function cf_sms_response_feature_stop($from, $to, $words) {

	// first, identify the user who sent the SMS
	$data = db_fetch_array(db_query("SELECT id, features, last_sms FROM {cf_sms_users} WHERE mobile_num = '%s'", $from));
	if ($data === FALSE) {
		if (strlen($from) > 10) {
			$altered_from = substr($from, 1);
		}
		else {
			$altered_from = '1' . $from;
		}
		$data = db_fetch_array(db_query("SELECT id, features, last_sms FROM {cf_sms_users} WHERE mobile_num = '%s'", $altered_from));
	}

	// if we couldn't identify the user, just exit the function now
	if ($data === FALSE) {
		return FALSE;
	}

	// prepare to update the user's record
	$user_id = $data['id'];
	$features = $data['features'];
	$last_sms = $data['last_sms'];
	$unsignups = 0;

	// see if there were any further words; if so, unsignup from the proper service(s) and exit
	if (count($words) > 0) {
		// loop through all the words, unsignup from each
		foreach ($words as $word) {
			// if the word is ALL, just unsignup from all features and move along
			if ($word == 'ALL') {
				$features = '';
				$unsignups = 'all';
				break;
			}
			else {
				// convert the word into a tagged feature id and replace it with an empty string
				$tagged_id = ':' . strtolower($word) . ';';
				$updated_features = str_replace($tagged_id, '', $features);
				if ($updated_features != $features) {
					$unsignups++;
					$features = $updated_features;
				}
			}
		}
	}
	else {
		// no further words were included with the STOP text;
		// first, identify the feature id of the last send to that user
		$feature_id = db_result(db_query("SELECT feature_id FROM {cf_sms_sends} WHERE timestamp = %d", $last_sms));
		// if we can't find the send, we know they want a STOP; unsignup from all
		if ($feature_id === FALSE) {
			$features = '';
			$unsignups = 'all';
		}
		else {
			// we found the send; unsignup for the feature
			$tagged_id = ':' . $feature_id . ';';
			$updated_features = str_replace($tagged_id, '', $features);
			if ($updated_features != $features) {
				$unsignups++;
				$features = $updated_features;
			}
		}
	} // end of check for further words

	// update the user's record
	db_query("UPDATE {cf_sms_users} SET features = '%s' WHERE id = %d", $features, $user_id);

	// let them know they were successful (if they were)
	if ($unsignups != 0) {
		$system_from = _cf_sms_extract_mobile_num(variable_get('cf_sms_default_from_num', variable_get('callfire_default_from_num', '')));
		$ftext = ($unsignups === 1) ? 'feature' : 'features';
		$unsignups = ($unsignups === 1) ? 'a' : $unsignups;
		$message = 'STOP was successful; you have been unsubscribed from ' . $unsignups . ' text message ' . $ftext . '.';
		// do the send
		$timestamp = time();
		if ($system_from == '') {
			$xml = cfSendSMS($from, $message);
		}
		else {
			$xml = cfSendSMS($from, $message, $system_from);
		}
		// extract the ID for future reference
		$send_id = (string)$xml->children(CALLFIRE_RESOURCE_NAMESPACE)->Id;
		// create and store a record of the send
		db_query("INSERT INTO {cf_sms_sends} (id, message, timestamp, feature_id, from_num) VALUES (%d, '%s', %d, 'stop', '%s')", $send_id, $message, $timestamp, $system_from);
		// record the user and the number we sent to
		db_query("INSERT INTO {cf_sms_send_numbers} (send_id, user_id, number) VALUES (%d, %d, '%s')", $send_id, $user_id, $from);
	}

	// we are done
	return $unsignups;

}
