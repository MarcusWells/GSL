<?php
/**
 * @file
 * This generates a report showing all the files in the file_usage table that
 * are not in the file_managed table.
 */

/**
 * Generates the report.
 *
 * @param array $form
 *   The form definition.
 * @param array $form_state
 *   The current state of the form.
 *
 * @return array
 *   The form definition.
 */
function auditfiles_used_not_managed_form($form, &$form_state) {
  // Check to see if the confirmation form needs to be displayed instead of the
  // normal form.
  if (isset($form_state['storage']['confirm'])) {
    return _auditfiles_used_not_managed_confirm_operation($form, $form_state);
  }

  $header = array(
    'fid' => array(
      'data' => t('File ID'),
      'field' => 'fu.fid',
      'sort' => 'asc',
    ),
    'module' => array(
      'data' => t('Used by'),
      'field' => 'fu.module',
    ),
    'id' => array(
      'data' => t('Used in'),
      'field' => 'fu.id',
    ),
    'count' => array(
      'data' => t('Count'),
      'field' => 'fu.count',
    ),
  );

  $rows = _auditfiles_used_not_managed_get_data($header);

  // Set up the pager.
  if (!empty($rows)) {
    $items_per_page = variable_get('auditfiles_report_options_items_per_page', 50);
    $current_page = pager_default_initialize(count($rows), $items_per_page);
    // Break the total data set into page sized chunks.
    $pages = array_chunk($rows, $items_per_page, TRUE);
  }

  // Define the form.
  // Setup the record count and related messages.
  if (!empty($rows)) {
    $maximum_records = variable_get('auditfiles_report_options_maximum_records', 0);
    if ($maximum_records > 0 && count($rows) >= $maximum_records) {
      $file_count_message = 'Found at least @count files in the file_usage table that are not in the file_managed table.';
    }
    else {
      $file_count_message = 'Found @count files in the file_usage table that are not in the file_managed table.';
    }
    $form_count = format_plural(
      count($rows),
      'Found 1 file in the file_usage table that is not in the file_managed table.',
      $file_count_message
    );
  }
  else {
    $form_count = 'Found no files in the file_usage table that are not in the file_managed table.';
  }
  $form['files'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#empty' => t('No items found.'),
    '#prefix' => '<div><em>' . $form_count . '</em></div>',
  );
  if (!empty($rows) && !empty($pages)) {
    $form['files']['#options'] = $pages[$current_page];
  }
  else {
    $form['files']['#options'] = array();
  }

  if (!empty($rows)) {
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Delete selected items from the file_usage table'),
    );

    $form['pager'] = array('#markup' => theme('pager'));
  }

  return $form;
}

/**
 * Submit handler for the auditfiles_used_not_managed_form form.
 */
function auditfiles_used_not_managed_form_submit($form, &$form_state) {
  // Check to see if any files were selected.
  if (!empty($form_state['storage']['files'])) {
    foreach ($form_state['storage']['files'] as $file_id) {
      if ($file_id != 0) {
        // The operation has been confirmed, so delete the records.
        _auditfiles_used_not_managed_delete_record($file_id);
      }
    }
  }
  elseif (!empty($form_state['values']['files'])) {
    foreach ($form_state['values']['files'] as $file_id) {
      if ($file_id != 0) {
        // At least one file was selected. Check to see if the confirmation form
        // has been processed.
        if (!isset($form_state['values']['process'])) {
          // The operation has not been confirmed, so modify the data to display
          // the confirmation form.
          $form_state['storage']['files'] = $form_state['values']['files'];
          $form_state['storage']['confirm'] = TRUE;
          $form_state['rebuild'] = TRUE;
          return TRUE;
        }
      }
    }

    drupal_set_message(t('No items were selected to delete.'));
  }
}

/**
 * Gets the data to display on the page.
 *
 * @param array $header
 *   The header to be used for the table.
 *
 * @return array
 *   The data to be displayed in the table body.
 */
function _auditfiles_used_not_managed_get_data($header) {
  // Get all the files in the file_usage table.
  $managed_results = db_query("SELECT fid, filename FROM {file_managed}");
  // Break out the IDs, so they can be used in the next query.
  foreach ($managed_results as $managed_result) {
    $file_managed_ids[] = $managed_result->fid;
  }

  // Get all the files listed in the file_managed database table.
  $query = db_select('file_usage', 'fu');
  $query->extend('TableSort')->orderByHeader($header);
  $query->fields('fu');
  $query->condition('fid', $file_managed_ids, 'NOT IN');
  $maximum_records = variable_get('auditfiles_report_options_maximum_records', 0);
  if ($maximum_records > 0) {
    $query->range(0, $maximum_records);
  }
  $results = $query->execute();

  // Set up the table rows.
  $rows = array();

  foreach ($results as $file) {
    // If the file does not exist, add it to the table.
    $rows[$file->fid] = array(
      'fid' => $file->fid,
      'module' => $file->module . ' ' . t('module'),
      'id' => l($file->type . '/' . $file->id, $file->type . '/' . $file->id),
      'count' => $file->count,
    );
  }

  return $rows;
}

/**
 * Presents a confimation form to verify the user wants to complete the action.
 *
 * @param array $form
 *   The form definition.
 * @param array $form_state
 *   The current state of the form.
 *
 * @return array
 *   A form array for a confirmation form.
 */
function _auditfiles_used_not_managed_confirm_operation($form, &$form_state) {
  $values = $form_state['input'];

  $form['changelist'] = array(
    '#prefix' => '<ul>',
    '#suffix' => '</ul>',
    '#tree' => TRUE,
  );

  // Prepare the list of items to present to the user.
  if (!empty($values['files'])) {
    foreach ($values['files'] as $file_id) {
      if (!empty($file_id)) {
        $file = file_load($file_id);
        if (!empty($file)) {
          $form['changelist'][$file_id] = array(
            '#type' => 'hidden',
            '#value' => $file_id,
            '#prefix' => '<li><strong>' . $file->filename . '</strong> '
              . t('will be deleted from the file_usage table.'),
            '#suffix' => "</li>\n",
          );
        }
      }
      else {
        // Unsetting the unprocessed files prevents confirm_submit from dealing
        // with them.
        unset($form_state['input']['files'][$file_id]);
      }
    }
  }

  $form['operation'] = array(
    '#type' => 'hidden',
    '#value' => 'delete',
  );

  // Tell the submit handler to process the confirmation.
  $form['process'] = array(
    '#type' => 'hidden',
    '#value' => 'TRUE'
  );
  // Go back to the main form, when done with this one.
  $form['destination'] = array(
    '#type' => 'hidden',
    '#value' => 'admin/reports/auditfiles/usednotmanaged'
  );

  return confirm_form(
    $form,
    t('Delete these items from the file_usage table?'),
    'admin/reports/auditfiles/usednotmanaged',
    '<strong>' . t('This action cannot be undone.') . '</strong>',
    t('Yes'),
    t('No')
  );
}

/**
 * Deletes a specified file from the database.
 *
 * @param integer $file_id
 *   The ID of the file to delete from the database.
 */
function _auditfiles_used_not_managed_delete_record($file_id) {
  $result = db_delete('file_usage')
    ->condition('fid', $file_id)
    ->execute();
  if ($result == 0) {
    drupal_set_message(
      t(
        'There were one or more problems deleting one or more records with the file ID of %fid. Check the logs for more information.',
        array('%fid' => $file_id)
      ),
      'warning'
    );
  }
}
