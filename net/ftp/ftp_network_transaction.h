// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_

#include <string>
#include <queue>
#include <utility>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/ftp/ftp_ctrl_response_buffer.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"

namespace net {

class ClientSocket;
class ClientSocketFactory;
class FtpNetworkSession;

class FtpNetworkTransaction : public FtpTransaction {
 public:
  FtpNetworkTransaction(FtpNetworkSession* session,
                        ClientSocketFactory* socket_factory);
  virtual ~FtpNetworkTransaction();

  // FtpTransaction methods:
  virtual int Start(const FtpRequestInfo* request_info,
                    CompletionCallback* callback);
  virtual int Stop(int error);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const FtpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  enum Command {
    COMMAND_NONE,
    COMMAND_USER,
    COMMAND_PASS,
    COMMAND_ACCT,
    COMMAND_SYST,
    COMMAND_TYPE,
    COMMAND_PASV,
    COMMAND_PWD,
    COMMAND_SIZE,
    COMMAND_RETR,
    COMMAND_CWD,
    COMMAND_LIST,
    COMMAND_MDTM,
    COMMAND_QUIT
  };

  enum ErrorClass {
    ERROR_CLASS_INITIATED = 1,  // The requested action was initiated.
    ERROR_CLASS_OK,             // The requested action successfully completed.
    ERROR_CLASS_PENDING,        // The command accepted, but the
                                // request on hold.
    ERROR_CLASS_ERROR_RETRY,    // The command was not accepted and the
                                // requested action did not take place,
                                // but the error condition is temporary and the
                                // action may be requested again.
    ERROR_CLASS_ERROR,          // The command was not accepted and
                                // the requested action did not take place.
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Executes correct ProcessResponse + command_name function based on last
  // issued command. Returns error code.
  int ProcessCtrlResponse();

  int SendFtpCommand(const std::string& command, Command cmd);

  // TODO(ibrar): Use C++ static_cast.
  ErrorClass GetErrorClass(int response_code) {
    return (ErrorClass)(response_code / 100);
  }

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoCtrlInit();
  int DoCtrlInitComplete(int result);
  int DoCtrlResolveHost();
  int DoCtrlResolveHostComplete(int result);
  int DoCtrlConnect();
  int DoCtrlConnectComplete(int result);
  int DoCtrlRead();
  int DoCtrlReadComplete(int result);
  int DoCtrlWrite();
  int DoCtrlWriteComplete(int result);
  int DoCtrlWriteUSER();
  int ProcessResponseUSER(const FtpCtrlResponse& response);
  int DoCtrlWritePASS();
  int ProcessResponsePASS(const FtpCtrlResponse& response);
  int DoCtrlWriteACCT();
  int ProcessResponseACCT(const FtpCtrlResponse& response);
  int DoCtrlWriteSYST();
  int ProcessResponseSYST(const FtpCtrlResponse& response);
  int DoCtrlWritePWD();
  int ProcessResponsePWD(const FtpCtrlResponse& response);
  int DoCtrlWriteTYPE();
  int ProcessResponseTYPE(const FtpCtrlResponse& response);
  int DoCtrlWritePASV();
  int ProcessResponsePASV(const FtpCtrlResponse& response);
  int DoCtrlWriteRETR();
  int ProcessResponseRETR(const FtpCtrlResponse& response);
  int DoCtrlWriteSIZE();
  int ProcessResponseSIZE(const FtpCtrlResponse& response);
  int DoCtrlWriteCWD();
  int ProcessResponseCWD(const FtpCtrlResponse& response);
  int DoCtrlWriteLIST();
  int ProcessResponseLIST(const FtpCtrlResponse& response);
  int DoCtrlWriteMDTM();
  int ProcessResponseMDTM(const FtpCtrlResponse& response);
  int DoCtrlWriteQUIT();
  int ProcessResponseQUIT(const FtpCtrlResponse& response);

  int DoDataResolveHost();
  int DoDataResolveHostComplete(int result);
  int DoDataConnect();
  int DoDataConnectComplete(int result);
  int DoDataRead();
  int DoDataReadComplete(int result);

  Command command_sent_;

  CompletionCallbackImpl<FtpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<FtpNetworkSession> session_;

  const FtpRequestInfo* request_;
  FtpResponseInfo response_;

  // Cancels the outstanding request on destruction.
  SingleRequestHostResolver resolver_;
  AddressList addresses_;

  // User buffer passed to the Read method for control socket.
  scoped_refptr<IOBuffer> read_ctrl_buf_;

  FtpCtrlResponseBuffer ctrl_response_buffer_;

  scoped_refptr<IOBuffer> read_data_buf_;
  int read_data_buf_len_;
  int file_data_len_;

  // Buffer holding the command line to be written to the control socket.
  scoped_refptr<IOBufferWithSize> write_command_buf_;

  // Buffer passed to the Write method of control socket. It actually writes
  // to the write_command_buf_ at correct offset.
  scoped_refptr<ReusedIOBuffer> write_buf_;

  // Number of bytes from write_command_buf_ that we've already sent to the
  // server.
  int write_command_buf_written_;

  int last_error_;

  bool is_anonymous_;
  bool retr_failed_;

  std::string data_connection_ip_;
  int data_connection_port_;

  ClientSocketFactory* socket_factory_;

  scoped_ptr<ClientSocket> ctrl_socket_;
  scoped_ptr<ClientSocket> data_socket_;

  enum State {
    // Control connection states:
    STATE_CTRL_INIT,
    STATE_CTRL_INIT_COMPLETE,
    STATE_CTRL_RESOLVE_HOST,
    STATE_CTRL_RESOLVE_HOST_COMPLETE,
    STATE_CTRL_CONNECT,
    STATE_CTRL_CONNECT_COMPLETE,
    STATE_CTRL_READ,
    STATE_CTRL_READ_COMPLETE,
    STATE_CTRL_WRITE,
    STATE_CTRL_WRITE_COMPLETE,
    STATE_CTRL_WRITE_USER,
    STATE_CTRL_WRITE_PASS,
    STATE_CTRL_WRITE_ACCT,
    STATE_CTRL_WRITE_SYST,
    STATE_CTRL_WRITE_TYPE,
    STATE_CTRL_WRITE_PASV,
    STATE_CTRL_WRITE_PWD,
    STATE_CTRL_WRITE_RETR,
    STATE_CTRL_WRITE_SIZE,
    STATE_CTRL_WRITE_CWD,
    STATE_CTRL_WRITE_LIST,
    STATE_CTRL_WRITE_MDTM,
    STATE_CTRL_WRITE_QUIT,
    // Data connection states:
    STATE_DATA_RESOLVE_HOST,
    STATE_DATA_RESOLVE_HOST_COMPLETE,
    STATE_DATA_CONNECT,
    STATE_DATA_CONNECT_COMPLETE,
    STATE_DATA_READ,
    STATE_DATA_READ_COMPLETE,
    STATE_NONE
  };
  State next_state_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_TRANSACTION_H_
