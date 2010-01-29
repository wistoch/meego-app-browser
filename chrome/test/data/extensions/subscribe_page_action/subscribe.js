// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Grab the querystring, removing question mark at the front and splitting on
// the ampersand.
var queryString = location.search.substring(1).split("&");

// The feed URL is the first component and always present.
var feedUrl = decodeURIComponent(queryString[0]);

// We allow synchronous requests for testing. This component is only present
// if true.
var synchronousRequest = queryString[1] == "synchronous";

// The XMLHttpRequest object that tries to load and parse the feed, and (if
// testing) also the style sheet and the frame js.
var req;

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the css file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var styleSheet = "";

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the js file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var frameScript = "";

// What to show when we cannot parse the feed name.
var unknownName = "Unknown feed name";

// A list of feed readers, populated by localStorage if available, otherwise
// hard coded.
var feedReaderList;

// Navigates to the reader of the user's choice (for subscribing to the feed).
function navigate() {
  var select = document.getElementById('readerDropdown');
  var url =
      feedReaderList[select.selectedIndex].url.replace(
          "%s", escape(encodeURI(feedUrl)));

  // Before we navigate, see if we want to skip this step in the future...
  if (storageEnabled) {
    // See if the user wants to always use this reader.
    var alwaysUse = document.getElementById('alwaysUse');
    if (alwaysUse.checked) {
      window.localStorage.defaultReader =
          feedReaderList[select.selectedIndex].url;
      window.localStorage.showPreviewPage = "No";
    }
  }

  document.location = url;
}

/**
* The main function. Sets up the selection list for possible readers and
* fetches the data.
*/
function main() {
  if (storageEnabled && window.localStorage.readerList)
      feedReaderList = JSON.parse(window.localStorage.readerList);
  if (!feedReaderList)
    feedReaderList = defaultReaderList();

  // Populate the list of readers.
  var readerDropdown = document.getElementById('readerDropdown');
  for (i = 0; i < feedReaderList.length; ++i) {
    readerDropdown.options[i] = new Option(feedReaderList[i].description, i);
    if (storageEnabled && isDefaultReader(feedReaderList[i].url))
      readerDropdown.selectedIndex = i;
  }

  if (storageEnabled) {
    // Add the "Manage..." entry to the dropdown and show the checkbox asking
    // if we always want to use this reader in the future (skip the preview).
    readerDropdown.options[i] = new Option("Manage...", "");
    document.getElementById('alwaysUseSpan').style.display = "block";
  }

  // Now fetch the data.
  req = new XMLHttpRequest();
  if (synchronousRequest) {
    // Tests that load the html page directly through a file:// url don't have
    // access to the js and css from the frame so we must load them first and
    // inject them into the src for the iframe.
    req.open("GET", "style.css", false);
    req.send(null);

    styleSheet = "<style>" + req.responseText + "</style>";

    req.open("GET", "iframe.js", false);
    req.send(null);

    frameScript = "<script>" + req.responseText +
                    "<" + "/script>";
  } else {
    // Normal loading just requires links to the css and the js file.
    styleSheet = "<link rel='stylesheet' type='text/css' href='" +
                    chrome.extension.getURL("style.css") + "'>";
    frameScript = "<script src='" + chrome.extension.getURL("iframe.js") +
                    "'></" + "script>";
  }

  feedUrl = decodeURIComponent(feedUrl);
  req.onload = handleResponse;
  req.onerror = handleError;
  // Not everyone sets the mime type correctly, which causes handleResponse
  // to fail to XML parse the response text from the server. By forcing
  // it to text/xml we avoid this.
  req.overrideMimeType('text/xml');
  req.open("GET", feedUrl, !synchronousRequest);
  req.send(null);

  document.getElementById('feedUrl').href = 'view-source:' + feedUrl;
}

// Sets the title for the feed.
function setFeedTitle(title) {
  var titleTag = document.getElementById('title');
  titleTag.textContent = "Feed for '" + title + "'";
}

// Handles errors during the XMLHttpRequest.
function handleError() {
  handleFeedParsingFailed("Error fetching feed");
}

// Handles feed parsing errors.
function handleFeedParsingFailed(error) {
  setFeedTitle(unknownName);

  // The tests always expect an IFRAME, so add one showing the error.
  var html = "<body><span id=\"error\" class=\"item_desc\">" + error +
               "</span></body>";

  var error_frame = createFrame('error', html);
  var itemsTag = document.getElementById('items');
  itemsTag.appendChild(error_frame);
}

function createFrame(frame_id, html) {
  frame = document.createElement('iframe');
  frame.id = frame_id;
  frame.src = "data:text/html;charset=utf-8,<html>" + styleSheet + html +
                "</html>";
  frame.scrolling = "auto";
  frame.frameBorder = "0";
  frame.marginWidth = "0";
  return frame;
}

function embedAsIframe(rssText) {
  var itemsTag = document.getElementById('items');

  // TODO(aa): Add base URL tag
  iframe = createFrame('rss', styleSheet + frameScript);
  itemsTag.appendChild(iframe);

  iframe.onload = function() {
    iframe.contentWindow.postMessage(rssText, "*");
  }
}

// Handles parsing the feed data we got back from XMLHttpRequest.
function handleResponse() {
  // Uncomment these three lines to see what the feed data looks like.
  // var itemsTag = document.getElementById('items');
  // itemsTag.textContent = req.responseText;
  // return;

  var doc = req.responseXML;
  if (!doc) {
    handleFeedParsingFailed("Not a valid feed.");
    return;
  }

  // We must find at least one 'entry' or 'item' element before proceeding.
  var entries = doc.getElementsByTagName('entry');
  if (entries.length == 0)
    entries = doc.getElementsByTagName('item');
  if (entries.length == 0) {
    handleFeedParsingFailed("This feed contains no entries.")
    return;
  }

  // Figure out what the title of the whole feed is.
  var title = doc.getElementsByTagName('title')[0];
  if (title)
    setFeedTitle(title.textContent);
  else
    setFeedTitle(unknownName);

  // Add an IFRAME with the html contents.
  embedAsIframe(req.responseText);
}

/**
* Handler for when selection changes.
*/
function onSelectChanged() {
  if (!storageEnabled)
    return;
  var readerDropdown = document.getElementById('readerDropdown');

  // If the last item (Manage...) was selected we show the options.
  var oldSelection = readerDropdown.selectedIndex;
  if (readerDropdown.selectedIndex == readerDropdown.length - 1)
    window.location = "options.html";
}
