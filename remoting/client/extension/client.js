// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BAD_AUTH_TOKEN = 'bad_token';

function init() {
  updateLoginStatus();

  // Defer getting the host list for a little bit so that we don't
  // block the display of the extension popup.
  window.setTimeout(listHosts, 100);
}

// Update the login status region (at the bottom of the popup) with the
// current account and links to sign in/out.
function updateLoginStatus() {
  var username = getCookie('username');

  var loginDiv = document.getElementById('login_div');
  clear(loginDiv);

  if (!username) {
    var signinLink = document.createElement('a');
    signinLink.setAttribute('href',
        "javascript:window.open('login.html', 'Sign In', " +
        "'width=400,height=200,scrollbars=no'); return false;");
    signinLink.appendChild(document.createTextNode('Sign In'));
    loginDiv.appendChild(signinLink);
  } else {
    var email = document.createElement('span');
    email.setAttribute('class', 'login_email');
    email.appendChild(document.createTextNode(username));
    loginDiv.appendChild(email);

    loginDiv.appendChild(document.createTextNode(' | '));

    var signoutLink = document.createElement('a');
    signoutLink.setAttribute('href', 'javascript:logout(this.form);');
    signoutLink.appendChild(document.createTextNode('Sign Out'));
    loginDiv.appendChild(signoutLink);
  }
}

// Sign out the current user by erasing the auth cookies.
function logout(form) {
  setCookie('username', '', 100);
  setCookie('chromoting_auth', '', 100);
  setCookie('xmpp_auth', '', 100);

  updateLoginStatus();
  listHosts();
}

function login(form) {
  var status = document.getElementById('login_status');
  clear(status);
  doLogin(form.username.value, form.password.value, checkLogin);
}

// Check to see if the login was successful.
function checkLogin() {
  var username = getCookie('username');
  var cauth = getCookie('chromoting_auth');
  var xauth = getCookie('xmpp_auth');

  // Verify login and show login status.
  var status = document.getElementById('login_status');
  if (cauth == BAD_AUTH_TOKEN || xauth == BAD_AUTH_TOKEN) {
    appendMessage(status, '', 'Sign in failed!');
    if (username) {
      setCookie('username', '', 100);
    }
  } else {
    appendMessage(status, '', 'Successfully signed in as ' + username);
  }
}

function doLogin(username, password, done) {
  // Don't call |done| callback until both login requests have completed.
  var count = 2;
  var barrier = function() {
    count--;
    if (done && count == 0) {
      done();
    }
  }
  setCookie('username', username, 100);
  doGaiaLogin(username, password, 'chromoting',
              function(cAuthToken) {
                setCookie('chromoting_auth', cAuthToken, 100);
                barrier();
              });
  doGaiaLogin(username, password, 'chromiumsync',
              function(xAuthToken) {
                setCookie('xmpp_auth', xAuthToken, 100);
                barrier();
              });
}

function doGaiaLogin(username, password, service, done) {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', 'https://www.google.com/accounts/ClientLogin', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status = 200) {
      done(extractAuthToken(xhr.responseText));
    } else {
      console.log('Bad status on auth: ' + xhr.statusText);
    }
  };

  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xhr.send('accountType=HOSTED_OR_GOOGLE&Email=' + username + '&Passwd=' +
           password + '&service=' + service + '&source=chromoclient');
}

function extractAuthToken(message) {
  var lines = message.split('\n');
  for (var i = 0; i < lines.length; i++) {
    if (lines[i].match('^Auth=.*')) {
      return lines[i].split('=')[1];
    }
  }

  console.log('Could not parse auth token in : "' + message + '"');
  return BAD_AUTH_TOKEN;
}

// Open a chromoting connection in a new tab.
function openChromotingTab(hostName, hostJid) {
  var background = chrome.extension.getBackgroundPage();
  background.openChromotingTab(hostName, hostJid);
}

// Erase the content of the specified element.
function clear(e) {
  e.innerHTML = '';
}

// Clear out the specified element and show the message to the user.
function displayMessage(e, classname, message) {
  clear(e);
  appendMessage(e, classname, message);
}

// Append the message text to the specified element.
function appendMessage(e, classname, message) {
  var p = document.createElement('p');
  if (classname.length != 0) {
    p.setAttribute('class', classname);
  }

  p.appendChild(document.createTextNode(message));

  e.appendChild(p);
}

function listHosts() {
  var username = getCookie('username');

  var hostlistDiv = document.getElementById('hostlist_div');
  if (!username) {
    displayMessage(hostlistDiv, 'message',
                   'Please sign in to see a list of available hosts.');
    return;
  }

  var xhr = new XMLHttpRequest();
  var token = getCookie('chromoting_auth');

  // Unhide host list.
  hostlistDiv.style.display = "block";

  xhr.onreadystatechange = function() {
    if (xhr.readyState == 1) {
      displayMessage(hostlistDiv, 'message', 'Loading host list for ' +
                     username);
    }
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      var parsed_response = JSON.parse(xhr.responseText);
      appendHostLinks(parsed_response.data.items);
    } else {
      var errorResponse = JSON.parse(xhr.responseText);

      console.log('Error: Bad status on host list query: "' +
                  xhr.status + ' ' + xhr.statusText);
      console.log('Error code ' + errorResponse.error.code);
      console.log('Error message ' + errorResponse.error.message);

      clear(hostlistDiv);
      appendMessage(hostlistDiv, 'message',
                    'Unable to load host list for ' + username + '. ' +
                    'Please try again later.');
      appendMessage(hostlistDiv, 'message',
                    'Error code: ' + errorResponse.error.code);
      appendMessage(hostlistDiv, 'message',
                    'Message: ' + errorResponse.error.message);
    }
  };

  xhr.open('GET', 'https://www.googleapis.com/chromoting/v1/@me/hosts');
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.setRequestHeader('Authorization', 'GoogleLogin auth=' + token);
  xhr.send(null);
}

// Populate the 'hostlist_div' element with the list of hosts for this user.
function appendHostLinks(hostlist) {
  var hostlistDiv = document.getElementById('hostlist_div');

  // Clear the div before adding the host info.
  clear(hostlistDiv);

  // Add the hosts.
  // TODO(garykac): We should have some sort of MRU list here.
  // First, add all of the connected hosts.
  for (var i = 0; i < hostlist.length; ++i) {
    if (hostlist[i].status == "ONLINE") {
      hostlistDiv.appendChild(addHostInfo(hostlist[i]));
    }
  }
  // Add non-online hosts at the end.
  for (var i = 0; i < hostlist.length; ++i) {
    if (hostlist[i].status != "ONLINE") {
      hostlistDiv.appendChild(addHostInfo(hostlist[i]));
    }
  }
}

// Create a single host description element.
function addHostInfo(host) {
  var hostEntry = document.createElement('div');
  hostEntry.setAttribute('class', 'hostentry');

  var hostIcon = document.createElement('img');
  hostIcon.setAttribute('src', 'machine.png');
  hostIcon.setAttribute('class', 'hosticon');
  hostEntry.appendChild(hostIcon);

  var span = document.createElement('span');
  span.setAttribute('class', 'connect');
  var connect = document.createElement('input');
  connect.setAttribute('type', 'button');
  connect.setAttribute('value', 'Connect');
  connect.setAttribute('onclick', "openChromotingTab('" + host.hostName +
                       "', '" + host.jabberId + "'); return false;");
  span.appendChild(connect);
  hostEntry.appendChild(span);

  var hostName = document.createElement('p');
  hostName.setAttribute('class', 'hostname');
  hostName.appendChild(document.createTextNode(host.hostName));
  hostEntry.appendChild(hostName);

  var hostStatus = document.createElement('p');
  hostStatus.setAttribute('class', 'hostinfo hoststatus_' +
                          ((host.status == 'ONLINE') ? 'good' : 'bad'));
  hostStatus.appendChild(document.createTextNode(host.status));
  hostEntry.appendChild(hostStatus);

  var hostInfo = document.createElement('p');
  hostInfo.setAttribute('class', 'hostinfo');
  hostInfo.appendChild(document.createTextNode(host.jabberId));
  hostEntry.appendChild(hostInfo);

  return hostEntry;
}
