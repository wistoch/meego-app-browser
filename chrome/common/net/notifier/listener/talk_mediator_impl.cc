// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/notifier/listener/talk_mediator_impl.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "chrome/common/net/notifier/listener/mediator_thread_impl.h"
#include "talk/base/cryptstring.h"
#include "talk/base/ssladapter.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

// Before any authorization event from TalkMediatorImpl, we need to initialize
// the SSL library.
class SslInitializationSingleton {
 public:
  virtual ~SslInitializationSingleton() {
    talk_base::CleanupSSL();
  };

  void RegisterClient() {}

  static SslInitializationSingleton* GetInstance() {
    return Singleton<SslInitializationSingleton>::get();
  }

 private:
  friend struct DefaultSingletonTraits<SslInitializationSingleton>;

  SslInitializationSingleton() {
    talk_base::InitializeSSL();
  };

  DISALLOW_COPY_AND_ASSIGN(SslInitializationSingleton);
};

TalkMediatorImpl::TalkMediatorImpl(
    MediatorThread* mediator_thread,
    bool initialize_ssl,
    bool connect_immediately,
    bool invalidate_xmpp_auth_token)
    : delegate_(NULL),
      mediator_thread_(mediator_thread),
      invalidate_xmpp_auth_token_(invalidate_xmpp_auth_token) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (initialize_ssl) {
    SslInitializationSingleton::GetInstance()->RegisterClient();
  }
  if (connect_immediately) {
    mediator_thread_->SetDelegate(this);
    state_.connected = 1;
  }
  mediator_thread_->Start();
  state_.started = 1;
}

TalkMediatorImpl::~TalkMediatorImpl() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (state_.started) {
    Logout();
  }
}

bool TalkMediatorImpl::Login() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // Connect to the mediator thread and start processing messages.
  if (!state_.connected) {
    mediator_thread_->SetDelegate(this);
    state_.connected = 1;
  }
  if (state_.initialized && !state_.logging_in && !state_.logged_in) {
    state_.logging_in = true;
    mediator_thread_->Login(xmpp_settings_);
    return true;
  }
  return false;
}

bool TalkMediatorImpl::Logout() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (state_.connected) {
    state_.connected = 0;
  }
  if (state_.started) {
    state_.started = 0;
    state_.logging_in = 0;
    state_.logged_in = 0;
    state_.subscribed = 0;
    // We do not want to be called back during logout since we may be
    // closing.
    mediator_thread_->SetDelegate(NULL);
    mediator_thread_->Logout();
    return true;
  }
  return false;
}

bool TalkMediatorImpl::SendNotification(const OutgoingNotificationData& data) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (state_.logged_in && state_.subscribed) {
    mediator_thread_->SendNotification(data);
    return true;
  }
  return false;
}

void TalkMediatorImpl::SetDelegate(TalkMediator::Delegate* delegate) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  delegate_ = delegate;
}

bool TalkMediatorImpl::SetAuthToken(const std::string& email,
                                    const std::string& token,
                                    const std::string& token_service) {
  DCHECK(non_thread_safe_.CalledOnValidThread());

  // Verify that we can create a JID from the email provided.
  buzz::Jid jid = buzz::Jid(email);
  if (jid.node().empty() || !jid.IsValid()) {
    return false;
  }

  // Construct the XmppSettings object for login to buzz.
  xmpp_settings_.set_user(jid.node());
  xmpp_settings_.set_resource("chrome-sync");
  xmpp_settings_.set_host(jid.domain());
  xmpp_settings_.set_use_tls(true);
  xmpp_settings_.set_auth_cookie(invalidate_xmpp_auth_token_ ?
                                 token + "bogus" : token);
  xmpp_settings_.set_token_service(token_service);

  state_.initialized = 1;
  return true;
}

void TalkMediatorImpl::AddSubscribedServiceUrl(
    const std::string& service_url) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  subscribed_services_list_.push_back(service_url);
  if (state_.logged_in) {
    LOG(INFO) << "Resubscribing for updates, a new service got added";
    mediator_thread_->SubscribeForUpdates(subscribed_services_list_);
  }
}


void TalkMediatorImpl::OnConnectionStateChange(bool logged_in) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  state_.logging_in = 0;
  state_.logged_in = logged_in;
  if (logged_in) {
    LOG(INFO) << "P2P: Logged in.";
    // ListenForUpdates enables the ListenTask.  This is done before
    // SubscribeForUpdates.
    mediator_thread_->ListenForUpdates();
    // Now subscribe for updates to all the services we are interested in
    mediator_thread_->SubscribeForUpdates(subscribed_services_list_);
  } else {
    LOG(INFO) << "P2P: Logged off.";
    OnSubscriptionStateChange(false);
  }
}

void TalkMediatorImpl::OnSubscriptionStateChange(bool subscribed) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  state_.subscribed = subscribed;
  LOG(INFO) << "P2P: " << (subscribed ? "subscribed" : "unsubscribed");
  if (delegate_) {
    delegate_->OnNotificationStateChange(subscribed);
  }
}

void TalkMediatorImpl::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  LOG(INFO) << "P2P: Updates are available on the server.";
  if (delegate_) {
    delegate_->OnIncomingNotification(notification_data);
  }
}

void TalkMediatorImpl::OnOutgoingNotification() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  LOG(INFO) <<
      "P2P: Peers were notified that updates are available on the server.";
  if (delegate_) {
    delegate_->OnOutgoingNotification();
  }
}

}  // namespace notifier
