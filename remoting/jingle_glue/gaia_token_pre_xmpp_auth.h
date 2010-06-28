// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): Unfork with
//   chrome/common/net/notifier/communicator/gaia_token_pre_xmpp_auth.h

#ifndef REMOTING_JINGLE_GLUE_GAIA_TOKEN_PRE_XMPP_AUTH_H_
#define REMOTING_JINGLE_GLUE_GAIA_TOKEN_PRE_XMPP_AUTH_H_

#include <string>
#include <vector>

#include "talk/xmpp/prexmppauth.h"

namespace remoting {

// This class implements buzz::PreXmppAuth interface for token-based
// authentication in GTalk. It looks for the X-GOOGLE-TOKEN auth mechanism
// and uses that instead of the default auth mechanism (PLAIN).
class GaiaTokenPreXmppAuth : public buzz::PreXmppAuth {
 public:
  GaiaTokenPreXmppAuth(const std::string& username, const std::string& token,
                       const std::string& token_service);

  virtual ~GaiaTokenPreXmppAuth();

  // buzz::PreXmppAuth (-buzz::SaslHandler) implementation.  We stub
  // all the methods out as we don't actually do any authentication at
  // this point.
  virtual void StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string& auth_cookie);

  virtual bool IsAuthDone() const;

  virtual bool IsAuthorized() const;

  virtual bool HadError() const;

  virtual int GetError() const;

  virtual buzz::CaptchaChallenge GetCaptchaChallenge() const;

  virtual std::string GetAuthCookie() const;

  // buzz::SaslHandler implementation.

  virtual std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms, bool encrypted);

  virtual buzz::SaslMechanism* CreateSaslMechanism(
      const std::string& mechanism);

 private:
  std::string username_;
  std::string token_;
  std::string token_service_;
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_GAIA_TOKEN_PRE_XMPP_AUTH_H_
