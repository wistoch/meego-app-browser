// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webcookiejar_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/cookie_message_filter.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCookie.h"

using WebKit::WebCookie;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

void RendererWebCookieJarImpl::SendSynchronousMessage(
    IPC::SyncMessage* message) {
  CookieMessageFilter* filter =
      RenderThread::current()->cookie_message_filter();

  message->set_pump_messages_event(filter->pump_messages_event());
  sender_->Send(message);

  // We may end up nesting calls to SendCookieMessage, so we defer the reset
  // until we return to the top-most message loop.
  if (filter->pump_messages_event()->IsSignaled()) {
    MessageLoop::current()->PostNonNestableTask(FROM_HERE,
        NewRunnableMethod(filter,
                          &CookieMessageFilter::ResetPumpMessagesEvent));
  }
}

void RendererWebCookieJarImpl::setCookie(
    const WebURL& url, const WebURL& first_party_for_cookies,
    const WebString& value) {
  std::string value_utf8;
  UTF16ToUTF8(value.data(), value.length(), &value_utf8);
  sender_->Send(new ViewHostMsg_SetCookie(
      MSG_ROUTING_NONE, url, first_party_for_cookies, value_utf8));
}

WebString RendererWebCookieJarImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  std::string value_utf8;
  SendSynchronousMessage(new ViewHostMsg_GetCookies(
      MSG_ROUTING_NONE, url, first_party_for_cookies, &value_utf8));
  return WebString::fromUTF8(value_utf8);
}

WebString RendererWebCookieJarImpl::cookieRequestHeaderFieldValue(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  return cookies(url, first_party_for_cookies);
}

void RendererWebCookieJarImpl::rawCookies(
    const WebURL& url, const WebURL& first_party_for_cookies,
    WebVector<WebCookie>& raw_cookies) {
  std::vector<webkit_glue::WebCookie> cookies;
  SendSynchronousMessage(new ViewHostMsg_GetRawCookies(
      MSG_ROUTING_NONE, url, first_party_for_cookies, &cookies));

  WebVector<WebCookie> result(cookies.size());
  int i = 0;
  for (std::vector<webkit_glue::WebCookie>::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
     result[i++] = WebCookie(WebString::fromUTF8(it->name),
                             WebString::fromUTF8(it->value),
                             WebString::fromUTF8(it->domain),
                             WebString::fromUTF8(it->path),
                             it->expires,
                             it->http_only,
                             it->secure,
                             it->session);
  }
  raw_cookies.swap(result);
}

void RendererWebCookieJarImpl::deleteCookie(
    const WebURL& url, const WebString& cookie_name) {
  std::string cookie_name_utf8;
  UTF16ToUTF8(cookie_name.data(), cookie_name.length(), &cookie_name_utf8);
  sender_->Send(new ViewHostMsg_DeleteCookie(url, cookie_name_utf8));
}

bool RendererWebCookieJarImpl::cookiesEnabled(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  bool enabled;
  sender_->Send(new ViewHostMsg_GetCookiesEnabled(
      url, first_party_for_cookies, &enabled));
  return enabled;
}
