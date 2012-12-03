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
function callfire_notification() {

	$post = file_get_contents("php://input");
	if ($post) {
		$xml = simplexml_load_string($post);
		
		// convert the input to a standardized internal format
		$cf_notify = array();
		
		// let each service scrutinize the results to see if it is for them
		$results = module_invoke_all('telemo_notify', $cf_notify);
	}

}


/**
 * Initial entry point function for TeleMo's Twilio notification URL
 */
function twilio_notification() {

	$post = file_get_contents("php://input");
	if ($post) {
		$xml = simplexml_load_string($post);
		
		// convert the input to a standardized internal format
		$tw_notify = array();
		
		// let each service scrutinize the XML to see if it is for them
		$results = module_invoke_all('telemo_notify', $tw_notify);
	}

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
		$m['subject'] = t('TeleMo Notification');
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
