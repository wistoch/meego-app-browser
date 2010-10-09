// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/certificate_manager_handler.h"

#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "base/file_util.h"  // for FileAccessProvider
#include "base/safe_strerror_posix.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_manager_model.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/chrome_thread.h"  // for FileAccessProvider
#include "chrome/browser/gtk/certificate_dialogs.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"

namespace {

static const char kKeyId[] = "id";
static const char kSubNodesId[] = "subnodes";
static const char kNameId[] = "name";
static const char kIconId[] = "icon";
static const char kSecurityDeviceId[] = "device";

// Enumeration of different callers of SelectFile.
enum {
  EXPORT_PERSONAL_FILE_SELECTED,
  IMPORT_PERSONAL_FILE_SELECTED,
};

// TODO(mattm): These are duplicated from cookies_view_handler.cc
// Encodes a pointer value into a hex string.
std::string PointerToHexString(const void* pointer) {
  return base::HexEncode(&pointer, sizeof(pointer));
}

// Decodes a pointer from a hex string.
void* HexStringToPointer(const std::string& str) {
  std::vector<uint8> buffer;
  if (!base::HexStringToBytes(str, &buffer) ||
      buffer.size() != sizeof(void*)) {
    return NULL;
  }

  return *reinterpret_cast<void**>(&buffer[0]);
}

std::string OrgNameToId(const std::string& org) {
  return "org-" + org;
}

std::string CertToId(const net::X509Certificate& cert) {
  return "cert-" + PointerToHexString(&cert);
}

net::X509Certificate* IdToCert(const std::string& id) {
  if (!StartsWithASCII(id, "cert-", true))
    return NULL;
  return reinterpret_cast<net::X509Certificate*>(
      HexStringToPointer(id.substr(5)));
}

net::X509Certificate* CallbackArgsToCert(const ListValue* args) {
  std::string node_id;
  if (!args->GetString(0, &node_id)){
    return NULL;
  }
  net::X509Certificate* cert = IdToCert(node_id);
  if (!cert) {
    NOTREACHED();
    return NULL;
  }
  return cert;
}

bool CallbackArgsToBool(const ListValue* args, int index, bool* result) {
  std::string string_value;
  if (!args->GetString(index, &string_value))
    return false;

  *result = string_value[0] == 't';
  return true;
}

struct DictionaryIdComparator {
  explicit DictionaryIdComparator(icu::Collator* collator)
      : collator_(collator) {
  }

  bool operator()(const Value* a,
                  const Value* b) const {
    DCHECK(a->GetType() == Value::TYPE_DICTIONARY);
    DCHECK(b->GetType() == Value::TYPE_DICTIONARY);
    const DictionaryValue* a_dict = reinterpret_cast<const DictionaryValue*>(a);
    const DictionaryValue* b_dict = reinterpret_cast<const DictionaryValue*>(b);
    string16 a_str;
    string16 b_str;
    a_dict->GetString(kNameId, &a_str);
    b_dict->GetString(kNameId, &b_str);
    if (collator_ == NULL)
      return a_str < b_str;
    return l10n_util::CompareString16WithCollator(
        collator_, a_str, b_str) == UCOL_LESS;
  }

  icu::Collator* collator_;
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  FileAccessProvider

// TODO(mattm): Move to some shared location?
class FileAccessProvider
    : public base::RefCountedThreadSafe<FileAccessProvider>,
      public CancelableRequestProvider {
 public:
  // Reports 0 on success or errno on failure, and the data of the file upon
  // success.
  // TODO(mattm): don't pass std::string by value.. could use RefCountedBytes
  // but it's a vector.  Maybe do the derive from CancelableRequest thing
  // described in cancelable_request.h?
  typedef Callback2<int, std::string>::Type ReadCallback;

  // Reports 0 on success or errno on failure, and the number of bytes written,
  // on success.
  typedef Callback2<int, int>::Type WriteCallback;

  Handle StartRead(const FilePath& path,
                   CancelableRequestConsumerBase* consumer,
                   ReadCallback* callback);
  Handle StartWrite(const FilePath& path,
                    const std::string& data,
                    CancelableRequestConsumerBase* consumer,
                    WriteCallback* callback);

 private:
  void DoRead(scoped_refptr<CancelableRequest<ReadCallback> > request,
              FilePath path);
  void DoWrite(scoped_refptr<CancelableRequest<WriteCallback> > request,
              FilePath path,
              std::string data);
};

CancelableRequestProvider::Handle FileAccessProvider::StartRead(
    const FilePath& path,
    CancelableRequestConsumerBase* consumer,
    FileAccessProvider::ReadCallback* callback) {
  scoped_refptr<CancelableRequest<ReadCallback> > request(
      new CancelableRequest<ReadCallback>(callback));
  AddRequest(request, consumer);

  // Send the parameters and the request to the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &FileAccessProvider::DoRead, request, path));

  // The handle will have been set by AddRequest.
  return request->handle();
}

CancelableRequestProvider::Handle FileAccessProvider::StartWrite(
    const FilePath& path,
    const std::string& data,
    CancelableRequestConsumerBase* consumer,
    WriteCallback* callback) {
  scoped_refptr<CancelableRequest<WriteCallback> > request(
      new CancelableRequest<WriteCallback>(callback));
  AddRequest(request, consumer);

  // Send the parameters and the request to the file thWrite.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &FileAccessProvider::DoWrite, request, path, data));

  // The handle will have been set by AddRequest.
  return request->handle();
}

void FileAccessProvider::DoRead(
    scoped_refptr<CancelableRequest<ReadCallback> > request,
    FilePath path) {
  if (request->canceled())
    return;

  std::string data;
  VLOG(1) << "DoRead starting read";
  bool success = file_util::ReadFileToString(path, &data);
  int saved_errno = success ? 0 : errno;
  VLOG(1) << "DoRead done read: " << success << " " << data.size();
  request->ForwardResult(ReadCallback::TupleType(saved_errno, data));
}

void FileAccessProvider::DoWrite(
    scoped_refptr<CancelableRequest<WriteCallback> > request,
    FilePath path,
    std::string data) {
  VLOG(1) << "DoWrite starting write";
  int bytes_written = file_util::WriteFile(path, data.data(), data.size());
  int saved_errno = bytes_written >= 0 ? 0 : errno;
  VLOG(1) << "DoWrite done write " << bytes_written;

  if (request->canceled())
    return;

  request->ForwardResult(WriteCallback::TupleType(saved_errno, bytes_written));
}

///////////////////////////////////////////////////////////////////////////////
//  CertificateManagerHandler

CertificateManagerHandler::CertificateManagerHandler()
    : file_access_provider_(new FileAccessProvider) {
  certificate_manager_model_.reset(new CertificateManagerModel(this));
}

CertificateManagerHandler::~CertificateManagerHandler() {
}

void CertificateManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("certificateManagerPage",
      l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE));

  // Tabs.
  localized_strings->SetString("personalCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PERSONAL_CERTS_TAB_LABEL));
  localized_strings->SetString("emailCertsTabTitle",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_OTHER_PEOPLES_CERTS_TAB_LABEL));
  localized_strings->SetString("serverCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_CERTS_TAB_LABEL));
  localized_strings->SetString("caCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_CERT_AUTHORITIES_TAB_LABEL));
  localized_strings->SetString("unknownCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TAB_LABEL));

  // Tab descriptions.
  localized_strings->SetString("personalCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_USER_TREE_DESCRIPTION));
  localized_strings->SetString("emailCertsTabDescription",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_OTHER_PEOPLE_TREE_DESCRIPTION));
  localized_strings->SetString("serverCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_TREE_DESCRIPTION));
  localized_strings->SetString("caCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_AUTHORITIES_TREE_DESCRIPTION));
  localized_strings->SetString("unknownCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TREE_DESCRIPTION));

  // Tree columns.
  localized_strings->SetString("certNameColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_NAME_COLUMN_LABEL));
  localized_strings->SetString("certDeviceColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DEVICE_COLUMN_LABEL));
  localized_strings->SetString("certSerialColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERIAL_NUMBER_COLUMN_LABEL));
  localized_strings->SetString("certExpiresColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPIRES_COLUMN_LABEL));
  localized_strings->SetString("certEmailColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EMAIL_ADDRESS_COLUMN_LABEL));

  // Buttons.
  localized_strings->SetString("view_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_VIEW_CERT_BUTTON));
  localized_strings->SetString("import_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_IMPORT_BUTTON));
  localized_strings->SetString("export_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_BUTTON));
  localized_strings->SetString("export_all_certificates",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_ALL_BUTTON));
  localized_strings->SetString("edit_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_BUTTON));
  localized_strings->SetString("delete_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_BUTTON));

  // Certificate Delete overlay strings.
  localized_strings->SetString("personalCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_USER_FORMAT));
  localized_strings->SetString("personalCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_USER_DESCRIPTION));
  // For now, use the "unknown" strings for email certs too.  Maybe we should
  // just get rid of the email tab.
  localized_strings->SetString("emailCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_UNKNOWN_FORMAT));
  localized_strings->SetString("emailCertsTabDeleteImpact", "");
  localized_strings->SetString("serverCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_SERVER_FORMAT));
  localized_strings->SetString("serverCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_SERVER_DESCRIPTION));
  localized_strings->SetString("caCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_CA_FORMAT));
  localized_strings->SetString("caCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_CA_DESCRIPTION));
  localized_strings->SetString("unknownCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_UNKNOWN_FORMAT));
  localized_strings->SetString("unknownCertsTabDeleteImpact", "");

  // Certificate Restore overlay strings.
  localized_strings->SetString("certificateRestorePasswordDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_RESTORE_PASSWORD_DESC));
  localized_strings->SetString("certificatePasswordLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PASSWORD_LABEL));

  // Personal Certificate Export overlay strings.
  localized_strings->SetString("certificateExportPasswordDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_PASSWORD_DESC));
  localized_strings->SetString("certificateExportPasswordHelp",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_PASSWORD_HELP));
  localized_strings->SetString("certificateConfirmPasswordLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_CONFIRM_PASSWORD_LABEL));

  // Edit CA Trust overlay strings.
  localized_strings->SetString("certificateEditTrustLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_TRUST_LABEL));
  localized_strings->SetString("certificateEditCaTrustDescriptionFormat",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_EDIT_CA_TRUST_DESCRIPTION_FORMAT));
  localized_strings->SetString("certificateCaTrustSSLLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_SSL_LABEL));
  localized_strings->SetString("certificateCaTrustEmailLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_EMAIL_LABEL));
  localized_strings->SetString("certificateCaTrustObjSignLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_OBJSIGN_LABEL));
}

void CertificateManagerHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("viewCertificate",
      NewCallback(this, &CertificateManagerHandler::View));

  dom_ui_->RegisterMessageCallback("getCaCertificateTrust",
      NewCallback(this, &CertificateManagerHandler::GetCATrust));
  dom_ui_->RegisterMessageCallback("editCaCertificateTrust",
      NewCallback(this, &CertificateManagerHandler::EditCATrust));

  dom_ui_->RegisterMessageCallback("editServerCertificate",
      NewCallback(this, &CertificateManagerHandler::EditServer));

  dom_ui_->RegisterMessageCallback("cancelImportExportCertificate",
      NewCallback(this, &CertificateManagerHandler::CancelImportExportProcess));

  dom_ui_->RegisterMessageCallback("exportPersonalCertificate",
      NewCallback(this, &CertificateManagerHandler::ExportPersonal));
  dom_ui_->RegisterMessageCallback("exportAllPersonalCertificates",
      NewCallback(this, &CertificateManagerHandler::ExportAllPersonal));
  dom_ui_->RegisterMessageCallback("exportPersonalCertificatePasswordSelected",
      NewCallback(this,
                  &CertificateManagerHandler::ExportPersonalPasswordSelected));

  dom_ui_->RegisterMessageCallback("importPersonalCertificate",
      NewCallback(this, &CertificateManagerHandler::StartImportPersonal));
  dom_ui_->RegisterMessageCallback("importPersonalCertificatePasswordSelected",
      NewCallback(this,
                  &CertificateManagerHandler::ImportPersonalPasswordSelected));

  dom_ui_->RegisterMessageCallback("importCaCertificate",
      NewCallback(this, &CertificateManagerHandler::ImportCA));

  dom_ui_->RegisterMessageCallback("exportCertificate",
      NewCallback(this, &CertificateManagerHandler::Export));

  dom_ui_->RegisterMessageCallback("deleteCertificate",
      NewCallback(this, &CertificateManagerHandler::Delete));

  dom_ui_->RegisterMessageCallback("populateCertificateManager",
      NewCallback(this, &CertificateManagerHandler::Populate));
}

void CertificateManagerHandler::CertificatesRefreshed() {
  PopulateTree("personalCertsTab", net::USER_CERT);
  PopulateTree("emailCertsTab", net::EMAIL_CERT);
  PopulateTree("serverCertsTab", net::SERVER_CERT);
  PopulateTree("caCertsTab", net::CA_CERT);
  PopulateTree("otherCertsTab", net::UNKNOWN_CERT);
  VLOG(1) << "populating finished";
}

void CertificateManagerHandler::FileSelected(const FilePath& path, int index,
                                             void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
      ExportPersonalFileSelected(path);
      break;
    case IMPORT_PERSONAL_FILE_SELECTED:
      ImportPersonalFileSelected(path);
      break;
    default:
      NOTREACHED();
  }
}

void CertificateManagerHandler::FileSelectionCanceled(void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_PERSONAL_FILE_SELECTED:
      ImportExportCleanup();
      break;
    default:
      NOTREACHED();
  }
}

void CertificateManagerHandler::View(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  if (!cert)
    return;
  ShowCertificateViewer(GetParentWindow(), cert);
}

void CertificateManagerHandler::GetCATrust(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  if (!cert) {
    dom_ui_->CallJavascriptFunction(L"CertificateEditCaTrustOverlay.dismiss");
    return;
  }

  int trust = certificate_manager_model_->GetCertTrust(cert, net::CA_CERT);
  FundamentalValue ssl_value(bool(trust & net::CertDatabase::TRUSTED_SSL));
  FundamentalValue email_value(bool(trust & net::CertDatabase::TRUSTED_EMAIL));
  FundamentalValue obj_sign_value(
      bool(trust & net::CertDatabase::TRUSTED_OBJ_SIGN));
  dom_ui_->CallJavascriptFunction(
      L"CertificateEditCaTrustOverlay.populateTrust",
      ssl_value, email_value, obj_sign_value);
}

void CertificateManagerHandler::EditCATrust(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  bool fail = !cert;
  bool trust_ssl;
  bool trust_email;
  bool trust_obj_sign;
  fail |= !CallbackArgsToBool(args, 1, &trust_ssl);
  fail |= !CallbackArgsToBool(args, 2, &trust_email);
  fail |= !CallbackArgsToBool(args, 3, &trust_obj_sign);
  if (fail) {
    LOG(ERROR) << "EditCATrust args fail";
    dom_ui_->CallJavascriptFunction(L"CertificateEditCaTrustOverlay.dismiss");
    return;
  }

  bool result = certificate_manager_model_->SetCertTrust(
      cert,
      net::CA_CERT,
      trust_ssl * net::CertDatabase::TRUSTED_SSL +
          trust_email * net::CertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::CertDatabase::TRUSTED_OBJ_SIGN);
  dom_ui_->CallJavascriptFunction(L"CertificateEditCaTrustOverlay.dismiss");
  if (!result) {
    // TODO(mattm): better error messages?
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SET_TRUST_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  }
}

void CertificateManagerHandler::EditServer(const ListValue* args) {
  NOTIMPLEMENTED();
}

void CertificateManagerHandler::ExportPersonal(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  if (!cert)
    return;

  selected_cert_list_.push_back(cert);

  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE, string16(),
      FilePath(), &file_type_info, 1,
      FILE_PATH_LITERAL("p12"), GetParentWindow(),
      reinterpret_cast<void*>(EXPORT_PERSONAL_FILE_SELECTED));
}

void CertificateManagerHandler::ExportAllPersonal(const ListValue* args) {
  NOTIMPLEMENTED();
}

void CertificateManagerHandler::ExportPersonalFileSelected(
    const FilePath& path) {
  file_path_ = path;
  dom_ui_->CallJavascriptFunction(
      L"CertificateManager.exportPersonalAskPassword");
}

void CertificateManagerHandler::ExportPersonalPasswordSelected(
    const ListValue* args) {
  if (!args->GetString(0, &password_)){
    dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }
  std::string output;
  int num_exported = certificate_manager_model_->ExportToPKCS12(
      selected_cert_list_,
      password_,
      &output);
  if (!num_exported) {
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
    dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartWrite(
      file_path_,
      output,
      &consumer_,
      NewCallback(this, &CertificateManagerHandler::ExportPersonalFileWritten));
}

void CertificateManagerHandler::ExportPersonalFileWritten(int write_errno,
                                                          int bytes_written) {
  dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
  ImportExportCleanup();
  if (write_errno) {
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_WRITE_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(write_errno))));
  }
}

void CertificateManagerHandler::StartImportPersonal(const ListValue* args) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE, string16(),
      FilePath(), &file_type_info, 1,
      FILE_PATH_LITERAL("p12"), GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_PERSONAL_FILE_SELECTED));
}

void CertificateManagerHandler::ImportPersonalFileSelected(
    const FilePath& path) {
  file_path_ = path;
  dom_ui_->CallJavascriptFunction(
      L"CertificateManager.importPersonalAskPassword");
}

void CertificateManagerHandler::ImportPersonalPasswordSelected(
    const ListValue* args) {
  if (!args->GetString(0, &password_)){
    dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartRead(
      file_path_,
      &consumer_,
      NewCallback(this, &CertificateManagerHandler::ImportPersonalFileRead));
}

void CertificateManagerHandler::ImportPersonalFileRead(
    int read_errno, std::string data) {
  if (read_errno) {
    ImportExportCleanup();
    dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_READ_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(read_errno))));
    return;
  }
  int result = certificate_manager_model_->ImportFromPKCS12(data, password_);
  ImportExportCleanup();
  dom_ui_->CallJavascriptFunction(L"CertificateRestoreOverlay.dismiss");
  switch (result) {
    case net::OK:
      break;
    case net::ERR_PKCS12_IMPORT_BAD_PASSWORD:
      ShowError(
          l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_IMPORT_ERROR_TITLE),
          l10n_util::GetStringUTF8(IDS_CERT_MANAGER_BAD_PASSWORD));
      // TODO(mattm): if the error was a bad password, we should reshow the
      // password dialog after the user dismisses the error dialog.
      break;
    default:
      ShowError(
          l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_IMPORT_ERROR_TITLE),
          l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
      break;
  }
}

void CertificateManagerHandler::CancelImportExportProcess(
    const ListValue* args) {
  ImportExportCleanup();
}

void CertificateManagerHandler::ImportExportCleanup() {
  file_path_.clear();
  password_.clear();
  selected_cert_list_.clear();
  select_file_dialog_ = NULL;
}

void CertificateManagerHandler::ImportCA(const ListValue* args) {
  NOTIMPLEMENTED();
}

void CertificateManagerHandler::Export(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  if (!cert)
    return;
  ShowCertExportDialog(GetParentWindow(), cert->os_cert_handle());
}

void CertificateManagerHandler::Delete(const ListValue* args) {
  net::X509Certificate* cert = CallbackArgsToCert(args);
  if (!cert)
    return;
  bool result = certificate_manager_model_->Delete(cert);
  if (!result) {
    // TODO(mattm): better error messages?
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_DELETE_CERT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  }
}

void CertificateManagerHandler::Populate(const ListValue* args) {
  certificate_manager_model_->Refresh();
}

void CertificateManagerHandler::PopulateTree(const std::string& tab_name,
                                             net::CertType type) {
  const std::string tree_name = tab_name + "-tree";

  scoped_ptr<icu::Collator> collator;
  UErrorCode error = U_ZERO_ERROR;
  collator.reset(
      icu::Collator::createInstance(
          icu::Locale(g_browser_process->GetApplicationLocale().c_str()),
          error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  DictionaryIdComparator comparator(collator.get());
  CertificateManagerModel::OrgGroupingMap map;

  certificate_manager_model_->FilterAndBuildOrgGroupingMap(type, &map);

  {
    ListValue* nodes = new ListValue;
    for (CertificateManagerModel::OrgGroupingMap::iterator i = map.begin();
         i != map.end(); ++i) {
      // Populate first level (org name).
      DictionaryValue* dict = new DictionaryValue;
      dict->SetString(kKeyId, OrgNameToId(i->first));
      dict->SetString(kNameId, i->first);

      // Populate second level (certs).
      ListValue* subnodes = new ListValue;
      for (net::CertificateList::const_iterator org_cert_it = i->second.begin();
           org_cert_it != i->second.end(); ++org_cert_it) {
        DictionaryValue* cert_dict = new DictionaryValue;
        net::X509Certificate* cert = org_cert_it->get();
        cert_dict->SetString(kKeyId, CertToId(*cert));
        cert_dict->SetString(kNameId, certificate_manager_model_->GetColumnText(
            *cert, CertificateManagerModel::COL_SUBJECT_NAME));
        // TODO(mattm): Other columns.
        // TODO(mattm): Get a real icon (or figure out how to make the domui
        // tree not use icons at all).
        cert_dict->SetString(kIconId, "chrome://theme/IDR_COOKIE_ICON");
        subnodes->Append(cert_dict);
      }
      std::sort(subnodes->begin(), subnodes->end(), comparator);

      dict->Set(kSubNodesId, subnodes);
      nodes->Append(dict);
    }
    std::sort(nodes->begin(), nodes->end(), comparator);

    ListValue args;
    args.Append(Value::CreateStringValue(tree_name));
    args.Append(nodes);
    dom_ui_->CallJavascriptFunction(L"CertificateManager.onPopulateTree", args);
  }
}

void CertificateManagerHandler::ShowError(const std::string& title,
                                          const std::string& error) const {
  std::vector<const Value*> args;
  args.push_back(Value::CreateStringValue(title));
  args.push_back(Value::CreateStringValue(error));
  args.push_back(Value::CreateNullValue());  // okTitle
  args.push_back(Value::CreateStringValue(""));  // cancelTitle
  args.push_back(Value::CreateNullValue());  // okCallback
  args.push_back(Value::CreateNullValue());  // cancelCallback
  dom_ui_->CallJavascriptFunction(L"AlertOverlay.show", args);
}

gfx::NativeWindow CertificateManagerHandler::GetParentWindow() const {
  return dom_ui_->tab_contents()->view()->GetTopLevelNativeWindow();
}
