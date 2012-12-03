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
 


/**
 * Implementation of hook_user().
 *
 * Enables the 'signup features' that are available in some TeleMo extending modules.
 * This particular function allows the user to view and edit their participation in
 * this features on their user account page.
 *
 */
function telemo_user($type, $edit, &$user, $category = NULL) {

	// if TeleMo is only debug-mode enabled, and this isn't an admin, skip out
	if (variable_get('telemo_debug_mode', FALSE) && !user_access('access administration menu')) {
		return;
	}

	// get user ID
	$user_id = $user->uid;
	if ($user_id <= 0) {
		return;
	}

	// if this isn't someone looking at their own profile, or an admin, skip out
  if (!($GLOBALS['user']->uid == $user_id) && !user_access('administer users')) {
  	return;
  }
	
	switch ($type) {
	
		case 'form':
		
			$data = db_fetch_array(db_query("SELECT mobile_num, signups FROM {telemo_users} WHERE id = %d", $user_id));
			if ($data === FALSE) {
				$mobile_num = '';
				$user_signups = '';
				db_query("INSERT INTO {telemo_users} (id, mobile_num, signups, creation, groupings, last_action_time, last_action_event) VALUES (%d, '', '', %d, '', 0, '')", $user_id, time());
			}
			else {
				$record = $data;
				$user_signups = $record['signups'];
				$mobile_num = $record['mobile_num'];
			}
			
			$enabled_services = _telemo_all_enabled_services();
			
  		if ($category == 'account') {
  		
  			$form['telemo_user_id_passthru'] = array( // CAREFUL!!!!! changed to '_user_id_', from '_user_'!
  				'#type' => 'hidden',
  				'#value' => $user_id
  			);

    		$form['telemo_mobile_num'] = array(
					'#type' => 'textfield',
					'#title' => t('Mobile number'),
					'#default_value' => _cf_sms_display_mobile_num($mobile_num),
					'#summary' => t('Mobile number for receiving TeleMo services: ') . $mobile_num,
					'#description' => t("Number where text messages and other telephony services should be directed."),
				);
    
				// loop through all enabled services
				foreach ($enabled_services as $service) {
				
					// add a fieldset on the form for the opt-in features of this service
					$service_id = $service['id'];
					$fieldset_name = 'tm_' . $service_id . '_user_signups';
		  		$form[$fieldset_name] = array(
			      '#type' => 'fieldset',
  	  		  '#title' => $service['name'],
		  	    '#collapsible' => TRUE,
		    	  '#collapsed' => FALSE,
    			);

					$service_prompt = 'tm_' . $service_id . '_prompt';
					$form[$fieldset_name][$service_prompt] = $service['prompt'];
					
					$features = $service['features'];
					
					// loop through all features for this service
					foreach ($features as $feature) {
						// if an opt-in feature, and enabled, add a checkbox to the form to signup for it
						if (($feature['optin']) && ($feature['enabled'])) {
							$signup_name = 'tm_' . $service_id . '_feature_' . $signup['id'] . '_enabled';
							$signup_tag = ':' . $service_id . '~' . $signup['id'] . ';';
							$pos = strpos($user_signups, $signup_tag);
							$form[$fieldset_name][$signup_name] = array(
								'#type' => 'checkbox',
								'#title' => '<strong>' . $signup['name'] . ':</strong> ' . $signup['text'],
								'#default_value' => !($pos === FALSE),
							);
						}
					}
					
					// check to make sure at least one opt-in feature is
					// present; if not, remove the service from the form
					if (count($form[$fieldset_name]) <= 5) {
						unset ($form[$fieldset_name]);
					}
					
				} // end loop through all enabled services
				
				
		    return $form;
  		}
  
  		break;
  		
  	case 'view':

			// Do not show the options in the view page; since the TeleMo options
			// are not part of the user profile, it may be best to not touch this.
			break;
			
  }

}


/**
 * Alter various forms to enable SMS settings, features, etc.
 */
function telemo_form_alter(&$form, &$form_state, $form_id) {

	switch ($form_id) {
	
		// store the checked features on the user profile page into the DB
		case 'user_profile_form':
		
			$form['#submit'][] = 'telemo_user_settings_validate';
			break;
			
		// allow the user to signup during checkout if using Ubercart
		case 'uc_cart_checkout_form':
		
			$form['#submit'][] = 'telemo_user_settings_validate';
			break;
			
		// special case for the 'ship' opt-in feature, so it'll trigger on a form submit
		// (we know we will be designing this service; we know we can just do this here.)
		case 'uc_order_view_update_form':
			
			$form['#submit'][] = 'tm_sms_feature_ship_check';
			break;
  }

}


/**
 * A validation routine for the user profile form, that takes their TeleMo
 * service settings and stores their opted-in features in the DB.
 */
function telemo_user_settings_validate($form, &$form_state) {

	// if TeleMo is only debug-mode enabled, and this isn't an admin, skip out
	if (variable_get('telemo_debug_mode', FALSE) && !user_access('access administration menu')) {
		return;
	}

	// get the user ID
	// first check to see if we're entering from Ubercart's checkout process
	if (isset($form_state['values']['panes'])) {
		$form_state_values = $form_state['values']['panes']['tm_sms']; // we know about this service
	}
	else {
		$form_state_values = $form_state['values'];
	}
	// NOW get the user ID
	$user_id = $form_state_values['telemo_user_id_passthru'];
	
	// if user ID is negative, we had a new user with no real user ID;
	// we still can't get anything except the order number right now, so
	// create a record just using the negative of the order number for now
	if ($user_id <= 0) {
		$order_id = intval($_SESSION['cart_order']);
		$user_id = -$order_id;
		$signups = '';
	}
	else {
		// get the user's opt-in features list;
		// if no record found, create a new, blank one
		$signups = db_result(db_query("SELECT signups FROM {telemo_users} WHERE id = %d", $user_id));
		if ($signups === FALSE) {
			$signups = '';
		}
	}
	
	// presume that an empty mobile number (no non-zero digits)
	// indicates the user is unsubscribing from all features
	$mobile_num = _telemo_extract_mobile_num($form_state_values['telemo_mobile_num']);
	if (_mobile_num_is_empty($mobile_num)) {
		$signups = '';
		$mobile_num = '';
	}
	else {

		// loop through and check all form elements
		foreach ($form_state_values as $key => $form_element_value) {
	
			// determine if the form element is a TeleMo signup feature setting
			$terms = explode('_', $key); // looking for array('tm', $service_id, 'feature', $feature_id, 'enabled')
			if (($terms[0] == 'tm') && ($terms[2] == 'feature') && ($terms[4] == 'enabled')) {
				$is_signup = TRUE;
				$service_id = $terms[1];
				$feature_id = $terms[3];
				$feature_is_enabled = !!$form_element_value;
			}
			else {
				$is_signup = FALSE;
			}
			
			// if so, we may need to update the user's features list
			if ($is_signup) {
				// check if the user is signed up for the feature
				$tagged_signup = ':' . $service_id . '~' . $feature_id . ';';
				$pos = strpos($signups, $tagged_signup);
		
				// if the feature is checked, but the user isn't signed up, make a change
				if ($feature_is_enabled && ($pos === FALSE)) {
					$signups .= $tagged_feature_id;
				}
				// if the feature isn't checked, but the user is signed up, make a change
				elseif ((!$feature_is_enabled) && ($pos !== FALSE)) {
					$signups = str_replace($tagged_signup, '', $signups);
				}
			}
		
		} // end foreach loop through all form elements
		
	} // end check for a blank mobile number
	
	// see if they have a record yet
	$data = db_result(db_query("SELECT mobile_num FROM {telemo_users} WHERE id = %d", $user_id));
	// if they do not...
	if ($data === FALSE) {
		// then see if they include a mobile number and at least one feature. If so...
		if (($mobile_num != '') && ($signups != '')) {
			// create a new record for the user
			db_query("INSERT INTO {telemo_users} (id, mobile_num, signups, creation, groupings, last_action_time, last_action_event) VALUES (%d, '%s', '%s', %d, '', 0, '')", $user_id, $mobile_num, $features, time());
		}
		// otherwise don't bother creating a record for the user
	}
	// but if they DO have a record already...
	else {
		// just update it
		db_query("UPDATE {telemo_users} SET mobile_num = '%s', signups = '%s' WHERE id = %d", $mobile_num, $signups, $user_id);
	}
	
	// while we're here, do the next scheduled actions
	telemo_execute_next_scheduled_actions();
	
}


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
 * Let the main CallFire module know this service (SMS) is here
 */
function cf_sms_callfire_service() {

	$services = array();
	
	$services[] = array(
		'id' => 'sms',
		'name' => t('CallFire SMS'),
		'desc' => t('Enable the CallFire SMS service.'),
		'callback' => 'cf_sms_settings',
		'weight' => -10,
	);

	return $services;
}


/**
 * Identifies and handles CallFire postbacks/notifications of incoming SMS service messages
 */
function cf_sms_callfire_notification($xml) {

	$ret = NULL;
	$continue = TRUE;
	
	if (variable_get('cf_sms_enabled', 0) && $xml) {
	
		$text = $xml->Text;
		if (!$text) {
			$ret = 'Error: no text found';
			$continue = FALSE;
		}
	
		if ($continue) {
			$inbound = (((boolean)$text->Inbound) == TRUE);
			if (!$inbound) {
				$ret = 'Error: not an inbound text';
				$continue = FALSE;
			}
		}
	
		if ($continue) {
			$message = strtoupper((string)$text->Message);
			$words = _cf_sms_text_to_words($message);
			if ((!is_array($words)) || (count($words) < 1)) {
				$ret = 'Error: no usable message in text';
				$continue = FALSE;
			}
		}
	
		// at this point, we have a usable SMS, and a usable list
		// of words in the SMS. The first is always the keyword,
		// an indicator of what response feature to activate, with
		// the remaining words being parameters for the feature
		if ($continue) {
			$keyword = $words[0];
			unset($words[0]);
		
			$from = (string)$text->FromNumber;
			$to = (string)$text->ToNumber;
		
			// get all response keywords our features have
			$response_features = array_reverse(_get_all_cf_sms_response_features());
			
			foreach ($response_features as $feature) {
				// don't even bother if the feature isn't enabled
				if ($feature['enabled']) {
					// check to see if the keyword matches
					$codeword = $feature['codeword'];
					if (!is_array($codeword)) {
						$match = ($codeword == $keyword);
					}
					else {
						$match = FALSE;
						foreach ($codeword as $single_codeword) {
							if ($single_codeword == $keyword) {
								$match = TRUE;
								break;
							}
						}
					}
					// if the feature matches the keyword, execute the feature
					if ($match) {
						$funcname = 'cf_sms_response_feature_' . $feature['id'];
						$ret = call_user_func($funcname, $from, $to, $words);
						break;
					}
				} // end of check to see if this feature is enabled
			} // end of loop through all response features
		
		} // end of last $continue check for a usable SMS notification

	} // end of check if SMS service is enabled & $xml is valid
	
	// while we're here, do the next scheduled sends
	cf_sms_execute_next_scheduled_sends();

	return $ret;
}


/**
 * Returns all the features, both opt-in and response, for the CallFire SMS
 * service, so that they can be enabled or disabled from the main settings
 */
function cf_sms_feature_settings() {

	$features = array();
	
	$features['optins'] = array(
		'#type' => 'fieldset',
		'#title' => t('Opt-in feature settings'),
		'#collapsible' => TRUE,
		'#collapsed' => TRUE,
	);
	
	$features['responses'] = array(
		'#type' => 'fieldset',
		'#title' => t('Response feature settings'),
		'#collapsible' => TRUE,
		'#collapsed' => TRUE,
	);
	
	$optin_features = _get_all_cf_sms_optin_features();
	
	$response_features = _get_all_cf_sms_response_features();
	
	foreach ($optin_features as $feature) {
		$feature_id = $feature['id'];
		$feature_id_enabled = 'cf_sms_optin_feature_' . $feature_id . '_enabled';
		$features['optins'][$feature_id_enabled] = array(
			'#type' => 'checkbox',
			'#title' => '<b>' . $feature['name'] . ':</b> ' . $feature['text'],
			'#default_value' => $feature['enabled'],
		);
		foreach ($feature['settings'] as $setting_name => $setting) {
			$full_setting_name = 'cf_sms_optin_feature_' . $feature_id . '_' . $setting_name;
			$features['optins'][$full_setting_name] = $setting;
		}
	}
	
	foreach ($response_features as $feature) {
		$feature_id = $feature['id'];
		$feature_id_enabled = 'cf_sms_response_feature_' . $feature_id . '_enabled';
		if (is_array($feature['codeword'])) {
			$codeword = 'Codewords ';
			foreach ($feature['codeword'] as $word) {
				$codeword .= $word . ', ';
			}
			$codeword = substr_replace($codeword, '', -2); // gets rid of last ", "
		}
		else {
			$codeword = 'Codeword ' . $feature['codeword'];
		}
		$features['responses'][$feature_id_enabled] = array(
			'#type' => 'checkbox',
			'#title' => '<b>' . $codeword . ':</b> ' . $feature['text'],
			'#default_value' => $feature['enabled'],
		);
		foreach ($feature['settings'] as $setting_name => $setting) {
			$full_setting_name = 'cf_sms_response_feature_' . $feature_id . '_' . $setting_name;
			$features['responses'][$full_setting_name] = $setting;
		}
	}
	
	return $features;
	
} // end function - cf_sms_feature_settings


/**
 *
 * Runs functions for all TeleMo features that require cron. To make use of,
 * after creating your external feature function (say, tm_sms_feature_example),
 * create a function for cron to call (in this case, tm_sms_feature_example_cron)
 *
 */
function telemo_cron() {

	// execute scheduled sends
	telemo_execute_next_scheduled_actions();

	// get the services
	$services = _telemo_all_services();
	
	// loop through all the services
	foreach ($services as $service) {
	
		$funcname_prefix = 'tm_' . $service['id'] . '_feature_';
		
		// loop through all the features
		foreach ($service['features'] as $feature) {
			// execute the cron function, if any, for the feature
			$funcname = $funcname_prefix . $feature['id'] . '_cron';
			if (function_exists($funcname)) {
				call_user_func($funcname);
			}
		}

	}

}


/**
 * Gather all the TeleMo services by invoking all the hooks for them
 */
function _telemo_all_services() {

	$services = module_invoke_all('telemo_services');
	
	return $services;
}


/**
 * Gather all the enabled TeleMo services
 */
function _telemo_all_enabled_services() {

	$services = module_invoke_all('telemo_services');
	
	foreach ($services as $key => $service) {
		if (!$service['enabled']) {
			unset($services[$key]);
		}
	}
	
	return array_values($services);
}


/**
 * Return the default, built-in opt-in features as an array
 */
function tm_sms_telemo_services() {

	$services = array();
	
	// Ship Notification: send user an SMS when their order ships
	$features[] = array(
		'id' => 'ship',
		'name' => 'Shipment notification',
		'text' => 'Receive an SMS whenever your order ships.',
		'filename' => 'cf_sms.optin.ship.inc',
		'enabled' => variable_get('cf_sms_optin_feature_ship_enabled', FALSE),
		'settings' => array(
			'message' => array(
				'#type' => 'textarea',
				'#rows' => 3,
				'#resizable' => FALSE,
				'#title' => t('Shipment notification message'),
				'#default_value' => variable_get('cf_sms_optin_feature_ship_message', "!name, your !org order (for !total) has shipped. Your tracking number is !track"),
				'#description' => t('Message to send the customer when their order ships. You can use the special variables !name, !order, !total, !carrier, !track and/or !org to reference the customer first name, order number, total order cost, carrier (UPS, etc.), or tracking number, or the short name of the store/website/organization sending the text message.'),
			),
		),
	);
	
	// General Promotions: send user an SMS whenever any sort of promotion is announced
	$features[] = array(
		'id' => 'promos',
		'name' => 'Promotions signup',
		'text' => 'Receive an SMS whenever any promotion is announced.',
		'filename' => 'cf_sms.optin.inc',
		'enabled' => variable_get('cf_sms_optin_feature_promos_enabled', FALSE),
		'settings' => array(),
	);
	
	// General Info: send user an SMS when any general site information is released
	$features[] = array(
		'id' => 'info',
		'name' => 'Info signup',
		'text' => 'Receive an SMS whenever any general site info is released.',
		'filename' => 'cf_sms.optin.inc',
		'enabled' => variable_get('cf_sms_optin_feature_info_enabled', FALSE),
		'settings' => array(),
	);
	
	return $features;

} // end function - cf_sms_cf_sms_optin_features


/**
 * Gather all the SMS response features by invoking all the hooks for them
 */
function _get_all_cf_sms_response_features() {

	$features = module_invoke_all('cf_sms_response_features');
	
	return $features;
}


/**
 * Return the default, built-in response features as an array
 */
function cf_sms_cf_sms_response_features() {

	$features = array();
	
	// STOP: user sends this to stop receiving an SMS features, or all features
	$features[] = array(
		'id' => 'stop',
		'name' => 'Stop SMS sends',
		'text' => 'Stops either the latest SMS feature sent to the user, or all of them.',
		'filename' => 'cf_sms.response.inc',
		'codeword' => 'STOP',
		'enabled' => variable_get('cf_sms_response_feature_stop_enabled', FALSE),
		'settings' => array(),
	);
	
	// HELP: user sends this to get a helpful SMS (or two) about response features
	$features[] = array(
		'id' => 'help',
		'name' => 'Get SMS help',
		'text' => 'Receive an SMS with details on using the features or getting more info.',
		'filename' => 'cf_sms.response.inc',
		'codeword' => array('HELP', '?'),
		'enabled' => variable_get('cf_sms_response_feature_help_enabled', FALSE),
		'settings' => array(
			'message' => array(
				'#type' => 'textarea',
				'#rows' => 3,
				'#size' => 160,
				'#resizable' => FALSE,
				'#title' => t('Help response message'),
				'#default_value' => variable_get('cf_sms_response_feature_help_message', "Text NEW for the latest info or promo, SIGNUP to signup for an opt-in texting feature, or STOP to cancel an opt-in feature (STOP ALL to cancel all.)"),
				'#description' => t("Message to send the customer when they text 'HELP' to our system."),
			),
		),
	);
	
	// NEW: user sends this to get a copy of the latest promo or info SMS
	$features[] = array(
		'id' => 'new',
		'name' => 'Receive newest SMS',
		'text' => 'Receive an SMS of the latest promo or info without signing up.',
		'filename' => 'cf_sms.response.inc',
		'codeword' => array('NEW', 'NEWEST', 'LATEST'),
		'enabled' => variable_get('cf_sms_response_feature_new_enabled', FALSE),
		'settings' => array(
			'time_limit' => array(
				'#type' => 'textfield',
				'#title' => t('Maximum age of info/promo'),
				'#default_value' => variable_get('cf_sms_response_feature_new_time_limit', 13),
				'#description' => t("Maximum number of days to search for the most recent promo or info broadcast."),
			),
			'message' => array(
				'#type' => 'textarea',
				'#rows' => 3,
				'#size' => 160,
				'#resizable' => FALSE,
				'#title' => t('No new info/promo message'),
				'#default_value' => variable_get('cf_sms_response_feature_new_message', "Sorry, there have been no recent promos or info texts sent out. Check again soon!"),
				'#description' => t("Message to send the customer when there have been no promo or info broadcasts within the set time limit."),
			),
		),
	);
	
	// DAMAGE: user sends this to report a damaged shipment
	$features[] = array(
		'id' => 'damage',
		'name' => 'Report damaged shipment',
		'text' => "Reports that the user's latest shipment was damaged, and responds accordingly.",
		'filename' => 'cf_sms.response.inc',
		'codeword' => array('DAMAGE', 'DAMAGED', 'DAMAGES'),
		'enabled' => variable_get('cf_sms_response_feature_damage_enabled', FALSE),
		'settings' => array(),
	);
	
	// SIGNUP: user sends this to signup remotely for a specific opt-in feature
	$features[] = array(
		'id' => 'signup',
		'name' => 'Signup for feature',
		'text' => "Lets the user signup for a specific opt-in feature remotely.",
		'filename' => 'cf_sms.response.inc',
		'codeword' => 'SIGNUP',
		'enabled' => variable_get('cf_sms_response_feature_signup_enabled', FALSE),
		'settings' => array(),
	);

	return $features;

} // end function - cf_sms_cf_sms_response_features


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
 * Given a string, breaks the string into individual words,
 * returning an array, each element being a single word
 */
function _tm_sms_text_to_words($text) {

	$separators = " ,;\n\t\r" . chr(27) . chr(0);
	
	$words = array();
	$current_word = '';
	
	for ($i = 0; $i < strlen($text); $i++) {
		$c = $text[$i];
		$pos = strpos($separators, $c);
		if ($pos === FALSE) {
			$current_word .= $c;
		}
		else {
			if (strlen($current_word) > 0) {
				$words[] = $current_word;
				$current_word = '';
			}
		}
	}
	
	if (strlen($current_word) > 0) {
		$words[] = $current_word;
	}
	
	return $words;
}


/**
 * Returns the user IDs and mobile numbers of all users signed up for a specific TeleMo opt-in feature
 */
function _get_signups_for_telemo_feature($service_id, $feature_id) {

	$signedup_users = array();
	
	// all feature IDs in this field are prefixed by ':', and suffixed by ';'
	$tagged_signup = ':' . $service_id . '~' . $feature_id . ';';
	
	$data = db_query("SELECT id, mobile_num FROM {telemo_users} WHERE signups LIKE '%%%s%%'", $tagged_signup);
	while ($record = db_fetch_array($data)) {
		$signedup_users[] = $record;
	}
	
	return $signedup_users;

}


/**
 * Displays an unformatted mobile number (just a string of digits) with dashes, etc.
 * for ease of visual recognition. Strongly biased toward U.S. mobile numbers.
 */
function _cf_sms_display_mobile_num($num) {

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
function _array_subvalues($arr, $key) {

	$ret = array();
	
	foreach ($arr as $row) {
		$ret[] = $row[$key];
	}
	
	return $ret;
}
