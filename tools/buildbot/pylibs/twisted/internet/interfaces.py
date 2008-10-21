# Copyright (c) 2001-2008 Twisted Matrix Laboratories.
# See LICENSE for details.


"""
Interface documentation.

Maintainer: U{Itamar Shtull-Trauring<mailto:twisted@itamarst.org>}
"""

from zope.interface import Interface


class IAddress(Interface):
    """An address, e.g. a TCP (host, port).

    Default implementations are in L{twisted.internet.address}.
    """


### Reactor Interfaces

class IConnector(Interface):
    """Object used to interface between connections and protocols.

    Each IConnector manages one connection.
    """

    def stopConnecting():
        """Stop attempting to connect."""

    def disconnect():
        """Disconnect regardless of the connection state.

        If we are connected, disconnect, if we are trying to connect,
        stop trying.
        """

    def connect():
        """Try to connect to remote address."""

    def getDestination():
        """Return destination this will try to connect to.

        @return: An object which provides L{IAddress}.
        """


class IResolverSimple(Interface):
    def getHostByName(name, timeout = (1, 3, 11, 45)):
        """Resolve the domain name C{name} into an IP address.

        @type name: C{str}
        @type timeout: C{tuple}
        @rtype: L{twisted.internet.defer.Deferred}
        @return: The callback of the Deferred that is returned will be
        passed a string that represents the IP address of the specified
        name, or the errback will be called if the lookup times out.  If
        multiple types of address records are associated with the name,
        A6 records will be returned in preference to AAAA records, which
        will be returned in preference to A records.  If there are multiple
        records of the type to be returned, one will be selected at random.

        @raise twisted.internet.defer.TimeoutError: Raised (asynchronously)
        if the name cannot be resolved within the specified timeout period.
        """

class IResolver(IResolverSimple):
    def lookupRecord(name, cls, type, timeout = 10):
        """Lookup the records associated with the given name
           that are of the given type and in the given class.
        """

    def query(query, timeout = 10):
        """Interpret and dispatch a query object to the appropriate
        lookup* method.
        """

    def lookupAddress(name, timeout = 10):
        """Lookup the A records associated with C{name}."""

    def lookupAddress6(name, timeout = 10):
        """Lookup all the A6 records associated with C{name}."""

    def lookupIPV6Address(name, timeout = 10):
        """Lookup all the AAAA records associated with C{name}."""

    def lookupMailExchange(name, timeout = 10):
        """Lookup the MX records associated with C{name}."""

    def lookupNameservers(name, timeout = 10):
        """Lookup the the NS records associated with C{name}."""

    def lookupCanonicalName(name, timeout = 10):
        """Lookup the CNAME records associated with C{name}."""

    def lookupMailBox(name, timeout = 10):
        """Lookup the MB records associated with C{name}."""

    def lookupMailGroup(name, timeout = 10):
        """Lookup the MG records associated with C{name}."""

    def lookupMailRename(name, timeout = 10):
        """Lookup the MR records associated with C{name}."""

    def lookupPointer(name, timeout = 10):
        """Lookup the PTR records associated with C{name}."""

    def lookupAuthority(name, timeout = 10):
        """Lookup the SOA records associated with C{name}."""

    def lookupNull(name, timeout = 10):
        """Lookup the NULL records associated with C{name}."""

    def lookupWellKnownServices(name, timeout = 10):
        """Lookup the WKS records associated with C{name}."""

    def lookupHostInfo(name, timeout = 10):
        """Lookup the HINFO records associated with C{name}."""

    def lookupMailboxInfo(name, timeout = 10):
        """Lookup the MINFO records associated with C{name}."""

    def lookupText(name, timeout = 10):
        """Lookup the TXT records associated with C{name}."""

    def lookupResponsibility(name, timeout = 10):
        """Lookup the RP records associated with C{name}."""

    def lookupAFSDatabase(name, timeout = 10):
        """Lookup the AFSDB records associated with C{name}."""

    def lookupService(name, timeout = 10):
        """Lookup the SRV records associated with C{name}."""

    def lookupAllRecords(name, timeout = 10):
        """Lookup all records associated with C{name}."""

    def lookupZone(name, timeout = 10):
        """Perform a zone transfer for the given C{name}."""


class IReactorArbitrary(Interface):
    def listenWith(portType, *args, **kw):
        """Start an instance of the given C{portType} listening.

        @type portType: type which implements L{IListeningPort}

        @param portType: The object given by C{portType(*args, **kw)} will be
                         started listening.

        @return: an object which provides L{IListeningPort}.
        """

    def connectWith(connectorType, *args, **kw):
        """
        Start an instance of the given C{connectorType} connecting.

        @type connectorType: type which implements L{IConnector}

        @param connectorType: The object given by C{connectorType(*args, **kw)}
                              will be started connecting.

        @return:  An object which provides L{IConnector}.
        """

class IReactorTCP(Interface):

    def listenTCP(port, factory, backlog=50, interface=''):
        """Connects a given protocol factory to the given numeric TCP/IP port.

        @param port: a port number on which to listen

        @param factory: a L{twisted.internet.protocol.ServerFactory} instance

        @param backlog: size of the listen queue

        @param interface: the hostname to bind to, defaults to '' (all)

        @return: an object that provides L{IListeningPort}.

        @raise CannotListenError: as defined here
                                  L{twisted.internet.error.CannotListenError},
                                  if it cannot listen on this port (e.g., it
                                  cannot bind to the required port number)
        """

    def connectTCP(host, port, factory, timeout=30, bindAddress=None):
        """Connect a TCP client.

        @param host: a host name

        @param port: a port number

        @param factory: a L{twisted.internet.protocol.ClientFactory} instance

        @param timeout: number of seconds to wait before assuming the
                        connection has failed.

        @param bindAddress: a (host, port) tuple of local address to bind
                            to, or None.

        @return: An object which provides L{IConnector}. This connector will
                 call various callbacks on the factory when a connection is
                 made, failed, or lost - see
                 L{ClientFactory<twisted.internet.protocol.ClientFactory>}
                 docs for details.
        """

class IReactorSSL(Interface):

    def connectSSL(host, port, factory, contextFactory, timeout=30, bindAddress=None):
        """Connect a client Protocol to a remote SSL socket.

        @param host: a host name

        @param port: a port number

        @param factory: a L{twisted.internet.protocol.ClientFactory} instance

        @param contextFactory: a L{twisted.internet.ssl.ClientContextFactory} object.

        @param timeout: number of seconds to wait before assuming the
                        connection has failed.

        @param bindAddress: a (host, port) tuple of local address to bind to,
                            or C{None}.

        @return: An object which provides L{IConnector}.
        """

    def listenSSL(port, factory, contextFactory, backlog=50, interface=''):
        """
        Connects a given protocol factory to the given numeric TCP/IP port.
        The connection is a SSL one, using contexts created by the context
        factory.

        @param port: a port number on which to listen

        @param factory: a L{twisted.internet.protocol.ServerFactory} instance

        @param contextFactory: a L{twisted.internet.ssl.ContextFactory} instance

        @param backlog: size of the listen queue

        @param interface: the hostname to bind to, defaults to '' (all)
        """


class IReactorUNIX(Interface):
    """UNIX socket methods."""

    def connectUNIX(address, factory, timeout=30, checkPID=0):
        """Connect a client protocol to a UNIX socket.

        @param address: a path to a unix socket on the filesystem.

        @param factory: a L{twisted.internet.protocol.ClientFactory} instance

        @param timeout: number of seconds to wait before assuming the connection
            has failed.

        @param checkPID: if True, check for a pid file to verify that a server
            is listening.

        @return: An object which provides L{IConnector}.
        """

    def listenUNIX(address, factory, backlog=50, mode=0666, wantPID=0):
        """Listen on a UNIX socket.

        @param address: a path to a unix socket on the filesystem.

        @param factory: a L{twisted.internet.protocol.Factory} instance.

        @param backlog: number of connections to allow in backlog.

        @param mode: mode to set on the unix socket.

        @param wantPID: if True, create a pidfile for the socket.

        @return: An object which provides L{IListeningPort}.
        """


class IReactorUNIXDatagram(Interface):
    """datagram UNIX socket methods."""

    def connectUNIXDatagram(address, protocol, maxPacketSize=8192, mode=0666, bindAddress=None):
        """Connect a client protocol to a datagram UNIX socket.

        @param address: a path to a unix socket on the filesystem.

        @param protocol: a L{twisted.internet.protocol.ConnectedDatagramProtocol} instance

        @param maxPacketSize: maximum packet size to accept

        @param mode: mode to set on the unix socket.

        @param bindAddress: address to bind to

        @return: An object which provides L{IConnector}.
        """

    def listenUNIXDatagram(address, protocol, maxPacketSize=8192, mode=0666):
        """Listen on a datagram UNIX socket.

        @param address: a path to a unix socket on the filesystem.

        @param protocol: a L{twisted.internet.protocol.DatagramProtocol} instance.

        @param maxPacketSize: maximum packet size to accept

        @param mode: mode to set on the unix socket.

        @return: An object which provides L{IListeningPort}.
        """


class IReactorUDP(Interface):
    """UDP socket methods.

    IMPORTANT: This is an experimental new interface. It may change
    without backwards compatability. Suggestions are welcome.
    """

    def listenUDP(port, protocol, interface='', maxPacketSize=8192):
        """Connects a given DatagramProtocol to the given numeric UDP port.

        @return: object which provides L{IListeningPort}.
        """

    def connectUDP(remotehost, remoteport, protocol, localport=0,
                  interface='', maxPacketSize=8192):
        """DEPRECATED.

        Connects a L{twisted.internet.protocol.ConnectedDatagramProtocol}
        instance to a UDP port.
        """


class IReactorMulticast(Interface):
    """UDP socket methods that support multicast.

    IMPORTANT: This is an experimental new interface. It may change
    without backwards compatability. Suggestions are welcome.
    """

    def listenMulticast(port, protocol, interface='', maxPacketSize=8192,
                        listenMultiple=False):
        """
        Connects a given
        L{DatagramProtocol<twisted.internet.protocol.DatagramProtocol>} to the
        given numeric UDP port.

        @param listenMultiple: boolean indicating whether multiple sockets can
                               bind to same UDP port.

        @returns: An object which provides L{IListeningPort}.
        """


class IReactorProcess(Interface):

    def spawnProcess(processProtocol, executable, args=(), env={}, path=None,
                     uid=None, gid=None, usePTY=0, childFDs=None):
        """
        Spawn a process, with a process protocol.

        @type processProtocol: L{IProcessProtocol} provider
        @param processProtocol: An object which will be notified of all
            events related to the created process.

        @param executable: the file name to spawn - the full path should be
                           used.

        @param args: the command line arguments to pass to the process; a
                     sequence of strings. The first string should be the
                     executable's name.

        @param env: the environment variables to pass to the processs; a
                    dictionary of strings. If 'None', use os.environ.

        @param path: the path to run the subprocess in - defaults to the
                     current directory.

        @param uid: user ID to run the subprocess as. (Only available on
                    POSIX systems.)

        @param gid: group ID to run the subprocess as. (Only available on
                    POSIX systems.)

        @param usePTY: if true, run this process in a pseudo-terminal.
                       optionally a tuple of (masterfd, slavefd, ttyname),
                       in which case use those file descriptors.
                       (Not available on all systems.)

        @param childFDs: A dictionary mapping file descriptors in the new child
                         process to an integer or to the string 'r' or 'w'.

                         If the value is an integer, it specifies a file
                         descriptor in the parent process which will be mapped
                         to a file descriptor (specified by the key) in the
                         child process.  This is useful for things like inetd
                         and shell-like file redirection.

                         If it is the string 'r', a pipe will be created and
                         attached to the child at that file descriptor: the
                         child will be able to write to that file descriptor
                         and the parent will receive read notification via the
                         L{IProcessProtocol.childDataReceived} callback.  This
                         is useful for the child's stdout and stderr.

                         If it is the string 'w', similar setup to the previous
                         case will occur, with the pipe being readable by the
                         child instead of writeable.  The parent process can
                         write to that file descriptor using
                         L{IProcessTransport.writeToChild}.  This is useful for
                         the child's stdin.

                         If childFDs is not passed, the default behaviour is to
                         use a mapping that opens the usual stdin/stdout/stderr
                         pipes.

        @see: L{twisted.internet.protocol.ProcessProtocol}

        @return: An object which provides L{IProcessTransport}.

        @raise OSError: Raised with errno EAGAIN or ENOMEM if there are
                        insufficient system resources to create a new process.
        """

class IReactorTime(Interface):
    """
    Time methods that a Reactor should implement.
    """

    def seconds():
        """
        Get the current time in seconds.

        @return: A number-like object of some sort.
        """


    def callLater(delay, callable, *args, **kw):
        """
        Call a function later.

        @type delay:  C{float}
        @param delay: the number of seconds to wait.

        @param callable: the callable object to call later.

        @param args: the arguments to call it with.

        @param kw: the keyword arguments to call it with.

        @return: An object which provides L{IDelayedCall} and can be used to
                 cancel the scheduled call, by calling its C{cancel()} method.
                 It also may be rescheduled by calling its C{delay()} or
                 C{reset()} methods.
        """

    def cancelCallLater(callID):
        """
        This method is deprecated.

        Cancel a call that would happen later.

        @param callID: this is an opaque identifier returned from C{callLater}
                       that will be used to cancel a specific call.

        @raise ValueError: if the callID is not recognized.
        """

    def getDelayedCalls():
        """
        Retrieve all currently scheduled delayed calls.

        @return: A tuple of all L{IDelayedCall} providers representing all
                 currently scheduled calls. This is everything that has been
                 returned by C{callLater} but not yet called or canceled.
        """


class IDelayedCall(Interface):
    """
    A scheduled call.

    There are probably other useful methods we can add to this interface;
    suggestions are welcome.
    """

    def getTime():
        """
        Get time when delayed call will happen.

        @return: time in seconds since epoch (a float).
        """

    def cancel():
        """
        Cancel the scheduled call.

        @raises twisted.internet.error.AlreadyCalled: if the call has already
            happened.
        @raises twisted.internet.error.AlreadyCancelled: if the call has already
            been cancelled.
        """

    def delay(secondsLater):
        """
        Delay the scheduled call.

        @param secondsLater: how many seconds from its current firing time to delay

        @raises twisted.internet.error.AlreadyCalled: if the call has already
            happened.
        @raises twisted.internet.error.AlreadyCancelled: if the call has already
            been cancelled.
        """

    def reset(secondsFromNow):
        """
        Reset the scheduled call's timer.

        @param secondsFromNow: how many seconds from now it should fire,
            equivalent to C{.cancel()} and then doing another
            C{reactor.callLater(secondsLater, ...)}

        @raises twisted.internet.error.AlreadyCalled: if the call has already
            happened.
        @raises twisted.internet.error.AlreadyCancelled: if the call has already
            been cancelled.
        """

    def active():
        """
        @return: True if this call is still active, False if it has been
                 called or cancelled.
        """

class IReactorThreads(Interface):
    """Dispatch methods to be run in threads.

    Internally, this should use a thread pool and dispatch methods to them.
    """

    def callInThread(callable, *args, **kwargs):
        """Run the callable object in a separate thread.
        """

    def callFromThread(callable, *args, **kw):
        """Cause a function to be executed by the reactor thread.

        Use this method when you want to run a function in the reactor's thread
        from another thread.  Calling callFromThread should wake up the main
        thread (where reactor.run() is executing) and run the given callable in
        that thread.

        Obviously, the callable must be thread safe.  (If you want to call a
        function in the next mainloop iteration, but you're in the same thread,
        use callLater with a delay of 0.)
        """

    def suggestThreadPoolSize(size):
        """
        Suggest the size of the internal threadpool used to dispatch functions
        passed to L{callInThread}.
        """


class IReactorCore(Interface):
    """Core methods that a Reactor must implement.
    """

    def resolve(name, timeout=10):
        """Return a L{twisted.internet.defer.Deferred} that will resolve a hostname.
        """


    def run():
        """Fire 'startup' System Events, move the reactor to the 'running'
        state, then run the main loop until it is stopped with stop() or
        crash().
        """

    def stop():
        """Fire 'shutdown' System Events, which will move the reactor to the
        'stopped' state and cause reactor.run() to exit. """

    def crash():
        """Stop the main loop *immediately*, without firing any system events.

        This is named as it is because this is an extremely "rude" thing to do;
        it is possible to lose data and put your system in an inconsistent
        state by calling this.  However, it is necessary, as sometimes a system
        can become wedged in a pre-shutdown call.
        """

    def iterate(delay=0):
        """Run the main loop's I/O polling function for a period of time.

        This is most useful in applications where the UI is being drawn "as
        fast as possible", such as games. All pending L{IDelayedCall}s will
        be called.

        The reactor must have been started (via the run() method) prior to
        any invocations of this method.  It must also be stopped manually
        after the last call to this method (via the stop() method).  This
        method is not re-entrant: you must not call it recursively; in
        particular, you must not call it while the reactor is running.
        """

    def fireSystemEvent(eventType):
        """Fire a system-wide event.

        System-wide events are things like 'startup', 'shutdown', and
        'persist'.
        """

    def addSystemEventTrigger(phase, eventType, callable, *args, **kw):
        """Add a function to be called when a system event occurs.

        Each "system event" in Twisted, such as 'startup', 'shutdown', and
        'persist', has 3 phases: 'before', 'during', and 'after' (in that
        order, of course).  These events will be fired internally by the
        Reactor.

        An implementor of this interface must only implement those events
        described here.

        Callbacks registered for the "before" phase may return either None or a
        Deferred.  The "during" phase will not execute until all of the
        Deferreds from the "before" phase have fired.

        Once the "during" phase is running, all of the remaining triggers must
        execute; their return values must be ignored.

        @param phase: a time to call the event -- either the string 'before',
                      'after', or 'during', describing when to call it
                      relative to the event's execution.

        @param eventType: this is a string describing the type of event.

        @param callable: the object to call before shutdown.

        @param args: the arguments to call it with.

        @param kw: the keyword arguments to call it with.

        @return: an ID that can be used to remove this call with
                 removeSystemEventTrigger.
        """

    def removeSystemEventTrigger(triggerID):
        """Removes a trigger added with addSystemEventTrigger.

        @param triggerID: a value returned from addSystemEventTrigger.

        @raise KeyError: If there is no system event trigger for the given
            C{triggerID}.

        @raise ValueError: If there is no system event trigger for the given
            C{triggerID}.

        @raise TypeError: If there is no system event trigger for the given
            C{triggerID}.
        """

    def callWhenRunning(callable, *args, **kw):
        """Call a function when the reactor is running.

        If the reactor has not started, the callable will be scheduled
        to run when it does start. Otherwise, the callable will be invoked
        immediately.

        @param callable: the callable object to call later.

        @param args: the arguments to call it with.

        @param kw: the keyword arguments to call it with.

        @return: None if the callable was invoked, otherwise a system
                 event id for the scheduled call.
        """


class IReactorPluggableResolver(Interface):
    """A reactor with a pluggable name resolver interface.
    """
    def installResolver(resolver):
        """Set the internal resolver to use to for name lookups.

        @type resolver: An object implementing the L{IResolverSimple} interface
        @param resolver: The new resolver to use.

        @return: The previously installed resolver.
        """


class IReactorFDSet(Interface):
    """
    Implement me to be able to use
    L{FileDescriptor<twisted.internet.abstract.FileDescriptor>} type resources.

    This assumes that your main-loop uses UNIX-style numeric file descriptors
    (or at least similarly opaque IDs returned from a .fileno() method)
    """

    def addReader(reader):
        """I add reader to the set of file descriptors to get read events for.

        @param reader: An L{IReadDescriptor} provider that will be checked for
                       read events until it is removed from the reactor with
                       L{removeReader}.

        @return: C{None}.
        """

    def addWriter(writer):
        """I add writer to the set of file descriptors to get write events for.

        @param writer: An L{IWriteDescriptor} provider that will be checked for
                       read events until it is removed from the reactor with
                       L{removeWriter}.

        @return: C{None}.
        """

    def removeReader(reader):
        """Removes an object previously added with L{addReader}.

        @return: C{None}.
        """

    def removeWriter(writer):
        """Removes an object previously added with L{addWriter}.

        @return: C{None}.
        """

    def removeAll():
        """Remove all readers and writers.

        Should not remove reactor internal reactor connections (like a waker).

        @return: A list of L{IReadDescriptor} and L{IWriteDescriptor} providers
                 which were removed.
        """


    def getReaders():
        """
        Return the list of file descriptors currently monitored for input
        events by the reactor.

        @return: the list of file descriptors monitored for input events.
        @rtype: C{list} of C{IReadDescriptor}
        """


    def getWriters():
        """
        Return the list file descriptors currently monitored for output events
        by the reactor.

        @return: the list of file descriptors monitored for output events.
        @rtype: C{list} of C{IWriteDescriptor}
        """



class IListeningPort(Interface):
    """A listening port.
    """

    def startListening():
        """Start listening on this port.

        @raise CannotListenError: If it cannot listen on this port (e.g., it is
                                  a TCP port and it cannot bind to the required
                                  port number).
        """

    def stopListening():
        """Stop listening on this port.

        If it does not complete immediately, will return Deferred that fires
        upon completion.
        """

    def getHost():
        """Get the host that this port is listening for.

        @return: An L{IAddress} provider.
        """


class ILoggingContext(Interface):
    """
    Give context information that will be used to log events generated by
    this item.
    """
    def logPrefix():
        """
        @return: Prefix used during log formatting to indicate context.
        @rtype: C{str}
        """


class IFileDescriptor(ILoggingContext):
    """
    A file descriptor.
    """

    def fileno():
        """
        @return: The platform-specified representation of a file-descriptor
                 number.
        """

    def connectionLost(reason):
        """Called when the connection was lost.

        This is called when the connection on a selectable object has been
        lost.  It will be called whether the connection was closed explicitly,
        an exception occurred in an event handler, or the other end of the
        connection closed it first.

        See also L{IHalfCloseableDescriptor} if your descriptor wants to be
        notified separately of the two halves of the connection being closed.

        @param reason: A failure instance indicating the reason why the
                       connection was lost.  L{error.ConnectionLost} and
                       L{error.ConnectionDone} are of special note, but the
                       failure may be of other classes as well.
        """

class IReadDescriptor(IFileDescriptor):

    def doRead():
        """Some data is available for reading on your descriptor.
        """


class IWriteDescriptor(IFileDescriptor):

    def doWrite():
        """Some data can be written to your descriptor.
        """


class IReadWriteDescriptor(IReadDescriptor, IWriteDescriptor):
    """I am a L{FileDescriptor<twisted.internet.abstract.FileDescriptor>} that can both read and write.
    """


class IHalfCloseableDescriptor(Interface):
    """A descriptor that can be half-closed."""

    def writeConnectionLost(reason):
        """Indicates write connection was lost."""

    def readConnectionLost(reason):
        """Indicates read connection was lost."""


class ISystemHandle(Interface):
    """An object that wraps a networking OS-specific handle."""

    def getHandle():
        """Return a system- and reactor-specific handle.

        This might be a socket.socket() object, or some other type of
        object, depending on which reactor is being used. Use and
        manipulate at your own risk.

        This might be used in cases where you want to set specific
        options not exposed by the Twisted APIs.
        """


class IConsumer(Interface):
    """A consumer consumes data from a producer."""

    def registerProducer(producer, streaming):
        """
        Register to receive data from a producer.

        This sets self to be a consumer for a producer.  When this object runs
        out of data (as when a send(2) call on a socket succeeds in moving the
        last data from a userspace buffer into a kernelspace buffer), it will
        ask the producer to resumeProducing().

        For L{IPullProducer} providers, C{resumeProducing} will be called once
        each time data is required.

        For L{IPushProducer} providers, C{pauseProducing} will be called
        whenever the write buffer fills up and C{resumeProducing} will only be
        called when it empties.

        @type producer: L{IProducer} provider

        @type streaming: C{bool}
        @param streaming: C{True} if C{producer} provides L{IPushProducer},
        C{False} if C{producer} provides L{IPullProducer}.

        @return: C{None}
        """

    def unregisterProducer():
        """Stop consuming data from a producer, without disconnecting.
        """

    def write(data):
        """The producer will write data by calling this method."""

class IFinishableConsumer(IConsumer):
    """A Consumer for producers that finish.
    """
    def finish():
        """The producer has finished producing."""

class IProducer(Interface):
    """A producer produces data for a consumer.

    Typically producing is done by calling the write method of an class
    implementing L{IConsumer}.
    """

    def stopProducing():
        """Stop producing data.

        This tells a producer that its consumer has died, so it must stop
        producing data for good.
        """


class IPushProducer(IProducer):
    """
    A push producer, also known as a streaming producer is expected to
    produce (write to this consumer) data on a continous basis, unless
    it has been paused. A paused push producer will resume producing
    after its resumeProducing() method is called.   For a push producer
    which is not pauseable, these functions may be noops.
    """

    def pauseProducing():
        """Pause producing data.

        Tells a producer that it has produced too much data to process for
        the time being, and to stop until resumeProducing() is called.
        """
    def resumeProducing():
        """Resume producing data.

        This tells a producer to re-add itself to the main loop and produce
        more data for its consumer.
        """

class IPullProducer(IProducer):
    """
    A pull producer, also known as a non-streaming producer, is
    expected to produce data each time resumeProducing() is called.
    """

    def resumeProducing():
        """Produce data for the consumer a single time.

        This tells a producer to produce data for the consumer once
        (not repeatedly, once only). Typically this will be done
        by calling the consumer's write() method a single time with
        produced data.
        """

class IProtocol(Interface):

    def dataReceived(data):
        """Called whenever data is received.

        Use this method to translate to a higher-level message.  Usually, some
        callback will be made upon the receipt of each complete protocol
        message.

        @param data: a string of indeterminate length.  Please keep in mind
            that you will probably need to buffer some data, as partial
            (or multiple) protocol messages may be received!  I recommend
            that unit tests for protocols call through to this method with
            differing chunk sizes, down to one byte at a time.
        """

    def connectionLost(reason):
        """Called when the connection is shut down.

        Clear any circular references here, and any external references
        to this Protocol.  The connection has been closed. The C{reason}
        Failure wraps a L{twisted.internet.error.ConnectionDone} or
        L{twisted.internet.error.ConnectionLost} instance (or a subclass
        of one of those).

        @type reason: L{twisted.python.failure.Failure}
        """

    def makeConnection(transport):
        """Make a connection to a transport and a server.
        """

    def connectionMade():
        """Called when a connection is made.

        This may be considered the initializer of the protocol, because
        it is called when the connection is completed.  For clients,
        this is called once the connection to the server has been
        established; for servers, this is called after an accept() call
        stops blocking and a socket has been received.  If you need to
        send any greeting or initial message, do it here.
        """


class IProcessProtocol(Interface):
    """
    Interface for process-related event handlers.
    """

    def makeConnection(process):
        """
        Called when the process has been created.

        @type process: L{IProcessTransport} provider
        @param process: An object representing the process which has been
            created and associated with this protocol.
        """


    def childDataReceived(childFD, data):
        """
        Called when data arrives from the child process.

        @type childFD: C{int}
        @param childFD: The file descriptor from which the data was
            received.

        @type data: C{str}
        @param data: The data read from the child's file descriptor.
        """


    def childConnectionLost(childFD):
        """
        Called when a file descriptor associated with the child process is
        closed.

        @type childFD: C{int}
        @param childFD: The file descriptor which was closed.
        """


    def processEnded(reason):
        """
        Called when the child process exits.

        @type reason: L{twisted.python.failure.Failure}
        @param reason: A failure giving the reason the child process
            terminated.  The type of exception for this failure is either
            L{twisted.internet.error.ProcessDone} or
            L{twisted.internet.error.ProcessTerminated}.
        """



class IHalfCloseableProtocol(Interface):
    """Implemented to indicate they want notification of half-closes.

    TCP supports the notion of half-closing the connection, e.g.
    closing the write side but still not stopping reading. A protocol
    that implements this interface will be notified of such events,
    instead of having connectionLost called.
    """

    def readConnectionLost():
        """Notification of the read connection being closed.

        This indicates peer did half-close of write side. It is now
        the responsiblity of the this protocol to call
        loseConnection().  In addition, the protocol MUST make sure a
        reference to it still exists (i.e. by doing a callLater with
        one of its methods, etc.)  as the reactor will only have a
        reference to it if it is writing.

        If the protocol does not do so, it might get garbage collected
        without the connectionLost method ever being called.
        """

    def writeConnectionLost():
        """Notification of the write connection being closed.

        This will never be called for TCP connections as TCP does not
        support notification of this type of half-close.
        """


class IProtocolFactory(Interface):
    """Interface for protocol factories.
    """

    def buildProtocol(addr):
        """Called when a connection has been established to addr.

        If None is returned, the connection is assumed to have been refused,
        and the Port will close the connection.

        @type addr: (host, port)
        @param addr: The address of the newly-established connection

        @return: None if the connection was refused, otherwise an object
                 providing L{IProtocol}.
        """

    def doStart():
        """Called every time this is connected to a Port or Connector."""

    def doStop():
        """Called every time this is unconnected from a Port or Connector."""


class ITransport(Interface):
    """I am a transport for bytes.

    I represent (and wrap) the physical connection and synchronicity
    of the framework which is talking to the network.  I make no
    representations about whether calls to me will happen immediately
    or require returning to a control loop, or whether they will happen
    in the same or another thread.  Consider methods of this class
    (aside from getPeer) to be 'thrown over the wall', to happen at some
    indeterminate time.
    """

    def write(data):
        """Write some data to the physical connection, in sequence, in a
        non-blocking fashion.

        If possible, make sure that it is all written.  No data will
        ever be lost, although (obviously) the connection may be closed
        before it all gets through.
        """

    def writeSequence(data):
        """Write a list of strings to the physical connection.

        If possible, make sure that all of the data is written to
        the socket at once, without first copying it all into a
        single string.
        """

    def loseConnection():
        """Close my connection, after writing all pending data.

        Note that if there is a registered producer on a transport it
        will not be closed until the producer has been unregistered.
        """

    def getPeer():
        """Get the remote address of this connection.

        Treat this method with caution.  It is the unfortunate result of the
        CGI and Jabber standards, but should not be considered reliable for
        the usual host of reasons; port forwarding, proxying, firewalls, IP
        masquerading, etc.

        @return: An L{IAddress} provider.
        """

    def getHost():
        """
        Similar to getPeer, but returns an address describing this side of the
        connection.

        @return: An L{IAddress} provider.
        """


class ITCPTransport(ITransport):
    """A TCP based transport."""

    def loseWriteConnection():
        """Half-close the write side of a TCP connection.

        If the protocol instance this is attached to provides
        IHalfCloseableProtocol, it will get notified when the operation is
        done. When closing write connection, as with loseConnection this will
        only happen when buffer has emptied and there is no registered
        producer.
        """

    def getTcpNoDelay():
        """Return if TCP_NODELAY is enabled."""

    def setTcpNoDelay(enabled):
        """Enable/disable TCP_NODELAY.

        Enabling TCP_NODELAY turns off Nagle's algorithm. Small packets are
        sent sooner, possibly at the expense of overall throughput."""

    def getTcpKeepAlive():
        """Return if SO_KEEPALIVE enabled."""

    def setTcpKeepAlive(enabled):
        """Enable/disable SO_KEEPALIVE.

        Enabling SO_KEEPALIVE sends packets periodically when the connection
        is otherwise idle, usually once every two hours. They are intended
        to allow detection of lost peers in a non-infinite amount of time."""

    def getHost():
        """Returns L{IPv4Address}."""

    def getPeer():
        """Returns L{IPv4Address}."""


class ITLSTransport(ITCPTransport):
    """A TCP transport that supports switching to TLS midstream.

    Once TLS mode is started the transport will implement L{ISSLTransport}.
    """

    def startTLS(contextFactory):
        """Initiate TLS negotiation.

        @param contextFactory: A context factory (see L{ssl.py<twisted.internet.ssl>})
        """

class ISSLTransport(ITCPTransport):
    """A SSL/TLS based transport."""

    def getPeerCertificate():
        """Return an object with the peer's certificate info."""


class IProcessTransport(ITransport):
    """A process transport.

    @ivar pid: The Process-ID of this process.
    """

    def closeStdin():
        """Close stdin after all data has been written out."""

    def closeStdout():
        """Close stdout."""

    def closeStderr():
        """Close stderr."""

    def closeChildFD(descriptor):
        """
        Close a file descriptor which is connected to the child process, identified
        by its FD in the child process.
        """

    def writeToChild(childFD, data):
        """
        Similar to L{ITransport.write} but also allows the file descriptor in
        the child process which will receive the bytes to be specified.

        This is not available on all platforms.

        @type childFD: C{int}
        @param childFD: The file descriptor to which to write.

        @type data: C{str}
        @param data: The bytes to write.

        @return: C{None}
        """

    def loseConnection():
        """Close stdin, stderr and stdout."""

    def signalProcess(signalID):
        """Send a signal to the process.

        @param signalID: can be
          - one of C{\"HUP\"}, C{\"KILL\"}, C{\"STOP\"}, or C{\"INT\"}.
              These will be implemented in a
              cross-platform manner, and so should be used
              if possible.
          - an integer, where it represents a POSIX
              signal ID.

        @raise twisted.internet.error.ProcessExitedAlready: The process has
        already exited.
        """


class IServiceCollection(Interface):
    """An object which provides access to a collection of services."""

    def getServiceNamed(serviceName):
        """Retrieve the named service from this application.

        Raise a KeyError if there is no such service name.
        """

    def addService(service):
        """Add a service to this collection.
        """

    def removeService(service):
        """Remove a service from this collection."""


class IUDPTransport(Interface):
    """Transport for UDP DatagramProtocols."""

    def write(packet, addr=None):
        """Write packet to given address.

        @param addr: a tuple of (ip, port). For connected transports must
                     be the address the transport is connected to, or None.
                     In non-connected mode this is mandatory.

        @raise twisted.internet.error.MessageLengthError: C{packet} was too
        long.
        """

    def connect(host, port):
        """Connect the transport to an address.

        This changes it to connected mode. Datagrams can only be sent to
        this address, and will only be received from this address. In addition
        the protocol's connectionRefused method might get called if destination
        is not receiving datagrams.

        @param host: an IP address, not a domain name ('127.0.0.1', not 'localhost')
        @param port: port to connect to.
        """

    def getHost():
        """Returns IPv4Address."""

    def stopListening():
        """Stop listening on this port.

        If it does not complete immediately, will return Deferred that fires
        upon completion.
        """


class IUDPConnectedTransport(Interface):
    """DEPRECATED. Transport for UDP ConnectedPacketProtocols."""

    def write(packet):
        """Write packet to address we are connected to."""

    def getHost():
        """Returns UNIXAddress."""


class IUNIXDatagramTransport(Interface):
    """Transport for UDP PacketProtocols."""

    def write(packet, address):
        """Write packet to given address."""

    def getHost():
        """Returns UNIXAddress."""


class IUNIXDatagramConnectedTransport(Interface):
    """Transport for UDP ConnectedPacketProtocols."""

    def write(packet):
        """Write packet to address we are connected to."""

    def getHost():
        """Returns UNIXAddress."""

    def getPeer():
        """Returns UNIXAddress."""


class IMulticastTransport(Interface):
    """Additional functionality for multicast UDP."""

    def getOutgoingInterface():
        """Return interface of outgoing multicast packets."""

    def setOutgoingInterface(addr):
        """Set interface for outgoing multicast packets.

        Returns Deferred of success.
        """

    def getLoopbackMode():
        """Return if loopback mode is enabled."""

    def setLoopbackMode(mode):
        """Set if loopback mode is enabled."""

    def getTTL():
        """Get time to live for multicast packets."""

    def setTTL(ttl):
        """Set time to live on multicast packets."""

    def joinGroup(addr, interface=""):
        """Join a multicast group. Returns Deferred of success or failure.

        If an error occurs, the returned Deferred will fail with
        L{error.MulticastJoinError}.
        """

    def leaveGroup(addr, interface=""):
        """Leave multicast group, return Deferred of success."""
