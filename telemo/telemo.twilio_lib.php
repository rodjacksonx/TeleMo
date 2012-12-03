<?php

define('CALLFIRE_SMS_SEND_URL', "https://www.callfire.com/api/rest/text/send");

define('CALLFIRE_WSDL_SOAP_URL', "https://callfire.com/api/1.0/wsdl/callfire-service-http-soap12.wsdl");

define('CALLFIRE_RESOURCE_NAMESPACE', "http://api.callfire.com/resource");

define('CALLFIRE_GET_STATS_URL', "https://www.callfire.com/api/rest/broadcast/");


/**
 * cfPost - this function is a generic accessor for CallFire's API; it
 *   gives their system data we want it to have, returning a result
 */
function cfPost($url, $user, $password, $params = array(),
                $contentType = 'application/x-www-form-urlencoded') {

	$query = http_build_query($params, '', '&');
	$authentication = 'Authorization: Basic '.base64_encode("$user:$password");
	$http = curl_init($url);
	curl_setopt($http, CURLOPT_SSL_VERIFYHOST, false);
	curl_setopt($http, CURLOPT_SSL_VERIFYPEER, false);
	curl_setopt($http, CURLOPT_RETURNTRANSFER, true);
	curl_setopt($http, CURLOPT_URL, $url);
	curl_setopt($http, CURLOPT_POST, true);
	curl_setopt($http, CURLOPT_POSTFIELDS, $query);
	curl_setopt($http, CURLOPT_HTTPHEADER, array(
	  "Content-Type: $contentType",
	  $authentication));
	return curl_exec($http);  
}


/**
 * cfGet - this function is a generic accessor for CallFire's API; it
 *   allows the retrieval of user-specific data from their system
 */
function cfGet($url, $user, $password, $params = array(), $contentType = 'plain/text') {
	$query = http_build_query($params, '', '&');
	$authentication = 'Authorization: Basic '.base64_encode("$user:$password");
	$http = curl_init($url);
	curl_setopt($http, CURLOPT_SSL_VERIFYHOST, false);
	curl_setopt($http, CURLOPT_SSL_VERIFYPEER, false);
	curl_setopt($http, CURLOPT_RETURNTRANSFER, true);
	curl_setopt($http, CURLOPT_URL, $url);
	curl_setopt($http, CURLOPT_HTTPHEADER, array(
	  "Content-Type: $contentType",
	  $authentication));
	return curl_exec($http);  
}
	
	
/**
 * cfSendSMS - this function uses CallFire's API to broadcast an SMS
 *   text message to one or more phone numbers, returning a response
 *   that is either an xml object or an error string
 *
 * @param - $phone_nums: this is one or more phone numbers, either as
 *   a string (comma-delimited if multiple numbers) or as an array,
 *   each element being a single phone number string. All phone numbers
 *   should include no extra symbols, e.g.: '18005559999'
 *
 * @param - $message: the actual text of the message, as a string
 *
 * @param - $from: a string phone number; if the message is meant to be
 *   shown as coming from a 10-digit long code rather than CallFire's
 *   short code, you can provide it optionally
 *
 */
function cfSendSMS($phone_nums, $message, $from = false) {

  // if given as an array, concat the numbers into a single string
  if (is_array($phone_nums)) {
    $phone_num_str = '';
    foreach ($phone_nums as $phone_num) {
      $phone_num_str .= $phone_num . ',';
    }
    $phone_num_str = substr($phone_num_str, 0, strlen($phone_num_str) - 1);
  }
  else {
    $phone_num_str = $phone_nums;
  }
  
  // organize the details
  $details = array();
  $details['to'] = $phone_num_str;
  $details['message'] = $message;
  
  // determine whether to include the $from parameter
  if ($from !== false) {
    $details['from'] = $from;
  }
  
  // use cfPost to do the actual send, getting a response
  $resp = cfPost(CALLFIRE_SMS_SEND_URL, _get_cf_login(), _get_cf_password(), $details);

  // convert the response into an xml object
  $xml = simplexml_load_string($resp);
  //$id = (string)$xml->children(CALLFIRE_RESOURCE_NAMESPACE)->Id;
  
  // return the results
  return $xml;
  
} // end function - cfSendSMS


/**
 * cfGetSendStats - this function gets the stats of a previous CallFire
 *   broadcast (specifically a SMS send). Returns an XML object or error
 *   string as a response
 *
 * @param - $id: the broadcast ID of the previous send
 *
 */
function cfGetSendStats($id) {

  $get_url = CALLFIRE_GET_STATS_URL . $id . '/stats';
   
  $resp = cfGet($get_url, _get_cf_login(), _get_cf_password());
  
  $xml = simplexml_load_string($resp);
  
  return $xml;
}

/*
string(735) '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<r:resource xmlns="http://api.callfire.com/data" xmlns:n="http://api.callfire.com/notification/xsd" xmlns:r="http://api.callfire.com/resource" xmlns:s="http://api.callfire.com/service/xsd">
    <r:note>Processing time was 38.7ms</r:note>
    <broadcaststats>
        <usagestats>
            <duration>0</duration>
            <billedduration>0</billedduration>
            <billedamount>0.03</billedamount>
            <attempts>1</attempts>
            <actions>1</actions>
        </usagestats>
        <resultstat>
            <result>SENT</result>
            <attempts>1</attempts>
            <actions>1</actions>
        </resultstat>
    </broadcaststats>
</r:resource>'
*/

/**
 * cfAddVoiceFile - this function loads, uploads and adds a .wav or .mp3
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
function cfAddVoiceFile($filename, $title = '') {

  $client = _new_cf_soap_client;
    
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

//echo "New sound ID: $soundId\n";


/**
 * cfVoiceBroadcast - this function takes a voice file previously uploaded
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
function cfVoiceBroadcast($phone_nums, $caller_id_num, $title, $config, $voice_id) {

  // create a WSDL SOAP client to process the request;
  // UNFINISHED: this may be required to be the same
  // actual client object that was previously created
  // with cfAddVoiceFile. If so, recode accordingly
  $client = _new_cf_soap_client();

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


/**
 * cfGetBroadcastInfo - Gets information about a previously made voice
 *   broadcast or, optionally, info about the individual phone calls in
 *   the broadcast. Returns the info, presumably (UNFINISHED) as an XML object
 *
 * @param - $id: the ID of the previously made voice broadcast
 *
 * @param - $call_info: if set to TRUE, forces the return of information
 *   about the individual phone calls rather than about the broadcast itself
 *
 */
function cfGetBroadcastInfo($id, $call_info = false) {

  $client = _new_cf_soap_client();

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


/**
 * cfPostbackSubscribe - subscribes a website URL (one capable of receiving
 *   and responding to HTTPS POST requests), so that CallFire can make use
 *   of it for 'push' notifications.
 *
 * @param - $url: the HTTPS URL of your website that will be
 *   responding to the postbacks
 *
 */
function cfPostbackSubscribe($url) {

  $client = _new_cf_soap_client('SOAP_1_2');

  $subscription_request = array('Subscription' => array(
    'Endpoint' => $url,
    'NotificationFormat' => 'XML'));

  $response = $client->createSubscription($subscription_request);

  return $response;
}

/*
echo "Response: $response\n";


-----------------------------------------------------------
//Here’s an example of a postback that was generated when I received an incoming SMS on one of my phone numbers that I own through CallFire:

xml
//Post-back example from incoming SMS
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<n:textnotification xmlns="http://api.callfire.com/data" xmlns:n="http://api.callfire.com/notification/xsd">
    <n:subscriptionid>19001</n:subscriptionid>
    <text id="5707328001">
        <fromnumber>18185551212</fromnumber>
        <tonumber>18186469991</tonumber>
        <state>FINISHED</state>
        <contactid>1418291001</contactid>
        <inbound>true</inbound>
        <created>2012-09-24T05:02:35Z</created>
        <modified>2012-09-24T05:02:35Z</modified>
        <finalresult>RECEIVED</finalresult>
        <message>Hello, World!</message>
        <textrecord id="3526268001">
            <result>RECEIVED</result>
            <finishtime>2012-09-24T05:02:35Z</finishtime>
            <billedamount>0.03</billedamount>
            <message>Hello, World!</message>
        </textrecord>
    </text>
</n:textnotification>

------------
//I want to receive all the SMS that come to my CallFire numbers as emails, so that I can read them on my desktop computer as they come in. Converting the SMS postbacks to emails can be done with just a few lines of code in PHP:

<?php
// This code will reside at the endpoint we defined earlier...
// http://www.yourwebsite.com/somephpfile.php
$xml = simplexml_load_string($postBody);
$text = $xml->Text or die('No text');
$inbound = $text->Inbound == 'true' or die('Not inbound');
$fromNumber = (string)$text->FromNumber;
$toNumber   = (string)$text->ToNumber;
$time       = (string)$text->Created;
$message    = (string)$text->Message;
$emailSubject = "$toNumber received a new SMS from $fromNumber";
$emailMessage = "From: $fromNumber\n".
                "  To: $toNumber\n".
                "Time: $time\n".
                "Text: $message";
mail('whatsyourname@yourwebsite.com', $emailSubject, $emailMessage);
/?>
-----------------------------------------------------------
*/


/**
 * cfPostbackUnsubscribe - given a subscription ID previously generated
 *   for a postback, unsubscribes that particular postback.
 *
 */
function cfPostbackUnsubscribe($id) {

  $client = _new_cf_soap_client('SOAP_1_2');
  
  $response = $client->querySubscriptions();

  if (is_array($response->Subscription)) {
    $subscriptions = $response->Subscription;
  }
  else {
    $subscriptions = array($response->Subscription);
  }

  // construct the request for the unsubscription
  $delete_request = array('Id' => $id);
  
  $response = $client->deleteSubscription($delete_request);
  
  return $response;

}

/*
-----------------------------------------------------------
// Let's assume we have $client defined from the first example:
//query CallFire for our account's "postbacks",
// then delete each one
foreach($subscriptions as $sub) {
    $id = $sub->id;
    $deleteRequest = array('Id' => $id);
    echo "Deleting $id...\n";
    $response = $client->deleteSubscription($deleteRequest);
}
-----------------------------------------------------------
*/


/**
 * cfGetSubscriptions - queries CallFire for all the postback subscriptions
 *   the given login/password has with them, returning either an array of
 *   objects if one or more subscriptions, or a boolean FALSE if none.
 *
 */
function cfGetSubscriptions() {

  $client = _new_cf_soap_client('SOAP_1_2');

  $response = $client->querySubscriptions();

	if ($response === FALSE) {
		$ret = FALSE;
	}
	elseif (is_array($response->Subscription)) {
		$ret = $response->Subscription;
	}
	else {
		$ret = array($response->Subscription);
	}
	
  return $ret;
}


/**
 * Creates an new instance of a CallFire SOAP client
 * for interacting with their system and handling requests
 */
function _new_cf_soap_client($version = '') {

  $client_params = array(
    'login'        => _get_cf_login(),    
    'password'     => _get_cf_password());

  if ($version !== '') {
    $client_params['soap_version'] = $version;
  }
  
  $client = new SoapClient(CALLFIRE_WSDL_SOAP_URL, $client_params);
  
  return $client;

}

/**
 * Returns the app login to access CallFire's API
 */
function _get_cf_login() {
  return variable_get('callfire_login', '');
}

/**
 * Returns the app password to access CallFire's API
 */
function _get_cf_password() {
  return variable_get('callfire_password', '');
}
