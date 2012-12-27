<?php


/**
 * tmSendSMS - this function broadcasts an SMS text message to one or
 *   more phone numbers, using any (or all) of the enabled services
 *   that TeleMo allows (currently Twilio and CallFire.) The returned
 *   response will be an associative array, one element per available
 *   service ('twilio', 'callfire'). Each element will either be an
 *   array of values (one send ID per text sent) or an error string.
 *
 * @param - $phone_nums: this is one or more phone numbers, either as
 *   a numeric string for one number or as an array of numeric strings
 *   for multiple numbers. All phone numbers should include no extra
 *   symbols, e.g.: '18005559999'
 *
 * @param - $from: a string phone number; may be left as an empty
 *   string, but with some systems (like Twilio) including the 'from'
 *   number is mandatory, and not including it here will result in
 *   the default 'from' number being used. You may also use '0' to
 *   force the default 'from' number on any system, such as on
 *   CallFire where a blank 'from' number is acceptable.
 *
 * @param - $message: the actual text of the message, as a string
 *
 */
function tmSendSMS($to, $from, $message) {

  // if 'to' is not an array, turn into a single-element array;
  // if it is, make sure it's not an associative array
  if (!is_array($to)) {
    $to = array($to);
  }
  else {
  	$to = array_values($to);
  }
  
  // verify all 'to' numbers are at least 11 digits (must include the leading "1");
  // kill any over 11 digits (apparently Twilio doesn't like international numbers)
  foreach ($to as $key => $to_num) {
  	if (strlen($to_num) == 10) {
  		$to[$key] = '1' . $to_num;
  	}
		elseif (strlen($to_num) > 11) {
			unset($to[$key]);
		}
  }
  $to = array_values($to);
  
  // verify 'from' number is not simply 10 digits
  if (strlen($from) == 10) {
  	$from = '1' . $from;
  }	

	// prepare the returned results
	$results = array();
	  
  // if enabled, send via CallFire's systems
  if (variable_get('telemo_callfire_enabled', FALSE)) {
  	if ($from == '0') {
  		$from = _telemo_extract_mobile_num(variable_get('telemo_callfire_default_from_num', ''));
  	}
  	if (strlen($from) == 10) {
  		$from = '1' . $from;
  	}
  	$ret = telemo_cfSendSMS($to, $message, $from);
  	if ($ret['error']) {
  		$num_sent = $ret['error_data']['error_key'];
  		$total_nums = count($to);
  		$result = 'A CallFire error occurred during an SMS send; ' . $num_set . ' out of ' . $total_nums . ' sends were successful.';
  		$result .= ':IDs:';
  		$result_ids = '';
  		foreach ($ret['ids'] as $id) {
  			$result_ids .= $id . ',';
  		}
  		if (strlen($result_ids) > 0) {
  			$result_ids = substr($result_ids, 0, strlen($result_ids) - 1);
  		}
  		$result .= $result_ids;
  	}
  	else {
  		$result = $ret['ids'];
  	}
  	$results['callfire'] = $result;
  }
  
  // if enabled, send via Twilio's systems
  if (variable_get('telemo_twilio_enabled', FALSE)) {
  	if (($from == '0') || ($from == '')) {
  		$from = _telemo_extract_mobile_num(variable_get('telemo_twilio_default_from_num', ''));
  	}
  	if (strlen($from) == 10) {
  		$from = '1' . $from;
  	}
  	$ret = telemo_twSendSMS($to, $from, $message);
  	if ($ret['error']) {
  		$num_sent = $ret['error_data']['error_key'];
  		$total_nums = count($to);
  		$result = 'A Twilio error occurred during an SMS send; ' . $num_set . ' out of ' . $total_nums . ' sends were successful.';
  		$result .= ':IDs:';
  		$result_ids = '';
  		foreach ($ret['ids'] as $id) {
  			$result_ids .= $id . ',';
  		}
  		if (strlen($result_ids) > 0) {
  			$result_ids = substr($result_ids, 0, strlen($result_ids) - 1);
  		}
  		$result .= $result_ids;
  	}
  	else {
  		$result = $ret['ids'];
  	}
  	$results['twilio'] = $result;
  }

  // return the results
  return $results;
  
} // end function - tmSendSMS


/**
 * tmAddVoiceFile - this function loads, uploads and adds a .wav or .mp3
 *   voice file to the CallFire system for future use in voice broadcasts.
 *   Returns the voice ID of the uploaded file or an error string
 *
 * @param - $filename: the filename of the file, as stored on the website
 *   server using Drupal's code, to upload to CallFire's system. All such
 *   files are typically stored ???,
 *   but any file can be specified.
 *
 * @param - $title: the title of the file as we wish it to be noted within
 *   CallFire's systems, available to us when recalling all our files.
 *
 */
/*function tmAddVoiceFile($filename, $title = '') {

  $client = _new_telemo_callfire_soap_client;
    
  // read the file into $voice_data as a string
  // UNFINISHED: this portion is probably handled
  // differently under Drupal's methods
  $filenum = fopen($filename, 'r');
  $voice_data = fread($filenum, filesize($filename));

  // add code here to automatically search through all
  // voice files we have stored on CallFire and choose
  // the next number to use in a generic number-title
  // scheme if they didn't give us a title
  if ($title === '') {
    // currently, do nothing, just have no title
  }
  
  // define the request we'll need for the client
  $create_voice_request = array(
    'Name' => $title,
    'Data' => $voice_data);
    
  // use the SOAP client to actually upload the file and
  // store in it CallFire's system for us
  $voice_id = $client->createSound($create_voice_request);

  return $voice_id;

} // end function - cfAddVoiceFile
*/
//echo "New sound ID: $soundId\n";


/**
 * tmVoiceBroadcast - this function takes a voice file previously uploaded
 *   to CallFire's system and broadcasts to one or more phones, making an
 *   individual call per phone and playing the voice file for the recipient.
 *   Returns the broadcast ID for the broadcast, or an error string.
 *
 * @param - $phone_nums: either a string with one or more (comma-delimited)
 *   phone numbers, or an array of strings, each being a single phone number.
 *   UNFINISHED: the example given mentioned an array of numbers, but not a
 *   multi-number string. CHECK TO MAKE SURE A MULTI-NUM STRING WORKS; IF NOT,
 *   CONSIDER CONVERTING ALL FUNCTIONS TO ONLY ACCEPT ARRAYS FOR MULTIPLE
 *   PHONE NUMBERS.
 *
 * @param - $caller_id_num: this is the number that will be displayed on
 *   caller IDs when each phone number is called. This must be verified with
 *   CallFire before you can use it.
 *
 * @param - $title: the textual title of the voice broadcast
 *
 * @param - $config: the parameter for 'AnsweringMachineConfig'. Currently
 *   'LIVE_IMMEDIATE' is the only known option
 *
 * @param - $voice_id: the voice ID of the previously uploaded voice file
 *
 */
/*function tmVoiceBroadcast($phone_nums, $caller_id_num, $title, $config, $voice_id) {

  // create a WSDL SOAP client to process the request;
  // UNFINISHED: this may be required to be the same
  // actual client object that was previously created
  // with cfAddVoiceFile. If so, recode accordingly
  $client = _new_telemo_callfire_soap_client();

  // define the request we'll need for the client
  $send_call_request = array(
    'ToNumber' => $phone_nums,
    'VoiceBroadcastConfig' => array(
      'FromNumber'             => $caller_id_num,
      'AnsweringMachineConfig' => $config,
      'BroadcastName'          => $title,
      'LiveSoundId'            => $voice_id));

  // use the SOAP client to actually initiate the voice broadcast
  $broadcast_id = $client->sendCall($send_call_request);

  return $broadcast_id;
  
} // end function - cfVoiceBroadcast
*/

/**
 * tmGetBroadcastInfo - Gets information about a previously made voice
 *   broadcast or, optionally, info about the individual phone calls in
 *   the broadcast. Returns the info, presumably (UNFINISHED) as an XML object
 *
 * @param - $id: the ID of the previously made voice broadcast
 *
 * @param - $call_info: if set to TRUE, forces the return of information
 *   about the individual phone calls rather than about the broadcast itself
 *
 */
/*function tmGetBroadcastInfo($id, $call_info = false) {

  $client = _new_telemo_callfire_soap_client();

  // if the call info flag is set, get the info on the individual
  // phone calls rather than the info about the general broadcast
  if (!$call_info) {
    $info = $client->getBroadcast(array('Id'=>$id));
  }
  else {
    $info = $client->queryCalls(array('BroadcastId'=>$id));
  }

  return $info;
}
*/
