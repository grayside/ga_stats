<?php

/**
 * Pull data from Google Analytics and prepare for storage.
 *
 * @param array|string $metric
 *   An array or string of the metrics to pull.
 * @param int $start_time
 *   The beginning of the timeframe for which data is pulled.
 * @param int $end_time
 *   The end of the timeframe for which data is pulled.
 * @param $timeframe
 *   A time_frame object.
 *
 * @return array
 *   An array of obj ready for the ga_stats_count table
 */
function ga_stats_get_data($metric, $start_time, $end_time, $timeframe = '') {
  $data_array = ga_stats_ga_data($metric, $start_time, $end_time);
  $metrics = ga_stats_ga_metrics();
  $counts = array();
  foreach ($data_array as $d) {
    $count = new stdClass;
    $count->url = $d['url'];
    $count->count = $d[$metric];
    $count->metric = $metric;
    $count->nid = FALSE;
    $count->timeframe = $timeframe;
    $alias = preg_replace('/^\//', '', $count->url);
    if (!preg_match('/^node\/([0-9]*)/', $alias, $matches) ) {
      $alias = drupal_lookup_path('source', $alias);
    }
    if (preg_match('/^node\/([0-9]*)/', $alias, $matches)  ) {
      $count->nid = $matches[1];
    }

    // only log nodes
    if ($count->nid) {
      $counts[] = $count;
    }
  }

  return $counts;
}

/**
 * Goes through sources and metrics and updates databases
 *
 * Views cannot tell the difference between the various metrics and timeframes
 * so we delete all counts before rebuilding.
 */
function _ga_stats_update_counts() {
  // Check that credentials are available.
  // @todo Move lower into callstack stack.
  if (!ga_stats_is_ready()) {
    drupal_set_message(t('Google Analytics email and password not set.'), 'error');
    watchdog('ga_stats', 'Google Analytics API credentials not set.', array(), WATCHDOG_WARNING);

    return FALSE;
  }

  $metrics = ga_stats_ga_metrics();
  $timeframes = ga_stats_ga_timeframes();
  $data = array();
  foreach ($metrics as $metric => $title) {
    foreach ($timeframes as $key => $time) {
      $filter = isset($time['filter']) ? $time['filter'] : NULL;
      $new_data = ga_stats_get_data($metric, time() - $time['secsToSub'], time(), $key, $filter);
      $data = array_merge($data, $new_data);
    }
  }

  db_query('DELETE FROM {ga_stats_count}');
  foreach ($data as $obj) {
    ga_stats_write_count($obj);
  }
  drupal_set_message(t('Counts Successfully Updated'));
  watchdog('ga_stats', 'Updated statistics for !count records.', array('!count' => count($data)));

  return ga_stats_schedule_update();
}

/**
 * Write the data associated with a specific page to the database.
 *
 * @param object $count
 */
function ga_stats_write_count($count) {
  drupal_write_record('ga_stats_count', $count);
}



/**
 *  call back funciton for pulling data from google anaylics
 *  @param metrics_in : and array or string of the metrics to pull
 *  @param start_date : time as an int
 *  @param end_date : time as an int
 *  @return : and array of external_statistics_count db obj
 *
 */
function ga_stats_ga_data($metrics, $start_date = 0, $end_date = 0, $filter=FALSE) {
  $url_dim = 'pagePath';
  if (!is_array($metrics)) {
   $metrics = array($metrics);
  }
  $request['dimensions'] = array($url_dim);
  $request['metrics'] = $metrics;
  
  if ($start_date)
    $request['start_date'] = date('Y-m-d', $start_date);
  else
    $request['start_date'] = NULL;
    
  if ($end_date)
    $request['end_date'] = date('Y-m-d', $end_date);
  else
    $request['end_date'] = NULL;
  
  $request['sort_metric'] = "-" . $metrics[0];
  $request['max_results'] = variable_get('ga_stats_max_results', "100");
  
  if ($filter) {
    $request['filter'] = $filter;
  }
  
  $data_raw = ga_stats_query_data($request);
  $data_array = ga_stats_ga_data_array($data_raw);
  foreach ($data_array as $k => $d) {
    $data_array[$k]['url'] = $d[$url_dim];
  }
  
  return $data_array;
}

/*
 * Helper function to list metrics with plugin names and google analytics names
 */
function ga_stats_ga_metrics($all=FALSE) {
  
  $metrics = array(
    'pageviews' => 'Page Views',
    'uniquePageviews' => 'Unique Page Views',
    'avgTimeOnPage' => 'Average Time on Page',
    'entrances' => 'Entrance Page'
  );
  
  if (!$all) {
    $enabled = variable_get('ga_stats_enabled_metrics', array('pageviews' => TRUE));
    foreach ($metrics as $k => $v) {
      if (!isset($enabled[$k]) || !$enabled[$k]) {
        unset($metrics[$k]);
      }
    }
  }
  
  return $metrics;
}

/**
 *  @param $just_keys : whether we should return all the data or just the keys->values array
 *  TODO: add forever
 */
function ga_stats_ga_timeframes($just_keys = FALSE, $all = FALSE) {
  if ($just_keys) {
    $timeframes = array(
      'hour' => 'in the past hour',
      'today' => 'in the past 24 hours',
      '2days' => 'in the past 48 hours',
      'week' => 'in the past 7 days',
      'month' => 'in the past 31 days',
      'forever' => 'for all time'
    );
  }
  else {
    $timeframes = array(
      'hour' => array('secsToSub' => (60*60), 'filter' => 'hour=='+date('G'), 'title' => 'in the past hour'),
      'today' => array('secsToSub' => (60*60*24), 'filter' => 'day=='+date('j'), 'title' => 'in the past 24 hours'),
      '2days' => array('secsToSub' => (60*60*24*2), 'title' => 'in the past 48 hours'),
      'week' => array('secsToSub' => (60*60*24*7), 'title' => 'in the past 7 days'),
      'month' => array('secsToSub' => (60*60*24*31), 'title' => 'in the past 31 days'),
      'forever' => array('secsToSub' => time() - strtotime('2005-01-01'), 'title' => 'for all time')
    );
  }
  
  if (!$all) {
    $enabled = variable_get('ga_stats_enabled_timeframes', array('today' => TRUE, 'month' => TRUE));
    foreach ($timeframes as $k => $v) {
      if (!isset($enabled[$k]) || !$enabled[$k]) {
        unset($timeframes[$k]);
      }
    }
  }
  
  return $timeframes;
}

function ga_stats_ga_data_array($data_in) {
  $data_all = array();
  $data = array();

  foreach ($data_in as $d) {
    $metrics = $d->getMetrics();
    $dimensions = $d->getDimensions();
    $data_all[] = array_merge($metrics, $dimensions);
  }
  return $data_all;
}

function ga_stats_ga_get_accounts() {
  require_once 'gapi.class.php';
  
  $user = variable_get('ga_stats_email', '');
  $password = variable_get('ga_stats_password', '');
  $type = variable_get('ga_stats_acct_type', NULL);
    
  if ($user && $password) {
    try{
      $ga = new gapi($user, $password, NULL, $type);
      $accounts = $ga->requestAccountData(1, 5000); 
      return $accounts;
    } catch (Exception $e) {
      drupal_set_message(t('Invalid Google Analytics login.'), 'error');
      watchdog('ga_stats', 'Invalid Google Analytics login.');
      return;
    }    
  }
  else {
    drupal_set_message(t('Google Analytics Email and password not set - Could not retrieve Account Information.'), 'error');
    watchdog('ga_stats', 'Google Analytics email and password not set.');
  }
}

/**
 *  @param $metric - one of the metrics values - a string value - i.e. 'pageviews'
 *  $report_id, $dimensions, $metrics, $sort_metric=null, $filter=null, $start_date=null, $end_date=null, $start_index=1, $max_results=30
 */

function ga_stats_query_data($request) {
  //this is really a waste I think because this shit is preprocessed
  require_once 'gapi.class.php';
  
  $user = variable_get('ga_stats_email', '');
  $password = variable_get('ga_stats_password', '');
  $aid = variable_get('ga_stats_profile', '');
  $type = variable_get('ga_stats_acct_type', NULL);
  
  if ($user && $password) {
    try{
      $ga = new gapi($user, $password, NULL, $type);
      $data = $ga->requestReportData($aid, $request['dimensions'], $request['metrics'], $request['sort_metric'], NULL, $request['start_date'], $request['end_date'], 1, $request['max_results']);
    } catch (Exception $e) {
      drupal_set_message(t('Invalid Google Analytics login.'), 'error');
      watchdog('ga_stats', 'Invalid Google Analytics login.');
      return array();
    }    
    return $data;    
  }
  else {
    drupal_set_message(t('Google Analytics Email and password not set.'), 'error');
    watchdog('ga_stats', 'Google Analytics email and password not set.');
  }
}