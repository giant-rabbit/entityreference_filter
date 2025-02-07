<?php
/**
 * @file
 * Definition of entityreference_filter_view_result.
 */

/**
 * The filter to select items from the list generated by another view.
 *
 * @ingroup views_filter_handlers
 */
class entityreference_filter_view_result extends views_handler_filter_in_operator {

  /**
   * {@inheritdoc}
   */
  public function option_definition() {
    $options = parent::option_definition();

    $options['value']['default'] = NULL;
    $options['reference_display'] = array('default' => '');
    $options['reference_arguments'] = array('default' => '');
    $options['expose']['contains']['hide_empty_filter'] = array(
      'default' => TRUE,
      'bool' => TRUE,
    );

    return $options;
  }

  /**
   * {@inheritdoc}
   */
  public function value_form(&$form, &$form_state) {
    parent::value_form($form, $form_state);

    if (!empty($form_state['exposed'])) {
      // Run time.
      $identifier = $this->options['expose']['identifier'];
      if ((isset($this->options['reference_arguments'])) && (strpos($this->options['reference_arguments'], '[') !== FALSE)) {
        // This filter depends on other filters dynamically,
        // store data for configuring Backdrop.settings.
        $form['#attached']['js'][] = backdrop_get_path('module', 'entityreference_filter') . '/entityreference_filter.js';

        if (empty($form['#after_build'])) {
          $form['#after_build'] = array();
        }
        if (!in_array('entityreference_filter_exposed_form_after_build', $form['#after_build'])) {
          $form['#after_build'][] = 'entityreference_filter_exposed_form_after_build';
        }

        $form['#entityreference_filter_view'] = $this->view->name;
        $form['#entityreference_filter_display'] = $this->view->current_display;
        $form['#entityreference_filter_args'] = $this->dynamic_filter_get_view_args();
        $form['#entityreference_filter_autosubmit'] = $this->is_auto_submit();
        if (!isset($form['#entityreference_filter_dynamic'])) {
          $form['#entityreference_filter_dynamic'] = array();
        }
        $form['#entityreference_filter_dynamic'][$identifier] = $this->dynamic_filter_get_controlling_filters();
      }

      $values = $this->get_value_options();

      // Delete irrelevant default values.
      $default_value = isset($form_state['input'][$identifier]) ? $form_state['input'][$identifier] : array();
      if (!is_array($default_value)) {
        $default_value = array($default_value);
      }
      $default_value = array_intersect($default_value, array_keys($values));
      if (empty($this->options['expose']['multiple'])) {
        if (empty($this->options['expose']['required']) && (empty($default_value) || !empty($this->options['expose']['reduce']))) {
          $default_value = 'All';
        }
        elseif (empty($default_value)) {
          $keys = array_keys($values);
          $default_value = array_shift($keys);
        }
        else {
          $copy = $default_value;
          $default_value = array_shift($copy);
        }
      }
      $form_state['input'][$identifier] = $default_value;

      if ((empty($values)) && (!empty($this->options['expose']['hide_empty_filter']))) {
        // Hide the form element.
        if (isset($form['value'])) {
          $form['value']['#prefix'] = '<div class="element-hidden">';
          $form['value']['#suffix'] = '</div>';
        }
      }

      return;
    }
    else {
      // Configuration via View UI time.
      if ((isset($form['operator']['#type'])) && ($form['operator']['#type'] === '')) {
        // Views dependent filters module needs option list
        // to configure dependencies.
        $values = $this->get_value_options();
        if (empty($values)) {
          if (isset($form['value'])) {
            $form['value']['#access'] = FALSE;
          }
        }
        return;
      }

      // Don't show value selection widget.
      $form['value']['#access'] = FALSE;
    }

    $displays = views_get_applicable_views('entityreference display');
    // Filter views that list the entity type we want, and group the separate
    // displays by view.
    $options = array();
    $field_base_table = isset($this->definition['filter_base_table']) ? $this->definition['filter_base_table'] : '';
    foreach ($displays as $data) {
      list($view, $display_name) = $data;
      if ((empty($field_base_table)) || ($view->base_table === $field_base_table)) {
        $options[$view->name . ':' . $display_name] = $view->name . ' - ' . $view->display[$display_name]->display_title;
      }
    }

    $form['reference_display'] = array(
      '#type' => 'select',
      '#title' => t('View used to select the entities'),
      '#required' => TRUE,
      '#options' => $options,
      '#default_value' => $this->options['reference_display'],
      '#description' => '<p>' . t('Choose the view and display that select the entities that can be referenced.<br />Only views with a display of type "Entity Reference" are eligible.') . '</p>',
    );

    if (empty($this->options['reference_display'])) {
      $form['reference_display']['#description'] .= '<p>' . t('Entity list will be available after saving this setting.') . '</p>';
    }

    $form['reference_arguments'] = array(
      '#type' => 'textfield',
      '#size' => 50,
      '#maxlength' => 256,
      '#title' => t('Arguments for the view'),
      '#default_value' => isset($this->options['reference_arguments']) ? $this->options['reference_arguments'] : '',
      '#description' => t('Define arguments for the view that selects the entities. Empty value means passing arguments of the current view. Format is arg1/arg2/...argN. Possible arguments are !n - argument number n of the current view, [filter_name] - value of the named exposed filter, other strings are passed as is.'),
    );
  }

  public function value_submit($form, &$form_state) {
    $form_state['values']['options']['value'] = NULL;
  }

  public function expose_form(&$form, &$form_state) {
    parent::expose_form($form, $form_state);

    $form['expose']['hide_empty_filter'] = array(
      '#type' => 'checkbox',
      '#title' => t('Hide empty filter'),
      '#description' => t('Hide the exposed widget if the entity list is empty.'),
      '#default_value' => $this->options['expose']['hide_empty_filter'],
    );
  }

  public function exposed_info() {
    $info = parent::exposed_info();

    if (!empty($this->options['expose']['hide_empty_filter'])) {
      $values = $this->get_value_options();
      if (empty($values)) {
        unset($info['operator'], $info['label'], $info['description']);
      }
    }

    return $info;
  }

  public function get_value_options() {
    if (isset($this->value_options)) {
      return $this->value_options;
    }

    // Initialize this to prevent infinite loops
    // when circular dependency is set.
    $this->value_options = array();

    // This calculation can create infinite loop.
    $this->value_options = $this->get_configured_views_result();

    return $this->value_options;
  }

  public function query() {
    // Recalculate values if the filter is not exposed.
    if (empty($this->options['exposed'])) {
      $this->value = array_keys($this->get_configured_views_result());
      if (empty($this->value)) {
        // If the result is empty, add zero value item to ensure SQL validity.
        $this->value = array(0);
      }
    }

    parent::query();
  }

  public function get_configured_views_result() {
    if (empty($this->options['reference_display'])) {
      return array();
    }
    list($view_name, $display_name) = explode(':', $this->options['reference_display']);
    // Check that the view is valid and the display still exists.
    /** @var view $view */
    $view = views_get_view($view_name);
    if (!$view || !isset($view->display[$display_name]) || !$view->access($display_name)) {
      watchdog('entityreference', 'The view %view_name is no longer eligible for the filter.', array('%view_name' => $view_name), WATCHDOG_WARNING);
      return array();
    }
    $view->set_display($display_name);
    // Make sure the query is not cached.
    $view->is_cacheable = FALSE;
    $view->set_items_per_page(0);
    // Make the display render the result as array.
    $view->display_handler->set_option('entityreference_options', array('limit' => 0));
    // Avoid warnings when the view can't run.
    $view->display_handler->id_field_alias = '';

    $filters_by_identifier = array();

    $args = $this->dynamic_filter_get_view_args();

    if (isset($this->options['reference_arguments'])) {
      $arg_str = trim($this->options['reference_arguments']);
      if (strlen($arg_str) > 0) {
        $args = explode('/', $arg_str);
        foreach ($args as $i => $arg) {
          $arg = trim($arg);
          $first_char = backdrop_substr($arg, 0, 1);

          if ($first_char === '!') {
            $arg_no = intval(backdrop_substr($arg, 1)) - 1;
            if ($arg_no >= 0) {
              $args[$i] = isset($this->view->args[$arg_no]) ? $this->view->args[$arg_no] : NULL;
              if ((is_null($args[$i])) && (!empty($this->view->argument))) {
                $arg_keys = array_keys($this->view->argument);
                if (isset($arg_keys[$arg_no])) {
                  $args[$i] = $this->view->argument[$arg_keys[$arg_no]]->get_value();
                }
              }
            }
          }

          if ($first_char === '[') {
            if (backdrop_substr($arg, -1, 1) === ']') {
              if ((empty($filters_by_identifier)) && (!empty($this->view->filter))) {
                foreach ($this->view->filter as $filter_handler) {
                  if (empty($filter_handler->options['exposed'])) {
                    continue;
                  }
                  $filters_by_identifier[$filter_handler->options['expose']['identifier']] = $filter_handler;
                }
              }

              $args[$i] = NULL;
              $filter_name = backdrop_substr($arg, 1, backdrop_strlen($arg) - 2);
              $input = $this->view->get_exposed_input();
              if (isset($input[$filter_name])) {
                $args[$i] = $input[$filter_name];
              }
              elseif (isset($filters_by_identifier[$filter_name])) {
                $args[$i] = $filters_by_identifier[$filter_name]->value;
              }

              if (($args[$i] === 'All') || ($args[$i] === array())) {
                $args[$i] = NULL;
              }

              if (is_array($args[$i])) {
                reset($args[$i]);
                $args[$i] = current($args[$i]);
              }

              if (is_null($args[$i])) {
                // Try to get the first value of list of options
                // if configured so.
                if (isset($filters_by_identifier[$filter_name])) {
                  $filter_handler = $filters_by_identifier[$filter_name];
                  if (((!empty($filter_handler->always_required)) || (!empty($filter_handler->options['expose']['required'])))
                      && (empty($filter_handler->options['expose']['multiple'])) && (method_exists($filter_handler, 'get_value_options'))) {
                    $values = $filter_handler->get_value_options();

                    if (count($values) > 0) {
                      reset($values);
                      $filter_handler->value = empty($filter_handler->options['expose']['multiple']) ? key($values) : array(key($values));
                      $args[$i] = key($values);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    $result = $view->execute_display($display_name, $args);

    if (is_array($result)) {
      // Clean entity reference style output.
      foreach ($result as &$item) {
        $item = trim($item);
      }
    }

    return $result ? $result : array();
  }

  public function dynamic_filter_depends_on($filter_name) {
    if (isset($this->options['reference_arguments'])) {
      $filter_str = '[' . $filter_name . ']';
      $args = explode('/', trim($this->options['reference_arguments']));
      foreach ($args as $arg) {
        return (trim($arg) === $filter_str);
      }
    }

    return FALSE;
  }

  public function dynamic_filter_get_controlling_filters() {
    $filters = array();
    if (isset($this->options['reference_arguments'])) {
      $arg_str = trim($this->options['reference_arguments']);
      if (strlen($arg_str) > 0) {
        $args = explode('/', $arg_str);
        foreach ($args as $arg) {
          $arg = trim($arg);
          $first_char = backdrop_substr($arg, 0, 1);
          if ($first_char === '[') {
            if (backdrop_substr($arg, -1, 1) === ']') {
              $filter_name = backdrop_substr($arg, 1, backdrop_strlen($arg) - 2);
              $filters[] = $filter_name;
            }
          }
        }
      }
    }

    return $filters;
  }

  public function dynamic_filter_get_view_args() {
    $args = $this->view->args;

    if (empty($this->view->argument)) {
      return $args;
    }

    $arg_count = count($args);
    $argument_count = count($this->view->argument);
    if ($argument_count > $arg_count) {
      for ($i = $arg_count; $i < $argument_count; $i++) {
        $args[] = NULL;
      }
    }

    $arg_keys = array_keys($this->view->argument);
    foreach ($args as $arg_no => $arg) {
      if ((is_null($arg)) && (isset($arg_keys[$arg_no]))) {
        $args[$arg_no] = $this->view->argument[$arg_keys[$arg_no]]->get_value();
      }
    }

    return $args;
  }

  public function is_auto_submit() {
    $exposed_form = $this->view->display_handler->get_plugin('exposed_form');
    return !empty($exposed_form->options['autosubmit']);
  }

}
