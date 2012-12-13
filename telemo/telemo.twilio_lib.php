<?php

//define('TWILIO_API_PREFIX', "https://api.twilio.com/2010-04-01/Accounts/");
//define('TWILIO_API_SMS_URI_PREFIX', "/SMS/Messages/");

require_once("twilio-php/Services/Twilio.php");


/**
 * twSendSMS - this function uses Twilio's API to broadcast an SMS
 *   text message to one or more phone numbers, returning a two-
 *   element array. The first element is an array of Send IDs of all
 *   the sends that did not fail, the second element has a key of
 *   'error', and is set to either FALSE (indicating no errors) or
 *   to TRUE (indicating that an error occurred, and their were
 *   probably sends that failed.) Also, the key of the first major
 *   element will be 'ids'.
 *
 * @param - $to: either a single phone number to send to (in e.164
 *   international format), given as a string of only digits, or an
 *   array of such numbers, each being a string of only digits.
 *
 * @param - $from: a string phone number,  in e.164 international
 *   format; optionally, can be a short code, but in either case,
 *   the number must be registered (purchased) and on record with
 *   Twilio
 *
 * @param - $message: the actual text of the message, as a string,
 *   with the usual 160-character SMS cap.
 *
 */
function telemo_twSendSMS($to, $from_phone_num, $message) {

	$client = new Services_Twilio(_get_telemo_twilio_account_sid(), _get_telemo_twilio_auth_token());
	
	$sms_error = FALSE;
	$send_ids = array();
	$error_data = array();
	
	if (!is_array($to)) {
		$to = array($to);
	}
	
	foreach ($to as $key => $to_phone_num) {
		$response = $client->account->sms_messages->create($from_phone_num, $to_phone_num, $message);
		if (($response === FALSE) || ($response->status == 'failed')) {
			$sms_error = TRUE;
			$error_data = array('error_key' => $key, 'error_num' => $to_phone_num);
			break;
		}
		$send_ids[] = $response->sid;
	}
	
	$ret_array = array('ids' => $send_ids, 'error' => $sms_error, 'error_data' => $error_data);

	return $ret_array;
}


/**
 * Returns the app login to access CallFire's API
 */
function _get_telemo_twilio_account_sid() {
  return variable_get('telemo_twilio_account_sid', '');
}

/**
 * Returns the app password to access CallFire's API
 */
function _get_telemo_twilio_auth_token() {
  return variable_get('telemo_twilio_auth_token', '');
}
