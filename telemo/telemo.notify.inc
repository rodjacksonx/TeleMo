<?php

/**
 * @file
 * TeleMo remote postback/notification functions.
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */


/**
 * Initial entry point function for TeleMo's CallFire notification URL
 */
function telemo_callfire_notification() {

	$post = file_get_contents("php://input");
	if ($post) {
		$xml = simplexml_load_string($post);
		$results = telemo_notification($xml, 'callfire');
	}

}


/**
 * Initial entry point function for TeleMo's Twilio notification URL
 */
function telemo_twilio_notification() {

	$post = file_get_contents("php://input");
	if ($post) {
		$xml = simplexml_load_string($post);
		$results = telemo_notification($xml, 'twilio');
	}

}


/**
 * Point where individual notification functions for each potential
 * service and feature are called; they must decide if the callback
 * is one for them to act on or not.
 */
function telemo_notification($xml, $service) {

	// convert the input to a standardized internal format
	// (will do this when we know more about incoming posts)
	//$telemo_notify = array();
		
	// let each service scrutinize the XML to see if it is for them
	$results = module_invoke_all('telemo_notify', $service, $xml);

	return $results;
}


/**
 * Sends out an email alert from the TeleMo module
 */
function telemo_mail($to, $txt, $priority = 3) {

	$success = false;
	
	if ($to != '') {
	  // prepare to construct the message array we need
		$m = array();
    // include the preface to the body of the email
  	$preface = t("ATTENTION: This is an alert from the TeleMo module of your website.") . "\r\n\r\n";
    // establish the body of the email
		$m['body'] = $preface . $txt;
    // get the 'From' address
		$from = _get_telemo_email_from_address();
	  // setup the other email elements
		$m['id'] = 'telemo_alert';
		$m['subject'] = t('TeleMo Alert');
		$m['to'] = $to;
		$m['from'] = $from;
		// finalize the email headers
		$headers = array();
		$headers['To'] = $to;
		$headers['From'] = $from;
		$headers['Sender'] = $from;
		$headers['X-Priority'] = $priority;
		$m['headers'] = $headers;
  	// actually send the email
    $success = drupal_mail_send($m);
  }
	
	return $success;
}


/**
 * Returns the string of the 'From' email address to be used when
 * sending out email alerts from the module. UNFINISHED
 */
function _get_telemo_email_from_address() {

  return variable_get('site_mail', ini_get('sendmail_from'));
}
