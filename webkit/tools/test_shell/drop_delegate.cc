// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/drop_delegate.h"

#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webview.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationCopy;
using WebKit::WebPoint;

// BaseDropTarget methods ----------------------------------------------------

DWORD TestDropDelegate::OnDragEnter(IDataObject* data_object,
                                    DWORD key_state,
                                    POINT cursor_position,
                                    DWORD effect) {
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation op = webview_->dragTargetDragEnter(
      drop_data.ToDragData(), drop_data.identity,
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y),
      WebDragOperationCopy);
  // TODO(snej): Pass the real drag operation instead
  return op ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  // TODO(snej): Return the real drop effect constant matching 'op'
}

DWORD TestDropDelegate::OnDragOver(IDataObject* data_object,
                                   DWORD key_state,
                                   POINT cursor_position,
                                   DWORD effect) {
  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation op = webview_->dragTargetDragOver(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y),
      WebDragOperationCopy);
  // TODO(snej): Pass the real drag operation instead
  return op ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  // TODO(snej): Return the real drop effect constant matching 'op'
}

void TestDropDelegate::OnDragLeave(IDataObject* data_object) {
  webview_->dragTargetDragLeave();
}

DWORD TestDropDelegate::OnDrop(IDataObject* data_object,
                               DWORD key_state,
                               POINT cursor_position,
                               DWORD effect) {
  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  webview_->dragTargetDrop(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y));

  // webkit win port always returns DROPEFFECT_NONE
  return DROPEFFECT_NONE;
}
