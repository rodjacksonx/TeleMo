<?php

/**
 * @file
 * Contains the functions for the CallFire SMS opt-in feature 'ship'
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */
 

define('CALLFIRE_RESOURCE_NAMESPACE', "http://api.callfire.com/resource");


/**
 * Implements the 'ship' opt-in feature: the user receives an SMS when his order has been 'shipped'.
 */
function cf_sms_optin_feature_ship($message, $from_num, $data) {

	// extract the info for the send
	$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
	$mobile_num = substr($parts[1], 6); // skip 'phone=' and store '###' into $mobile_num
	$user_id = substr($parts[2], 5); // skip 'user=' and store '###' into $user_id
	// do the send
	$timestamp = time();
	$xml = cfSendSMS($mobile_num, $message, $from_num);
	// extract the ID for future reference
	$send_id = (string)$xml->children(CALLFIRE_RESOURCE_NAMESPACE)->Id;
	// create and store a record of the send
	db_query("INSERT INTO {cf_sms_sends} (id, message, timestamp, feature_id, from_num, data) VALUES (%d, '%s', %d, 'ship', %d, '%s')", $send_id, $message, $timestamp, $from_num, $data);
	// record the user and the number we sent to
	db_query("INSERT INTO {cf_sms_send_numbers} (send_id, user_id, number) VALUES (%d, %d, '%s')", $send_id, $user_id, $mobile_num);
	// update the user's last SMS timestamp
	db_query("UPDATE {cf_sms_users} SET last_sms = %d WHERE id = %d", $timestamp, $user_id);

}


/**
 * Checks to see if the 'ship' opt-in feature needs to send an SMS; also executes all pending SMS sends
 */
function cf_sms_optin_feature_ship_check($form, &$form_state) {

	// make sure this feature is enabled before doing anything
	$continue = TRUE;
	if (!variable_get('cf_sms_optin_feature_ship_enabled', FALSE)) {
		$continue = FALSE;
	}

	if ($continue) {
		// extract the data needed to perform the check
		$order_id = $form_state['values']['order_id'];
		$status_id = $form_state['values']['status'];

		// fetch the rest of the data needed from the database
		$record = db_fetch_array(db_query("SELECT uid, order_total, delivery_first_name FROM {uc_orders} WHERE order_id = %d", $order_id));
		$order_total = uc_price($record['order_total'], array('revision' => 'formatted'));
		$user_id = $record['uid'];
		$user_name = $record['delivery_first_name'];
		if ($user_name == '') {$user_name = 'CUSTOMER';}
		$record = db_fetch_array(db_query("SELECT mobile_num, features FROM {cf_sms_users} WHERE id = %d", $user_id));
		if ($record === FALSE) {
			$old_user_id = -$order_id;
			db_query("UPDATE {cf_sms_users} SET id = %d WHERE id = %d", $user_id, $old_user_id);
			$record = db_fetch_array(db_query("SELECT mobile_num, features FROM {cf_sms_users} WHERE id = %d", $user_id));
		}
		$user_features = $record['features'];
		$mobile_num = $record['mobile_num'];

		// make sure the user is signed up for this feature
		$tagged_feature_id = ':ship;'; // this is how features are stored in the field
		$pos = strpos($user_features, $tagged_feature_id);
		if ($pos === FALSE) {
			$continue = FALSE;
		}

		// make sure the status is set or being set to 'shipped';
		// look for 'shipped' in the status title, or just a status
		// 'completed' (in the 'completed' state)
		$status_title = uc_order_status_data($status_id, 'title');
		$pos = strpos(strtolower($status_title), 'shipped');
		if (($status_title != 'completed') && ($pos === FALSE)) {
			$continue = FALSE;
		}

	}

	// if we're still continuing, then the feature is enabled, the user is
	// signed up for it, and the status is set to 'Shipped'. But....
	if ($continue) {
		// Has an SMS for this order's shipping been sent
		// aready? Check records up to a week old.
		$t = time();
		$t_minus_one_week = $t - 604800;
		$db_data = db_query("SELECT data FROM {cf_sms_sends} WHERE feature_id = 'ship' AND timestamp > %d", $t_minus_one_week);
		$found = FALSE;
		while ($data = db_result($db_data)) {
			$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
			$num = substr($parts[0], 6); // skip 'order=' and store '###' into $num
			if ($num == $order_id) {
				$found = TRUE;
				break;
			}
		}
		// if we found a prior send for the order, don't continue
		if ($found) {
			$continue = FALSE;
		}
		else {
			// check if a send for this order is pending (may not be immediate sends)
			$db_data = db_query("SELECT data FROM {cf_sms_future_sends} WHERE feature_id = 'ship'");
			while ($data = db_result($db_data)) {
				$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
				$num = substr($parts[0], 6); // skip 'order=' and store '###' into $num
				if ($num == $order_id) {
					$found = TRUE;
					break;
				}
			}
			if ($found) {
				$continue = FALSE;
			}
		}
	}

	// still continuing? Then we passed all checks; do the send
	if ($continue) {
		// setup a future SMS send, for right now, non-recurring
		$t = time();
		// get info for carrier and tracking number
		$shipment = db_fetch_array(db_query("SELECT carrier, tracking_number FROM {uc_shipments} WHERE order_id = %d", $order_id));
		$carrier = $shipment['carrier'];
		$tracking_num = $shipment['tracking_number'];
		// find and replace the variables in the message
		$message = variable_get('cf_sms_optin_feature_ship_message', CF_SMS_OPTIN_DEFAULT_SHIP_MESSAGE);
		$message = str_replace('!name', $user_name, $message);
		$message = str_replace('!order', $order_id, $message);
		$message = str_replace('!total', $order_total, $message);
		$message = str_replace('!org', variable_get('cf_sms_org_name', ''), $message);
		$message = str_replace('!carrier', $carrier, $message);
		$message = str_replace('!track', $tracking_num, $message);
		// for the 'ship' feature we need to bundle the order ID,
		// the mobile number and the user ID to send to
		$data = 'order=' . $order_id . ':phone=' . $mobile_num . ':user=' . $user_id;
		// get the default "from" phone number
		$from = variable_get('cf_sms_optin_feature_ship_from_num', variable_get('cf_sms_default_from_num', variable_get('callfire_default_from_num','')));
		$from = _cf_sms_extract_mobile_num($from);
		// store the future send
		db_query("INSERT INTO {cf_sms_future_sends} (send_time, from_num, recurrence, feature_id, message, data) VALUES (%d, '%s', 'none', 'ship', '%s', '%s')", $t, $from, $message, $data);
	}

	// execute all pending sends (including this one, if created)
	cf_sms_execute_next_scheduled_sends();

} // end function - cf_sms_optin_feature_ship_check
