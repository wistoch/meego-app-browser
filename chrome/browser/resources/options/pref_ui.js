// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var Preferences = options.Preferences;
  /////////////////////////////////////////////////////////////////////////////
  // PrefCheckbox class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefCheckbox = cr.ui.define('input');

  PrefCheckbox.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'checkbox';
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.managed = event.value['managed']
            self.disabled = self.managed;
            self.checked = event.value['value'];
          });

      // Listen to user events.
      this.addEventListener('click',
          function(e) {
            Preferences.setBooleanPref(self.pref, self.checked);
          });
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefCheckbox, 'pref', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefRadio class:

  //Define a constructor that uses an input element as its underlying element.
  var PrefRadio = cr.ui.define('input');

  PrefRadio.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'radio';
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.managed = event.value['managed']
            self.disabled = self.managed;
            self.checked = String(event.value['value']) == self.value;
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            if(self.value == 'true' || self.value == 'false') {
              Preferences.setBooleanPref(self.pref,
                  self.value == 'true');
            } else {
              Preferences.setIntegerPref(self.pref,
                  parseInt(self.value, 10));
            }
          });
    },

    /**
     * Getter for preference name attribute.
     */
    get pref() {
      return this.getAttribute('pref');
    },

    /**
     * Setter for preference name attribute.
     */
    set pref(name) {
      this.setAttribute('pref', name);
    }
  };


  /////////////////////////////////////////////////////////////////////////////
  // PrefNumeric class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefNumeric = function() {};
  PrefNumeric.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.managed = event.value['managed']
            self.disabled = self.managed;
            self.value = event.value['value'];
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            Preferences.setIntegerPref(self.pref, self.value);
          });
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefNumeric, 'pref', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefNumber class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefNumber = cr.ui.define('input');

  PrefNumber.prototype = {
    // Set up the prototype chain
    __proto__: PrefNumeric.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'number';
      PrefNumeric.prototype.decorate.call(this);
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefRange class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefRange = cr.ui.define('input');

  PrefRange.prototype = {
    // Set up the prototype chain
    __proto__: PrefNumeric.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'range';
      PrefNumeric.prototype.decorate.call(this);
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefSelect class:

  // Define a constructor that uses an select element as its underlying element.
  var PrefSelect = cr.ui.define('select');

  PrefSelect.prototype = {
    // Set up the prototype chain
    __proto__: HTMLSelectElement.prototype,

    /**
    * Initialization function for the cr.ui framework.
    */
    decorate: function() {
      var self = this;
      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.managed = event.value['managed']
            self.disabled = self.managed;
            for (var i = 0; i < self.options.length; i++) {
              if (self.options[i].value == event.value['value']) {
                self.selectedIndex = i;
                return;
              }
            }
            // Item not found, select first item.
            self.selectedIndex = 0;
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            switch(self.dataType) {
              case 'number':
                Preferences.setIntegerPref(self.pref,
                    self.options[self.selectedIndex].value);
                break;
              case 'boolean':
                Preferences.setBooleanValue(self.pref,
                    self.options[self.selectedIndex].value);
                break;
              case 'string':
                Preferences.setStringPref(self.pref,
                    self.options[self.selectedIndex].value);
                break;
            }
          });

      // Initialize options.
      this.ownerDocument.addEventListener('DOMContentLoaded',
          function() {
            var values = self.getAttribute('data-values');
            if (values) {
              self.initializeValues(templateData[values]);
            }
          });
    },

    /**
     * Sets up options in select element.
     * @param {Array} options List of option and their display text.
     * Each element in the array is an array of length 2 which contains options
     * value in the first element and display text in the second element.
     *
     * TODO(zelidrag): move this to that i18n template classes.
     */
    initializeValues: function(options) {
      options.forEach(function (values) {
        if (this.dataType == undefined)
          this.dataType = typeof values[0];
        this.appendChild(new Option(values[1], values[0]));
      }, this);
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'pref', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefTextField class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefTextField = cr.ui.define('input');

  PrefTextField.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.managed = event.value['managed']
            self.disabled = self.managed;
            self.value = event.value['value'];
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            Preferences.setStringPref(self.pref, self.value);
          });

      window.addEventListener('unload',
          function() {
            if (document.activeElement == self)
              self.blur();
          });
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'pref', cr.PropertyKind.ATTR);

  // Export
  return {
    PrefCheckbox: PrefCheckbox,
    PrefNumber: PrefNumber,
    PrefNumeric: PrefNumeric,
    PrefRadio: PrefRadio,
    PrefRange: PrefRange,
    PrefSelect: PrefSelect,
    PrefTextField: PrefTextField
  };

});

