// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly into net_log.h. DO NOT include this file anywhere else.

// --------------------------------------------------------------------------
// General pseudo-events
// --------------------------------------------------------------------------

// Something got cancelled (we determine what is cancelled based on the
// log context around it.)
EVENT_TYPE(CANCELLED)

// TODO(eroman): remove the remaining consumers of this.
EVENT_TYPE(TODO_STRING)

// Marks the creation/destruction of a request (URLRequest or SocketStream).
// In the begin phase of this event, the message will contain a string which
// is the URL.
EVENT_TYPE(REQUEST_ALIVE)

// ------------------------------------------------------------------------
// HostResolverImpl
// ------------------------------------------------------------------------

// The start/end of a host resolve (DNS) request.
EVENT_TYPE(HOST_RESOLVER_IMPL)

// The start/end of HostResolver::Observer::OnStartResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONSTART)

// The start/end of HostResolver::Observer::OnFinishResolutionWithStatus
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONFINISH)

// The start/end of HostResolver::Observer::OnCancelResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONCANCEL)

// ------------------------------------------------------------------------
// InitProxyResolver
// ------------------------------------------------------------------------

// The start/end of auto-detect + custom PAC URL configuration.
EVENT_TYPE(INIT_PROXY_RESOLVER)

// The start/end of download of a PAC script. This could be the well-known
// WPAD URL (if testing auto-detect), or a custom PAC URL.
//
// The START event has the parameters:
//   {
//     "url": <URL string of script being fetched>
//   }
//
// If the fetch failed, then the END phase has these parameters:
//   {
//      "error_code": <Net error code integer>
//   }
EVENT_TYPE(INIT_PROXY_RESOLVER_FETCH_PAC_SCRIPT)

// The start/end of the testing of a PAC script (trying to parse the fetched
// file as javascript).
//
// If the parsing of the script failed, the END phase will have parameters:
//   {
//      "error_code": <Net error code integer>
//   }
EVENT_TYPE(INIT_PROXY_RESOLVER_SET_PAC_SCRIPT)

// This event means that initialization failed because there was no
// configured script fetcher. (This indicates a configuration error).
EVENT_TYPE(INIT_PROXY_RESOLVER_HAS_NO_FETCHER)

// This event is emitted after deciding to fall-back to the next PAC
// script in the list.
EVENT_TYPE(INIT_PROXY_RESOLVER_FALLING_BACK_TO_NEXT_PAC_URL)

// ------------------------------------------------------------------------
// ProxyService
// ------------------------------------------------------------------------

// The start/end of a proxy resolve request.
EVENT_TYPE(PROXY_SERVICE)

// The time while a request is waiting on InitProxyResolver to configure
// against either WPAD or custom PAC URL. The specifics on this time
// are found from ProxyService::init_proxy_resolver_log().
EVENT_TYPE(PROXY_SERVICE_WAITING_FOR_INIT_PAC)

// The time taken to fetch the system proxy configuration.
EVENT_TYPE(PROXY_SERVICE_POLL_CONFIG_SERVICE_FOR_CHANGES)

// This event is emitted to show what the PAC script returned. It can contain
// extra parameters that are either:
//   {
//      "pac_string": <List of valid proxy servers, in PAC format>
//   }
//
//  Or if the the resolver failed:
//   {
//      "net_error": <Net error code that resolver failed with>
//   }
EVENT_TYPE(PROXY_SERVICE_RESOLVED_PROXY_LIST)

// ------------------------------------------------------------------------
// Proxy Resolver
// ------------------------------------------------------------------------

// Measures the time taken to execute the "myIpAddress()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_MY_IP_ADDRESS)

// Measures the time taken to execute the "myIpAddressEx()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX)

// Measures the time taken to execute the "dnsResolve()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_DNS_RESOLVE)

// Measures the time taken to execute the "dnsResolveEx()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_DNS_RESOLVE_EX)

// Measures the time that a proxy resolve request was stalled waiting for the
// proxy resolver thread to free-up.
EVENT_TYPE(WAITING_FOR_SINGLE_PROXY_RESOLVER_THREAD)

// ------------------------------------------------------------------------
// ClientSocket
// ------------------------------------------------------------------------

// The start/end of a TCP connect().
EVENT_TYPE(TCP_CONNECT)

// Marks the destruction of a TCP socket.
EVENT_TYPE(TCP_SOCKET_DONE)

// The start/end of a SOCKS connect().
EVENT_TYPE(SOCKS_CONNECT)

// The start/end of a SOCKS5 connect().
EVENT_TYPE(SOCKS5_CONNECT)

// This event is emitted when the SOCKS connect fails because the provided
// was longer than 255 characters.
EVENT_TYPE(SOCKS_HOSTNAME_TOO_BIG)

// These events are emitted when insufficient data was read while
// trying to establish a connection to the SOCKS proxy server
// (during the greeting phase or handshake phase, respectively).
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_GREETING)
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_HANDSHAKE)

// This event indicates that a bad version number was received in the
// proxy server's response. The extra parameters show its value:
//   {
//     "version": <Integer version number in the response>
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_VERSION)

// This event indicates that the SOCKS proxy server returned an error while
// trying to create a connection. The following parameters will be attached
// to the event:
//   {
//     "error_code": <Integer error code returned by the server>
//   }
EVENT_TYPE(SOCKS_SERVER_ERROR)

// This event indicates that the SOCKS proxy server asked for an authentication
// method that we don't support. The following parameters are attached to the
// event:
//   {
//     "method": <Integer method code>
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_AUTH)

// This event indicates that the SOCKS proxy server's response indicated an
// address type which we are not prepared to handle.
// The following parameters are attached to the event:
//   {
//     "address_type": <Integer code for the address type>
//   }
EVENT_TYPE(SOCKS_UNKNOWN_ADDRESS_TYPE)

// The start/end of a SSL connect().
EVENT_TYPE(SSL_CONNECT)

// The specified number of bytes were sent on the socket.
// The following parameters are attached:
//   {
//     "num_bytes": <Number of bytes that were just sent>
//   }
EVENT_TYPE(SOCKET_BYTES_SENT)

// The specified number of bytes were received on the socket.
// The following parameters are attached:
//   {
//     "num_bytes": <Number of bytes that were just sent>
//   }
EVENT_TYPE(SOCKET_BYTES_RECEIVED)

// ------------------------------------------------------------------------
// ClientSocketPoolBase::ConnectJob
// ------------------------------------------------------------------------

// The start/end of a ConnectJob.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB)

// Whether the connect job timed out.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_TIMED_OUT)

// ------------------------------------------------------------------------
// ClientSocketPoolBaseHelper
// ------------------------------------------------------------------------

// The start/end of a client socket pool request for a socket.
EVENT_TYPE(SOCKET_POOL)

// The request stalled because there are too many sockets in the pool.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS)

// The request stalled because there are too many sockets in the group.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP)

// Indicates that we reused an existing socket. Attached to the event are
// the parameters:
//   {
//     "idle_ms": <The number of milliseconds the socket was sitting idle for>
//   }
EVENT_TYPE(SOCKET_POOL_REUSED_AN_EXISTING_SOCKET)

// This event simply describes the host:port that were requested from the
// socket pool. Its parameters are:
//   {
//     "host_and_port": <String encoding the host and port>
//   }
EVENT_TYPE(TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET)


// A backup socket is created due to slow connect
EVENT_TYPE(SOCKET_BACKUP_CREATED)

// A backup socket is created due to slow connect
EVENT_TYPE(SOCKET_BACKUP_TIMER_EXTENDED)

// Identifies the NetLog::Source() for a ConnectJob.  The begin event
// is sent to the request that triggered the ConnectJob, the end event
// is sent to the request that received the connected socket.  Because of
// late binding, they may not be the same. Therefore the ID for the
// ConnectJob NetLog is sent in both events. The event parameters are:
//   {
//      "source_id": <ID of the connect job that was bound to this source>
//   }
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_ID)

// Identifies the NetLog::Source() for the Socket assigned to the pending
// request. The event parameters are:
//   {
//      "source_id": <ID of the socket that was bound to this source>
//   }
EVENT_TYPE(SOCKET_POOL_SOCKET_ID)

// ------------------------------------------------------------------------
// URLRequest
// ------------------------------------------------------------------------

// Measures the time between URLRequest::Start() and
// URLRequest::ResponseStarted().
//
// For the BEGIN phase, the following parameters are attached:
//   {
//      "url": <String of URL being loaded>
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>
//   }
EVENT_TYPE(URL_REQUEST_START)

// This event is sent once a URLRequest receives a redirect. The parameters
// attached to the event are:
//   {
//     "location": <The URL that was redirected to>
//   }
EVENT_TYPE(URL_REQUEST_REDIRECTED)

// ------------------------------------------------------------------------
// HttpCache
// ------------------------------------------------------------------------

// Measures the time while opening a disk cache entry.
EVENT_TYPE(HTTP_CACHE_OPEN_ENTRY)

// Measures the time while creating a disk cache entry.
EVENT_TYPE(HTTP_CACHE_CREATE_ENTRY)

// Measures the time while deleting a disk cache entry.
EVENT_TYPE(HTTP_CACHE_DOOM_ENTRY)

// Measures the time while reading the response info from a disk cache entry.
EVENT_TYPE(HTTP_CACHE_READ_INFO)

// Measures the time that an HttpCache::Transaction is stalled waiting for
// the cache entry to become available (for example if we are waiting for
// exclusive access to an existing entry).
EVENT_TYPE(HTTP_CACHE_WAITING)

// ------------------------------------------------------------------------
// HttpNetworkTransaction
// ------------------------------------------------------------------------

// Measures the time taken to send the request to the server.
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST)

// This event is sent for a HTTP request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST_HEADERS)

// This event is sent for a tunnel request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_TUNNEL_HEADERS)

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_HEADERS)

// Measures the time to resolve the canonical name for HTTP Negotiate
// authentication scheme.
EVENT_TYPE(HTTP_TRANSACTION_RESOLVE_CANONICAL_NAME)

// Measures the time to read the entity body from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_BODY)

// Measures the time taken to read the response out of the socket before
// restarting for authentication, on keep alive connections.
EVENT_TYPE(HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART)

// ------------------------------------------------------------------------
// SpdyNetworkTransaction
// ------------------------------------------------------------------------

// Measures the time taken to get a spdy stream.
EVENT_TYPE(SPDY_TRANSACTION_INIT_CONNECTION)

// Measures the time taken to send the request to the server.
EVENT_TYPE(SPDY_TRANSACTION_SEND_REQUEST)

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(SPDY_TRANSACTION_READ_HEADERS)

// Measures the time to read the entity body from the server.
EVENT_TYPE(SPDY_TRANSACTION_READ_BODY)

// ------------------------------------------------------------------------
// SpdyStream
// ------------------------------------------------------------------------

// Measures the time taken to send headers on a stream.
EVENT_TYPE(SPDY_STREAM_SEND_HEADERS)

// Measures the time taken to send the body (e.g. a POST) on a stream.
EVENT_TYPE(SPDY_STREAM_SEND_BODY)

// Measures the time taken to read headers on a stream.
EVENT_TYPE(SPDY_STREAM_READ_HEADERS)

// Measures the time taken to read the body on a stream.
EVENT_TYPE(SPDY_STREAM_READ_BODY)

// Logs that a stream attached to a pushed stream.
EVENT_TYPE(SPDY_STREAM_ADOPTED_PUSH_STREAM)

// ------------------------------------------------------------------------
// HttpStreamParser
// ------------------------------------------------------------------------

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_STREAM_PARSER_READ_HEADERS)

// ------------------------------------------------------------------------
// SocketStream
// ------------------------------------------------------------------------

// Measures the time between SocketStream::Connect() and
// SocketStream::DidEstablishConnection()
//
// For the BEGIN phase, the following parameters are attached:
//   {
//      "url": <String of URL being loaded>
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>
//   }
EVENT_TYPE(SOCKET_STREAM_CONNECT)

// A message sent on the SocketStream.
EVENT_TYPE(SOCKET_STREAM_SENT)

// A message received on the SocketStream.
EVENT_TYPE(SOCKET_STREAM_RECEIVED)

// ------------------------------------------------------------------------
// SOCKS5ClientSocket
// ------------------------------------------------------------------------

// The time spent sending the "greeting" to the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_WRITE)

// The time spent waiting for the "greeting" response from the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_READ)

// The time spent sending the CONNECT request to the SOCKS server.
EVENT_TYPE(SOCKS5_HANDSHAKE_WRITE)

// The time spent waiting for the response to the CONNECT request.
EVENT_TYPE(SOCKS5_HANDSHAKE_READ)
