// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constant defines used in the cloud print proxy code

#include "chrome/browser/printing/cloud_print/cloud_print_consts.h"

const char kProxyIdValue[] = "proxy";
const char kPrinterNameValue[] = "printer";
const char kPrinterDescValue[] = "description";
const char kPrinterCapsValue[] = "capabilities";
const char kPrinterDefaultsValue[] = "defaults";
const char kPrinterStatusValue[] = "status";

// Values in the respone JSON from the cloud print server
const wchar_t kPrinterListValue[] = L"printers";
const wchar_t kSuccessValue[] = L"success";
const wchar_t kNameValue[] = L"name";
const wchar_t kIdValue[] = L"id";
const wchar_t kTicketUrlValue[] = L"ticketUrl";
const wchar_t kFileUrlValue[] = L"fileUrl";
const wchar_t kJobListValue[] = L"jobs";
const wchar_t kTitleValue[] = L"title";
const wchar_t kPrinterCapsHashValue[] = L"capsHash";

// TODO(sanjeevr): Change this to a real one. Also read this from prefs instead
// of hardcoding.
const char kCloudPrintServerUrl[] = "https://<TBD>";

