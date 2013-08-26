<?php

/**
 * Callback for the GA Stats admin form.
 */
function ga_stats_admin_settings() {
  $form = array();

  $form['ga_stats_login'] = array(
    '#type' => 'fieldset',
    '#title' => t('Google Analytics Login Information'),
    '#collapsible' => TRUE
  );
  if (variable_get('ga_stats_email', FALSE)) {
    $form['ga_stats_login']['#collapsed'] = TRUE;
  }

  $form['ga_stats_login']['ga_stats_email'] = array(
    '#type' => 'textfield',
    '#title' => t('Account Email'),
    '#description' => t('The email account you use to log in to Google Analytics'),
    '#default_value' => variable_get('ga_stats_email', '')
  );

  $form['ga_stats_login']['ga_stats_password'] = array(
    '#type' => 'password',
    '#title' => t('Account Password'),
    '#description' => t('The password you use to log in to Google Analytics'),
  );

  $form['ga_stats_login']['ga_stats_acct_type'] = array(
    '#type' => 'select',
    '#title' => t('Account Type'),
    '#description' => t('The account type you use to log in to Google Analytics'),
    '#options' => array(
      'GOOGLE' => t('Google'),
      'HOSTED' => t('Hosted'),
    ),
    '#default_value' => variable_get('ga_stats_acct_type', NULL),
  );

  $form['ga_stats_accounts'] = array(
    '#type' => 'fieldset',
    '#title' => t('Google Analytics Tracking Accounts'),
    '#collapsible' => TRUE
  );
  if (variable_get('ga_stats_email', FALSE) && variable_get('ga_stats_password', FALSE)) {
    require_once 'includes/ga.inc';
    $account = ga_stats_ga_get_accounts(ga_stats_get_client());
  }
  else {
    $account = FALSE;
  }
  $options = array();
  if (is_array($account)) {
    foreach ($account as $id => $value) {
      $acc = $value->getProperties();
      $options[$acc['profileId']] = "{$acc['title']} ({$acc['webPropertyId']})";
    }
  }
  if ($options) {
    $form['ga_stats_accounts']['ga_stats_profile'] = array(
      '#type' => 'select',
      '#title' => t('Google Analytics Profile to Use'),
      '#description' => t('The Google Analytics profile from which to retrieve statistics'),
      '#options' => $options,
      '#default_value' => variable_get('ga_stats_profile', '')
    );

  }
  else {
    $form['ga_stats_accounts']['ga_stats_profile'] = array(
      '#type' => 'markup',
      '#markup' => '<div class="messages warning">' . t('Email and Pasword not set or invalid. Please set the login information and save this form. You will then be able to view and configure account information.') . '</div>'
    );
  }

  $form['enabled_stats'] = array(
    '#type' => 'fieldset',
    '#title' => t('Enabled Statistics'),
    '#description' => t('Make sure to clear the Drupal cache after changing this setting in order to inform Views of the new settings. <br/><em><b>WARNING:</b> Do not disable a setting which is currently in use in Views.</em>')
  );

  $form['enabled_stats']['ga_stats_enabled_metrics'] = array(
    '#type' => 'checkboxes',
    '#default_value' => variable_get('ga_stats_enabled_metrics', array('pageviews')),
    '#options' => ga_stats_ga_metrics(true),
    '#title' => t('Metrics'),
    '#description' => t('The metrics that will be available from Google Analytics in Views.')
  );

  $form['enabled_stats']['ga_stats_enabled_timeframes'] = array(
    '#type' => 'checkboxes',
    '#default_value' => variable_get('ga_stats_enabled_timeframes', array('today', 'month')),
    '#options' => ga_stats_ga_timeframes(TRUE, TRUE),
    '#title' => t('Time Frames'),
    '#description' => t('The timeframes that will be available from Google Analytics in Views.')
  );

  $form['ga_stats_max_results'] = array(
    '#type' => 'textfield',
    '#title' => t('Max Results per Metric/Timeframe'),
    '#description' => t('The max results that a call (a metric/timeframe combination) can return. MUST be a number.'),
    '#default_value' => variable_get('ga_stats_max_results', '100')
  );

  if (variable_get('ga_stats_profile', FALSE)) {
    $form['actions']['ga_stats_update'] = array(
      '#type' => 'button',
      '#value' => t('Update Counts'),
      '#weight' => 20,
      '#executes_submit_callback' => TRUE,
      '#submit' => array('ga_stats_update_counts'),
    );
  }
  $form['#validate'][] = 'ga_stats_form_validate';
  return system_settings_form($form);
}

/**
 *  Just set the password if we already have one and the form field was ignored.
 */
function ga_stats_form_validate($form, &$form_state) {
  if (!$form_state['values']['ga_stats_password']) {
    $form_state['values']['ga_stats_password'] = variable_get('ga_stats_password', '');
  }
}
