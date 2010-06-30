// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
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
          self.checked = event.value;
        });

    // Listen to user events.
    this.addEventListener('click',
        function(e) {
          Preferences.setBooleanPref(self.pref,
                                     self.checked);
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

///////////////////////////////////////////////////////////////////////////////
// PrefRange class:

// Define a constructor that uses an input element as its underlying element.
var PrefRange = cr.ui.define('input');

PrefRange.prototype = {
  // Set up the prototype chain
  __proto__: HTMLInputElement.prototype,

  /**
   * Initialization function for the cr.ui framework.
   */
  decorate: function() {
    this.type = 'range';
    var self = this;

    // Listen to pref changes.
    Preferences.getInstance().addEventListener(this.pref,
        function(event) {
          self.value = event.value;
        });

    // Listen to user events.
    this.addEventListener('change',
        function(e) {
          Preferences.setIntegerPref(self.pref, self.value);
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


///////////////////////////////////////////////////////////////////////////////
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
          for (var i = 0; i < self.options.length; i++) {
            if (self.options[i].value == event.value) {
              self.selectedIndex = i;
              return;
            }
          }
          self.selectedIndex = -1;
        });

    // Listen to user events.
    this.addEventListener('change',
        function(e) {
          Preferences.setStringPref(self.pref,
              self.options[self.selectedIndex].value);
        });
  },

  /**
   * Sets up options in select element.
   * @param {Array} options List of option and their display text.
   * Each string in the array contains options value and display text split
   * with '|' character.
   *
   * TODO(zelidrag): move this to that i18n template classes.
   */
  initializeValues: function(options) {
    var self = this;
    options.forEach(function (option) {
      var values = option.split('|');
      self.appendChild(new Option(values[1], values[0], false, false));
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
