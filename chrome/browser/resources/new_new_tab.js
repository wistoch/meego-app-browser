// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// To avoid creating tons of unnecessary nodes. We assume we cannot fit more
// than this many items in the miniview.
var MAX_MINIVIEW_ITEMS = 15;

var loading = true;

function updateSimpleSection(id, section) {
  if (shownSections & section)
    $(id).classList.remove('hidden');
  else
    $(id).classList.add('hidden');
}

function recentlyClosedTabs(data) {
  logEvent('received recently closed tabs');
  // We need to store the recent items so we can update the layout on a resize.
  recentItems = data;
  renderRecentlyClosed();
  layoutSections();
}

var recentItems = [];

function renderRecentlyClosed() {
  // Remove all existing items and create new items.
  var recentElement = $('recently-closed');
  var parentEl = recentElement.lastElementChild;
  parentEl.textContent = '';

  recentItems.forEach(function(item) {
    parentEl.appendChild(createRecentItem(item));
  });

  layoutRecentlyClosed();
}

function createRecentItem(data) {
  var isWindow = data.type == 'window';
  var el;
  if (isWindow) {
    el = document.createElement('span');
    el.className = 'item link window';
    el.tabItems = data.tabs;
    el.tabIndex = 0;
    el.textContent = formatTabsText(data.tabs.length);
  } else {
    el = document.createElement('a');
    el.className = 'item';
    el.href = data.url;
    el.style.backgroundImage = url('chrome://favicon/' + data.url);
    el.dir = data.direction;
    el.textContent = data.title;
  }
  el.sessionId = data.sessionId;
  el.xtitle = data.title;
  var wrapperEl = document.createElement('span');
  wrapperEl.appendChild(el);
  return wrapperEl;
}

function saveShownSections() {
  chrome.send('setShownSections', [String(shownSections)]);
}

var LayoutMode = {
  SMALL: 1,
  NORMAL: 2
};

var layoutMode = useSmallGrid() ? LayoutMode.SMALL : LayoutMode.NORMAL;

function handleWindowResize() {
  if (window.innerWidth < 10) {
    // We're probably a background tab, so don't do anything.
    return;
  }

  var oldLayoutMode = layoutMode;
  var b = useSmallGrid();
  layoutMode = b ? LayoutMode.SMALL : LayoutMode.NORMAL

  if (layoutMode != oldLayoutMode){
    mostVisited.useSmallGrid = b;
    mostVisited.layout();
    renderRecentlyClosed();
  }

  layoutSections();
}

// Stores some information about each section necessary to layout. A new
// instance is constructed for each section on each layout.
function SectionLayoutInfo(section) {
  this.section = section;
  this.header = section.getElementsByTagName('h2')[0];
  this.miniview = section.getElementsByClassName('miniview')[0];
  this.maxiview = section.getElementsByClassName('maxiview')[0];
  this.expanded = !section.classList.contains('hidden');
  this.fixedHeight = this.header.offsetHeight;
  this.scrollingHeight = 0;

  if (this.expanded) {
    this.scrollingHeight = this.maxiview.offsetHeight;
  } else if (this.miniview) {
    this.fixedHeight += this.miniview.offsetHeight;
  }
}

// Get all sections to be layed out.
SectionLayoutInfo.getAll = function() {
  var sections = document.querySelectorAll('.section:not(.disabled)');
  var result = [];
  for (var i = 0, section; section = sections[i]; i++) {
    result.push(new SectionLayoutInfo(section));
  }
  return result;
};

// Layout the sections in a modified accordian. The header and miniview, if
// visible are fixed within the viewport. If there is an expanded section, its
// it scrolls.
//
// =============================
// | collapsed section         |  <- Any collapsed sections are fixed position.
// | and miniview              |
// |---------------------------|
// | expanded section          |
// |                           |  <- There can be one expanded section and it
// | and maxiview              |     is absolutely positioned so that it can
// |                           |     scroll "underneath" the fixed elements.
// |                           |
// |---------------------------|
// | another collapsed section |
// |---------------------------|
//
// We want the main frame scrollbar to be the one that scrolls the expanded
// region. To get this effect, we make the fixed elements position:fixed and the
// scrollable element position:absolute. We also artificially increase the
// height of the document so that it is possible to scroll down enough to
// display the end of the document, even with any fixed elements at the bottom
// of the viewport.
//
// There is a final twist: If the intrinsic height of the expanded section is
// less than the available height (because the window is tall), any collapsed
// sections sinch up and sit below the expanded section. This is so that we
// don't have a bunch of dead whitespace in the case of expanded sections that
// aren't very tall.
function layoutSections() {
  var sections = SectionLayoutInfo.getAll();
  var expandedSection = null;
  var headerHeight = 0;
  var footerHeight = 0;

  // Calculate the height of the fixed elements above the expanded section. Also
  // take note of the expanded section, if there is one.
  var i;
  var section;
  for (i = 0; section = sections[i]; i++) {
    headerHeight += section.fixedHeight;
    if (section.expanded) {
      expandedSection = section;
      i++;
      break;
    }
  }

  // Calculate the height of the fixed elements below the expanded section, if
  // any.
  for (; section = sections[i]; i++) {
    footerHeight += section.fixedHeight;
  }

  // Determine the height to use for the expanded section. If there isn't enough
  // space to show the expanded section completely, this will be the available
  // height. Otherwise, we use the intrinsic height of the expanded section.
  var expandedSectionHeight;
  if (expandedSection) {
    var flexHeight = window.innerHeight - headerHeight - footerHeight;
    if (flexHeight < expandedSection.scrollingHeight) {
      expandedSectionHeight = flexHeight;

      // Also, artificially expand the height of the document so that we can see
      // the entire expanded section.
      //
      // TODO(aa): Where does this come from? It is the difference between what
      // we set document.body.style.height to and what
      // document.body.scrollHeight measures afterward. I expect them to be the
      // same if document.body has no margins.
      var fudge = 44;
      document.body.style.height =
          headerHeight +
          expandedSection.scrollingHeight +
          footerHeight +
          fudge +
          'px';
    } else {
      expandedSectionHeight = expandedSection.scrollingHeight;
      document.body.style.height = '';
    }
  }

  // Now position all the elements.
  var y = 0;
  for (i = 0, section; section = sections[i]; i++) {
    section.header.style.top = y + 'px';
    y += section.header.offsetHeight;

    if (section.miniview) {
      section.miniview.style.top = y + 'px';
      if (section != expandedSection) {
        y += section.miniview.offsetHeight;
      }
    }

    if (section.maxiview) {
      section.maxiview.style.top = y + 'px';
      if (section == expandedSection) {
        y += expandedSectionHeight;
      }
    }
  }
}

window.addEventListener('resize', handleWindowResize);

var sectionToElementMap;
function getSectionElement(section) {
  if (!sectionToElementMap) {
    sectionToElementMap = {};
    for (var key in Section) {
      sectionToElementMap[Section[key]] =
          document.querySelector('.section[section=' + key + ']');
    }
  }
  return sectionToElementMap[section];
}

function showSection(section) {
  if (!(section & shownSections)) {
    shownSections |= section;
    var el = getSectionElement(section);
    if (el)
      el.classList.remove('hidden');

    switch (section) {
      case Section.THUMB:
        mostVisited.visible = true;
        mostVisited.layout();
        break;
    }
  }
}

function hideSection(section) {
  if (section & shownSections) {
    shownSections &= ~section;

    switch (section) {
      case Section.THUMB:
        mostVisited.visible = false;
        mostVisited.layout();
        break;
    }

    var el = getSectionElement(section);
    if (el)
      el.classList.add('hidden');
  }
}

/**
 * Callback when the shown sections changes in another NTP.
 * @param {number} newShownSections Bitmask of the shown sections.
 */
function setShownSections(newShownSections) {
  for (var key in Section) {
    if (newShownSections & Section[key])
      showSection(Section[key]);
    else
      hideSection(Section[key]);
  }
  layoutSections();
}

// Recently closed

function layoutRecentlyClosed() {
  var recentElement = $('recently-closed');
  // We cannot use clientWidth here since the width has a transition.
  var availWidth = useSmallGrid() ? 692 : 920;
  var parentEl = recentElement.lastElementChild;

  // Now go backwards and hide as many elements as needed.
  var elementsToHide = [];
  for (var el = parentEl.lastElementChild; el;
       el = el.previousElementSibling) {
    if (el.offsetLeft + el.offsetWidth > availWidth) {
      elementsToHide.push(el);
    }
  }

  elementsToHide.forEach(function(el) {
    parentEl.removeChild(el);
  });

  if (parentEl.hasChildNodes()) {
    recentElement.classList.remove('disabled');
  } else {
    recentElement.classList.add('disabled');
  }
}

/**
 * This function is called by the backend whenever the sync status section
 * needs to be updated to reflect recent sync state changes. The backend passes
 * the new status information in the newMessage parameter. The state includes
 * the following:
 *
 * syncsectionisvisible: true if the sync section needs to show up on the new
 *                       tab page and false otherwise.
 * title: the header for the sync status section.
 * msg: the actual message (e.g. "Synced to foo@gmail.com").
 * linkisvisible: true if the link element should be visible within the sync
 *                section and false otherwise.
 * linktext: the text to display as the link in the sync status (only used if
 *           linkisvisible is true).
 * linkurlisset: true if an URL should be set as the href for the link and false
 *               otherwise. If this field is false, then clicking on the link
 *               will result in sending a message to the backend (see
 *               'SyncLinkClicked').
 * linkurl: the URL to use as the element's href (only used if linkurlisset is
 *          true).
 */
function syncMessageChanged(newMessage) {
  var syncStatusElement = $('sync-status');
  var style = syncStatusElement.style;

  // Hide the section if the message is emtpy.
  if (!newMessage['syncsectionisvisible']) {
    style.display = 'none';
    return;
  }
  style.display = 'block';

  // Set the sync section background color based on the state.
  if (newMessage.msgtype == 'error') {
    style.backgroundColor = 'tomato';
  } else {
    style.backgroundColor = '';
  }

  // Set the text for the header and sync message.
  var titleElement = syncStatusElement.firstElementChild;
  titleElement.textContent = newMessage.title;
  var messageElement = titleElement.nextElementSibling;
  messageElement.textContent = newMessage.msg;

  // Remove what comes after the message
  while (messageElement.nextSibling) {
    syncStatusElement.removeChild(messageElement.nextSibling);
  }

  if (newMessage.linkisvisible) {
    var el;
    if (newMessage.linkurlisset) {
      // Use a link
      el = document.createElement('a');
      el.href = newMessage.linkurl;
    } else {
      el = document.createElement('button');
      el.className = 'link';
      el.addEventListener('click', syncSectionLinkClicked);
    }
    el.textContent = newMessage.linktext;
    syncStatusElement.appendChild(el);
    fixLinkUnderline(el);
  }
}

/**
 * Invoked when the link in the sync status section is clicked.
 */
function syncSectionLinkClicked(e) {
  chrome.send('SyncLinkClicked');
  e.preventDefault();
}

/**
 * Invoked when link to start sync in the promo message is clicked, and Chrome
 * has already been synced to an account.
 */
function syncAlreadyEnabled(message) {
  showNotification(message.syncEnabledMessage,
                   localStrings.getString('close'));
}

/**
 * Returns the text used for a recently closed window.
 * @param {number} numTabs Number of tabs in the window.
 * @return {string} The text to use.
 */
function formatTabsText(numTabs) {
  if (numTabs == 1)
    return localStrings.getString('closedwindowsingle');
  return localStrings.getStringF('closedwindowmultiple', numTabs);
}

// Theme related

function themeChanged() {
  $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
  updateAttribution();
}

function updateAttribution() {
  $('attribution-img').src = 'chrome://theme/IDR_THEME_NTP_ATTRIBUTION?' +
      Date.now();
}

function bookmarkBarAttached() {
  document.documentElement.setAttribute('bookmarkbarattached', 'true');
}

function bookmarkBarDetached() {
  document.documentElement.setAttribute('bookmarkbarattached', 'false');
}

function viewLog() {
  var lines = [];
  var start = log[0][1];

  for (var i = 0; i < log.length; i++) {
    lines.push((log[i][1] - start) + ': ' + log[i][0]);
  }

  console.log(lines.join('\n'));
}

// We apply the size class here so that we don't trigger layout animations
// onload.

handleWindowResize();

var localStrings = new LocalStrings();

///////////////////////////////////////////////////////////////////////////////
// Things we know are not needed at startup go below here

function afterTransition(f) {
  if (loading) {
    // Make sure we do not use a timer during load since it slows down the UI.
    f();
  } else {
    // The duration of all transitions are .15s
    window.setTimeout(f, 150);
  }
}

// Notification


var notificationTimeout;

function showNotification(text, actionText, opt_f, opt_delay) {
  var notificationElement = $('notification');
  var f = opt_f || function() {};
  var delay = opt_delay || 10000;

  function show() {
    window.clearTimeout(notificationTimeout);
    notificationElement.classList.add('show');
    document.body.classList.add('notification-shown');
  }

  function delayedHide() {
    notificationTimeout = window.setTimeout(hideNotification, delay);
  }

  function doAction() {
    f();
    hideNotification();
  }

  // Remove any possible first-run trails.
  notification.classList.remove('first-run');

  var actionLink = notificationElement.querySelector('.link-color');
  notificationElement.firstElementChild.textContent = text;
  actionLink.textContent = actionText;

  actionLink.onclick = doAction;
  actionLink.onkeydown = handleIfEnterKey(doAction);
  notificationElement.onmouseover = show;
  notificationElement.onmouseout = delayedHide;
  actionLink.onfocus = show;
  actionLink.onblur = delayedHide;
  // Enable tabbing to the link now that it is shown.
  actionLink.tabIndex = 0;

  show();
  delayedHide();
}

/**
 * Hides the notifier.
 */
function hideNotification() {
  var notificationElement = $('notification');
  notificationElement.classList.remove('show');
  document.body.classList.remove('notification-shown');
  var actionLink = notificationElement.querySelector('.link-color');
  // Prevent tabbing to the hidden link.
  actionLink.tabIndex = -1;
  // Setting tabIndex to -1 only prevents future tabbing to it. If, however, the
  // user switches window or a tab and then moves back to this tab the element
  // may gain focus. We therefore make sure that we blur the element so that the
  // element focus is not restored when coming back to this window.
  actionLink.blur();
}

function showFirstRunNotification() {
  showNotification(localStrings.getString('firstrunnotification'),
                   localStrings.getString('closefirstrunnotification'),
                   null, 30000);
  var notificationElement = $('notification');
  notification.classList.add('first-run');
}

/**
 * This handles the option menu.
 * @param {Element} button The button element.
 * @param {Element} menu The menu element.
 * @constructor
 */
function OptionMenu(button, menu) {
  this.button = button;
  this.menu = menu;
  this.button.onclick = bind(this.handleClick, this);
  this.button.onmousedown = bind(this.handleMouseDown, this);
  this.button.onkeydown = bind(this.handleKeyDown, this);
  this.boundHideMenu_ = bind(this.hide, this);
  this.boundMaybeHide_ = bind(this.maybeHide_, this);
  this.menu.onmouseover = bind(this.handleMouseOver, this);
  this.menu.onmouseout = bind(this.handleMouseOut, this);
  this.menu.onmouseup = bind(this.handleMouseUp, this);
}

OptionMenu.prototype = {
  show: function() {
    this.positionMenu_();
    this.menu.style.display = 'block';
    this.button.classList.add('open');
    this.button.focus();

    // Listen to document and window events so that we hide the menu when the
    // user clicks outside the menu or tabs away or the whole window is blurred.
    document.addEventListener('focus', this.boundMaybeHide_, true);
    document.addEventListener('mousedown', this.boundMaybeHide_, true);
  },

  positionMenu_: function() {
    var rect = this.button.getBoundingClientRect();
    this.menu.style.top = rect.bottom + 'px';
    this.menu.style.right = (document.body.clientWidth - rect.right) + 'px'
  },

  hide: function() {
    this.menu.style.display = 'none';
    this.button.classList.remove('open');
    this.setSelectedIndex(-1);

    document.removeEventListener('focus', this.boundMaybeHide_, true);
    document.removeEventListener('mousedown', this.boundMaybeHide_, true);
  },

  isShown: function() {
    return this.menu.style.display == 'block';
  },

  /**
   * Callback for document mousedown and focus. It checks if the user tried to
   * navigate to a different element on the page and if so hides the menu.
   * @param {Event} e The mouse or focus event.
   * @private
   */
  maybeHide_: function(e) {
    if (!this.menu.contains(e.target) && !this.button.contains(e.target)) {
      this.hide();
    }
  },

  handleMouseDown: function(e) {
    if (this.isShown()) {
      this.hide();
    } else {
      this.show();
    }
  },

  handleClick: function(e) {
    e.stopPropagation();
  },

  handleMouseOver: function(e) {
    var el = e.target;
    if (!el.hasAttribute('command')) {
      this.setSelectedIndex(-1);
    } else {
      var index = Array.prototype.indexOf.call(this.menu.children, el);
      this.setSelectedIndex(index);
    }
  },

  handleMouseOut: function(e) {
    this.setSelectedIndex(-1);
  },

  handleMouseUp: function(e) {
    var item = this.getSelectedItem();
    if (item) {
      this.executeItem(item);
    }
  },

  handleKeyDown: function(e) {
    var item = this.getSelectedItem();

    var self = this;
    function selectNextVisible(m) {
      var children = self.menu.children;
      var len = children.length;
      var i = self.selectedIndex_;
      if (i == -1 && m == -1) {
        // Edge case when we need to go the last item fisrt.
        i = 0;
      }
      while (true) {
        i = (i + m + len) % len;
        item = children[i];
        if (item && item.hasAttribute('command') &&
            item.style.display != 'none') {
          break;
        }
      }
      if (item) {
        self.setSelectedIndex(i);
      }
    }

    switch (e.keyIdentifier) {
      case 'Down':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(1);
        e.preventDefault();
        break;
      case 'Up':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(-1);
        e.preventDefault();
        break;
      case 'Esc':
      case 'U+001B': // Maybe this is remote desktop playing a prank?
        this.hide();
        break;
      case 'Enter':
      case 'U+0020': // Space
        if (this.isShown()) {
          if (item) {
            this.executeItem(item);
          } else {
            this.hide();
          }
        } else {
          this.show();
        }
        e.preventDefault();
        break;
    }
  },

  selectedIndex_: -1,
  setSelectedIndex: function(i) {
    if (i != this.selectedIndex_) {
      var items = this.menu.children;
      var oldItem = items[this.selectedIndex_];
      if (oldItem) {
        oldItem.removeAttribute('selected');
      }
      var newItem = items[i];
      if (newItem) {
        newItem.setAttribute('selected', 'selected');
      }
      this.selectedIndex_ = i;
    }
  },

  getSelectedItem: function() {
    return this.menu.children[this.selectedIndex_] || null;
  },

  executeItem: function(item) {
    var command = item.getAttribute('command');
    if (command in this.commands) {
      this.commands[command].call(this, item);
    }

    this.hide();
  }
};

var optionMenu = new OptionMenu(
    document.querySelector('#most-visited-section h2 .settings'),
    $('option-menu'));
optionMenu.commands = {
  'clear-all-blacklisted' : function() {
    mostVisited.clearAllBlacklisted();
    chrome.send('getMostVisited');
  }
};

$('main').addEventListener('click', function(e) {
  var p = e.target;
  while (p && p.tagName != 'H2') {
    p = p.parentNode;
  }

  if (!p) {
    return;
  }

  p = p.parentNode;
  if (p.noexpand) {
    return;
  }

  var section = p.getAttribute('section');
  if (section) {
    if (shownSections & Section[section]) {
      hideSection(Section[section]);
    } else {
      for (var p in Section) {
        if (p == section)
          showSection(Section[p]);
        else
          hideSection(Section[p]);
      }
    }
    layoutSections();
    saveShownSections();
  }
});

function handleIfEnterKey(f) {
  return function(e) {
    if (e.keyIdentifier == 'Enter') {
      f(e);
    }
  };
}

function maybeReopenTab(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionId !== undefined;
  });
  if (el) {
    chrome.send('reopenTab', [String(el.sessionId)]);
    e.preventDefault();

    // HACK(arv): After the window onblur event happens we get a mouseover event
    // on the next item and we want to make sure that we do not show a tooltip
    // for that.
    window.setTimeout(function() {
      windowTooltip.hide();
    }, 2 * WindowTooltip.DELAY);
  }
}

function maybeShowWindowTooltip(e) {
  var f = function(el) {
    return el.tabItems !== undefined;
  };
  var el = findAncestor(e.target, f);
  var relatedEl = findAncestor(e.relatedTarget, f);
  if (el && el != relatedEl) {
    windowTooltip.handleMouseOver(e, el, el.tabItems);
  }
}


var recentlyClosedElement = $('recently-closed');

recentlyClosedElement.addEventListener('click', maybeReopenTab);
recentlyClosedElement.addEventListener('keydown',
                                       handleIfEnterKey(maybeReopenTab));

recentlyClosedElement.addEventListener('mouseover', maybeShowWindowTooltip);
recentlyClosedElement.addEventListener('focus', maybeShowWindowTooltip, true);

/**
 * This object represents a tooltip representing a closed window. It is
 * shown when hovering over a closed window item or when the item is focused. It
 * gets hidden when blurred or when mousing out of the menu or the item.
 * @param {Element} tooltipEl The element to use as the tooltip.
 * @constructor
 */
function WindowTooltip(tooltipEl) {
  this.tooltipEl = tooltipEl;
  this.boundHide_ = bind(this.hide, this);
  this.boundHandleMouseOut_ = bind(this.handleMouseOut, this);
}

WindowTooltip.trackMouseMove_ = function(e) {
  WindowTooltip.clientX = e.clientX;
  WindowTooltip.clientY = e.clientY;
};

/**
 * Time in ms to delay before the tooltip is shown.
 * @type {number}
 */
WindowTooltip.DELAY = 300;

WindowTooltip.prototype = {
  timer: 0,
  handleMouseOver: function(e, linkEl, tabs) {
    this.linkEl_ = linkEl;
    if (e.type == 'mouseover') {
      this.linkEl_.addEventListener('mousemove', WindowTooltip.trackMouseMove_);
      this.linkEl_.addEventListener('mouseout', this.boundHandleMouseOut_);
    } else { // focus
      this.linkEl_.addEventListener('blur', this.boundHide_);
    }
    this.timer = window.setTimeout(bind(this.show, this, e.type, linkEl, tabs),
                                   WindowTooltip.DELAY);
  },
  show: function(type, linkEl, tabs) {
    window.addEventListener('blur', this.boundHide_);
    this.linkEl_.removeEventListener('mousemove',
                                     WindowTooltip.trackMouseMove_);
    window.clearTimeout(this.timer);

    this.renderItems(tabs);
    var rect = linkEl.getBoundingClientRect();
    var bodyRect = document.body.getBoundingClientRect();
    var rtl = document.documentElement.dir == 'rtl';

    this.tooltipEl.style.display = 'block';
    var tooltipRect = this.tooltipEl.getBoundingClientRect();
    var x, y;

    // When focused show below, like a drop down menu.
    if (type == 'focus') {
      x = rtl ?
          rect.left + bodyRect.left + rect.width - this.tooltipEl.offsetWidth :
          rect.left + bodyRect.left;
      y = rect.top + bodyRect.top + rect.height;
    } else {
      x = bodyRect.left + (rtl ?
          WindowTooltip.clientX - this.tooltipEl.offsetWidth :
          WindowTooltip.clientX);
      // Offset like a tooltip
      y = 20 + WindowTooltip.clientY + bodyRect.top;
    }

    // We need to ensure that the tooltip is inside the window viewport.
    x = Math.min(x, bodyRect.width - tooltipRect.width);
    x = Math.max(x, 0);
    y = Math.min(y, bodyRect.height - tooltipRect.height);
    y = Math.max(y, 0);

    this.tooltipEl.style.left = x + 'px';
    this.tooltipEl.style.top = y + 'px';
  },
  handleMouseOut: function(e) {
    // Don't hide when move to another item in the link.
    var f = function(el) {
      return el.tabItems !== undefined;
    };
    var el = findAncestor(e.target, f);
    var relatedEl = findAncestor(e.relatedTarget, f);
    if (el && el != relatedEl) {
      this.hide();
    }
  },
  hide: function() {
    window.clearTimeout(this.timer);
    window.removeEventListener('blur', this.boundHide_);
    this.linkEl_.removeEventListener('mousemove',
                                     WindowTooltip.trackMouseMove_);
    this.linkEl_.removeEventListener('mouseout', this.boundHandleMouseOut_);
    this.linkEl_.removeEventListener('blur', this.boundHide_);
    this.linkEl_ = null;

    this.tooltipEl.style.display  = 'none';
  },
  renderItems: function(tabs) {
    var tooltip = this.tooltipEl;
    tooltip.textContent = '';

    tabs.forEach(function(tab) {
      var span = document.createElement('span');
      span.className = 'item';
      span.style.backgroundImage = url('chrome://favicon/' + tab.url);
      span.dir = tab.direction;
      span.textContent = tab.title;
      tooltip.appendChild(span);
    });
  }
};

var windowTooltip = new WindowTooltip($('window-tooltip'));

window.addEventListener('load', bind(logEvent, global, 'Tab.NewTabOnload',
                                     true));

window.addEventListener('resize', handleWindowResize);
document.addEventListener('DOMContentLoaded',
    bind(logEvent, global, 'Tab.NewTabDOMContentLoaded', true));

// Whether or not we should send the initial 'GetSyncMessage' to the backend
// depends on the value of the attribue 'syncispresent' which the backend sets
// to indicate if there is code in the backend which is capable of processing
// this message. This attribute is loaded by the JSTemplate and therefore we
// must make sure we check the attribute after the DOM is loaded.
document.addEventListener('DOMContentLoaded',
                          callGetSyncMessageIfSyncIsPresent);

/**
 * The sync code is not yet built by default on all platforms so we have to
 * make sure we don't send the initial sync message to the backend unless the
 * backend told us that the sync code is present.
 */
function callGetSyncMessageIfSyncIsPresent() {
  if (document.documentElement.getAttribute('syncispresent') == 'true') {
    chrome.send('GetSyncMessage');
  }
}

function hideAllMenus() {
  optionMenu.hide();
}

window.addEventListener('blur', hideAllMenus);
window.addEventListener('keydown', function(e) {
  if (e.keyIdentifier == 'Alt' || e.keyIdentifier == 'Meta') {
    hideAllMenus();
  }
}, true);

// Tooltip for elements that have text that overflows.
document.addEventListener('mouseover', function(e) {
  // We don't want to do this while we are dragging because it makes things very
  // janky
  if (mostVisited.isDragging()) {
    return;
  }

  var el = findAncestor(e.target, function(el) {
    return el.xtitle;
  });
  if (el && el.xtitle != el.title) {
    if (el.scrollWidth > el.clientWidth) {
      el.title = el.xtitle;
    } else {
      el.title = '';
    }
  }
});

/**
 * Makes links and buttons support a different underline color.
 * @param {Node} node The node to search for links and buttons in.
 */
function fixLinkUnderlines(node) {
  var elements = node.querySelectorAll('a,button');
  Array.prototype.forEach.call(elements, fixLinkUnderline);
}

/**
 * Wraps the content of an element in a a link-color span.
 * @param {Element} el The element to wrap.
 */
function fixLinkUnderline(el) {
  var span = document.createElement('span');
  span.className = 'link-color';
  while (el.hasChildNodes()) {
    span.appendChild(el.firstChild);
  }
  el.appendChild(span);
}

updateAttribution();

var mostVisited = new MostVisited(
    $('most-visited'),
    document.querySelector('#most-visited-section .miniview'),
    useSmallGrid(),
    shownSections & Section.THUMB);

function mostVisitedPages(data, firstRun) {
  logEvent('received most visited pages');

  mostVisited.data = data;
  mostVisited.layout();
  layoutSections();

  loading = false;

  // Remove class name in a timeout so that changes done in this JS thread are
  // not animated.
  window.setTimeout(function() {
    mostVisited.ensureSmallGridCorrect();
    document.body.classList.remove('loading');
  }, 1);

  // Only show the first run notification if first run.
  if (firstRun) {
    showFirstRunNotification();
  }
}
