<?php

/**
 * @file
 * Contains the functions for the default, included CallFire SMS opt-in features
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */
 

define('CALLFIRE_WEBSITE', "https://callfire.com");
 
define('CF_SMS_SIGNUP_MESSAGE', "Please choose which text messaging features you would like to receive.");


/**
 * Executes the code for the SMS opt-in feature 'promos':
 * Users signed up for this feature will receive information
 * about various promotions, discounts, and other specials
 * that your website and/or store is offering. These promos
 * can be sent out immediately by site admins, or can be
 * setup to be regularly recurring. The text messages sent
 * are intended to not vary among the recipients.
 */
function cf_sms_optin_feature_promos($message, $from_num, $data) {

	// get the info for the send
	$signups = _get_signups_for_sms_feature('promos');
	$num_array = _array_subvalues($signups, 'mobile_num');
	// do the send
	$timestamp = time();
	$xml = cfSendSMS($num_array, $message, $from_num);
	// extract the ID for future reference
	$send_id = (string)$xml->children(CALLFIRE_RESOURCE_NAMESPACE)->Id;
	// create and store a record of the send
	db_query("INSERT INTO {cf_sms_sends} (id, message, timestamp, feature_id, from) VALUES (%d, '%s', %d, '%s', %d)", $send_id, $message, $timestamp, 'promos', $from_num);
	// record all the users, and their numbers, we sent to
	foreach ($signups as $signup) {
		db_query("INSERT INTO {cf_sms_send_numbers} (send_id, user_id, number) VALUES (%d, %d, '%s')", $send_id, $signup['id'], $signup['mobile_num']);
	}
	// record the last public send for use by other features
	variable_set('cf_sms_last_public_sms_message', $message);
	variable_set('cf_sms_last_public_sms_time', $timestamp);

}


/*
 * Executes the code for the SMS opt-in feature 'info':
 * Users signed up for this feature will receive informative
 * alerts that the site admins may broadcast at any time.
 * These messages are generally not intended to have any
 * user-specific elements (such as including the user's
 * name or order number in the message,) nor to be
 * scheduled for broadcast at a future time.
 */
function cf_sms_optin_feature_info($message, $from_num) { // the 'data' param is ignored

	// include details of last alert (will need this for the admin page)
	//$data = db_query("SELECT id, message, timestamp, from_num FROM {cf_sms_sends} WHERE feature_id = '%s' ORDER BY timestamp DESC");
	//$record = db_fetch_array($data); // first record should be the latest

	// get the info for the send
	$signups = _get_signups_for_sms_feature('info');
	$num_array = _array_subvalues($signups, 'mobile_num');
	// do the send
	$timestamp = time();
	$xml = cfSendSMS($num_array, $message, $from_num);
	// extract the ID for future reference
	$send_id = (string)$xml->children(CALLFIRE_RESOURCE_NAMESPACE)->Id;
	// create and store a record of the send
	db_query("INSERT INTO {cf_sms_sends} (id, message, timestamp, feature_id, from_num) VALUES (%d, '%s', %d, '%s', %d)", $send_id, $message, $timestamp, 'info', $from_num);
	// record all the users, and their numbers, we sent to
	foreach ($signups as $signup) {
		db_query("INSERT INTO {cf_sms_send_numbers} (send_id, user_id, number) VALUES (%d, %d, '%s')", $send_id, $signup['id'], $signup['mobile_num']);
	}
	// record the last public send for use by other features
	variable_set('cf_sms_last_public_sms_message', $message);
	variable_set('cf_sms_last_public_sms_time', $timestamp);

}


/**
 * Implements hook_checkout_pane for Ubercart; allows user opt-in of SMS features during checkout.
 */
function cf_sms_checkout_pane() {

  $panes[] = array(
    'id' => 'cf_sms',
    'title' => t('SMS notifications opt-in'),
    'desc' => t('Allow the user to opt-in to various SMS features.'),
    'callback' => 'uc_checkout_pane_cf_sms',
    'weight' => 7,
  );

  return $panes;

}


/**
 * Interacts with Ubercart's checkout system to allow a customer to opt-in
 * to various CallFire SMS features during checkout.
 */
function uc_checkout_pane_cf_sms($op, &$arg1, $arg2) {

	global $user;

	switch ($op) {

		case 'view':

			$contents = array();

			// get the user ID
			$user_id = $user->uid;

			// if no user ID, we need to keep track of what they sign up for;
			// store a negative timestamp for now, replace with order # later
			if ($user_id <= 0) {
				$user_id = -time();
			}

			// if SMS is in debug mode, but this isn't an admin, skip out
			if (variable_get('cf_sms_debug_mode', 0) && !user_access('access administration menu')) {
				return $contents;
			}

			// load the user's features & phone num; if none, give blank values
			$data = db_fetch_array(db_query("SELECT mobile_num, features FROM {cf_sms_users} WHERE id = %d", $user_id));
			if ($data === FALSE) {
				$mobile_num = '';
				$user_features = '';
			}
			else {
				$record = $data;
				$user_features = $record['features'];
				$mobile_num = $record['mobile_num'];
			}

			// start adding the form elements

 			$contents['cf_sms_user_passthru'] = array(
 				'#type' => 'hidden',
 				'#value' => $user_id
 			);

    	$contents['cf_sms_optin_mobile_num'] = array(
				'#type' => 'textfield',
				'#title' => t('Mobile number for texting'),
				'#default_value' => _cf_sms_display_mobile_num($mobile_num),
			);
    
			// loop through all SMS opt-in features
	    $all_features = _get_all_cf_sms_optin_features();
   		foreach ($all_features as $feature) {

				// if the feature is enabled, add it to the form
				$feature_id = $feature['id'];
  		  $feature_id_enabled = 'cf_sms_optin_feature_' . $feature_id . '_enabled';
				if (variable_get($feature_id_enabled, 0)) {
	  		  $tagged_feature_id = ':' . $feature_id . ';';
	  		  $feature_id_signedup = 'cf_sms_optin_feature_' . $feature_id . '_signedup';
					$pos = strpos($user_features, $tagged_feature_id);
					$contents[$feature_id_signedup] = array(
						'#type' => 'checkbox',
						'#title' => '<b>' . $feature['name'] . ':</b> ' . $feature['text'],
						'#default_value' => !($pos === FALSE),
					);
     		}
      		
   		}
   		
   		$description = variable_get('cf_sms_signup_message', CF_SMS_SIGNUP_MESSAGE);
   		
			$pane = array('description' => $description, 'contents' => $contents);

			return $pane;

	} // end switch based on $op

}


/**
 * Get array of only phone numbers (as strings) that are signed up for an SMS opt-in feature
 */
function _get_numbers_for_sms_feature($feature_id) {

	return _array_subvalues(_get_signups_for_sms_feature($feature_id), 'mobile_num');

}
