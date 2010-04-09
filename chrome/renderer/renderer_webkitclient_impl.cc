// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webkitclient_impl.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/database_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webstoragenamespace_impl.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "chrome/renderer/webgraphicscontext3d_command_buffer_impl.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX)
#include "chrome/renderer/renderer_sandbox_support_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using WebKit::WebFrame;
using WebKit::WebKitClient;
using WebKit::WebStorageArea;
using WebKit::WebStorageEventDispatcher;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

//------------------------------------------------------------------------------

WebKit::WebClipboard* RendererWebKitClientImpl::clipboard() {
  return &clipboard_;
}

WebKit::WebMimeRegistry* RendererWebKitClientImpl::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebSandboxSupport* RendererWebKitClientImpl::sandboxSupport() {
#if defined(OS_WIN) || defined(OS_LINUX)
  return &sandbox_support_;
#else
  return NULL;
#endif
}

WebKit::WebCookieJar* RendererWebKitClientImpl::cookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return NULL;
}

bool RendererWebKitClientImpl::sandboxEnabled() {
  // As explained in WebKitClient.h, this function is used to decide whether to
  // allow file system operations to come out of WebKit or not.  Even if the
  // sandbox is disabled, there's no reason why the code should act any
  // differently...unless we're in single process mode.  In which case, we have
  // no other choice.  WebKitClient.h discourages using this switch unless
  // absolutely necessary, so hopefully we won't end up with too many code paths
  // being different in single-process mode.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

bool RendererWebKitClientImpl::getFileSize(const WebString& path,
                                           long long& result) {
  if (RenderThread::current()->Send(
          new ViewHostMsg_GetFileSize(webkit_glue::WebStringToFilePath(path),
                                      reinterpret_cast<int64*>(&result)))) {
    return result >= 0;
  }

  result = -1;
  return false;
}

bool RendererWebKitClientImpl::getFileModificationTime(
    const WebKit::WebString& path,
    double& result) {
  base::Time time;
  if (RenderThread::current()->Send(
          new ViewHostMsg_GetFileModificationTime(
              webkit_glue::WebStringToFilePath(path), &time))) {
    result = time.ToDoubleT();
    return true;
  }

  result = 0;
  return false;
}

unsigned long long RendererWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return RenderThread::current()->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool RendererWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  return RenderThread::current()->visited_link_slave()->IsVisited(link_hash);
}

WebKit::WebMessagePortChannel*
RendererWebKitClientImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void RendererWebKitClientImpl::prefetchHostName(const WebString& hostname) {
  if (!hostname.isEmpty()) {
    std::string hostname_utf8;
    UTF16ToUTF8(hostname.data(), hostname.length(), &hostname_utf8);
    DnsPrefetchCString(hostname_utf8.data(), hostname_utf8.length());
  }
}

WebString RendererWebKitClientImpl::defaultLocale() {
  // TODO(darin): Eliminate this webkit_glue call.
  return WideToUTF16(webkit_glue::GetWebKitLocale());
}

void RendererWebKitClientImpl::suddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ = std::max(--sudden_termination_disables_, 0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThread* thread = RenderThread::current();
  if (thread)  // NULL in unittests.
    thread->Send(new ViewHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace* RendererWebKitClientImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return WebStorageNamespace::createLocalStorageNamespace(path, quota);
  return new RendererWebStorageNamespaceImpl(DOM_STORAGE_LOCAL);
}

void RendererWebKitClientImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  DCHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  // Inefficient, but only used in single process mode.
  scoped_ptr<WebStorageEventDispatcher> event_dispatcher(
      WebStorageEventDispatcher::create());
  event_dispatcher->dispatchStorageEvent(key, old_value, new_value, origin,
                                         url, is_local_storage);
}

//------------------------------------------------------------------------------

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromExtension(
      webkit_glue::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_path)),
      &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  FilePath::StringType file_extension;
  RenderThread::current()->Send(
      new ViewHostMsg_GetPreferredExtensionForMimeType(UTF16ToASCII(mime_type),
          &file_extension));
  return webkit_glue::FilePathStringToWebString(file_extension);
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitClientImpl::SandboxSupport::ensureFontLoaded(HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  return RenderThread::current()->Send(new ViewHostMsg_LoadFont(logfont));
}

#elif defined(OS_LINUX)

WebString RendererWebKitClientImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebKit::WebUChar* characters, size_t num_characters) {
  AutoLock lock(unicode_font_families_mutex_);
  const std::string key(reinterpret_cast<const char*>(characters),
                        num_characters * sizeof(characters[0]));
  const std::map<std::string, std::string>::const_iterator iter =
      unicode_font_families_.find(key);
  if (iter != unicode_font_families_.end())
    return WebString::fromUTF8(iter->second);

  const std::string family_name =
      renderer_sandbox_support::getFontFamilyForCharacters(characters,
                                                           num_characters);
  unicode_font_families_.insert(make_pair(key, family_name));
  return WebString::fromUTF8(family_name);
}

void RendererWebKitClientImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  renderer_sandbox_support::getRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

WebKitClient::FileHandle RendererWebKitClientImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags,
  WebKitClient::FileHandle* dir_handle) {
  return DatabaseUtil::databaseOpenFile(vfs_file_name, desired_flags,
      dir_handle);
}

int RendererWebKitClientImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::databaseDeleteFile(vfs_file_name, sync_dir);
}

long RendererWebKitClientImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::databaseGetFileAttributes(vfs_file_name);
}

long long RendererWebKitClientImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::databaseGetFileSize(vfs_file_name);
}

WebKit::WebSharedWorkerRepository*
RendererWebKitClientImpl::sharedWorkerRepository() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSharedWorkers)) {
    return &shared_worker_repository_;
  } else {
    return NULL;
  }
}

WebKit::WebGraphicsContext3D*
RendererWebKitClientImpl::createGraphicsContext3D() {
  // TODO(kbr): remove the WebGraphicsContext3D::createDefault code path
  // completely, and at least for a period of time, either pop up a warning
  // dialog, or don't even start the browser, if WebGL is enabled and the
  // sandbox isn't.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    return WebKit::WebGraphicsContext3D::createDefault();
  } else {
#if defined(ENABLE_GPU)
    return new WebGraphicsContext3DCommandBufferImpl();
#else
    return NULL;
#endif
  }
}

//------------------------------------------------------------------------------

WebKit::WebString RendererWebKitClientImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const WebKit::WebString& challenge,
    const WebKit::WebURL& url) {
  std::string signed_public_key;
  RenderThread::current()->Send(new ViewHostMsg_Keygen(
      static_cast<uint32>(key_size_index),
      challenge.utf8(),
      GURL(url),
      &signed_public_key));
  return WebString::fromUTF8(signed_public_key);
}
