# -*- test-case-name: twisted.test.test_threadpool -*-
# Copyright (c) 2001-2007 Twisted Matrix Laboratories.
# See LICENSE for details.


"""
twisted.threadpool: a pool of threads to which we dispatch tasks.

In most cases you can just use reactor.callInThread and friends
instead of creating a thread pool directly.
"""

# System Imports
import Queue
import threading
import copy
import sys
import warnings


# Twisted Imports
from twisted.python import log, runtime, context, threadable

WorkerStop = object()


class ThreadPool:
    """
    This class (hopefully) generalizes the functionality of a pool of
    threads to which work can be dispatched.

    callInThread() and stop() should only be called from
    a single thread, unless you make a subclass where stop() and
    _startSomeWorkers() are synchronized.
    """
    min = 5
    max = 20
    joined = False
    started = False
    workers = 0
    name = None

    threadFactory = threading.Thread
    currentThread = staticmethod(threading.currentThread)

    def __init__(self, minthreads=5, maxthreads=20, name=None):
        """
        Create a new threadpool.

        @param minthreads: minimum number of threads in the pool

        @param maxthreads: maximum number of threads in the pool
        """
        assert minthreads >= 0, 'minimum is negative'
        assert minthreads <= maxthreads, 'minimum is greater than maximum'
        self.q = Queue.Queue(0)
        self.min = minthreads
        self.max = maxthreads
        self.name = name
        if runtime.platform.getType() != "java":
            self.waiters = []
            self.threads = []
            self.working = []
        else:
            self.waiters = ThreadSafeList()
            self.threads = ThreadSafeList()
            self.working = ThreadSafeList()

    def start(self):
        """
        Start the threadpool.
        """
        self.joined = False
        self.started = True
        # Start some threads.
        self.adjustPoolsize()

    def startAWorker(self):
        self.workers += 1
        name = "PoolThread-%s-%s" % (self.name or id(self), self.workers)
        newThread = self.threadFactory(target=self._worker, name=name)
        self.threads.append(newThread)
        newThread.start()

    def stopAWorker(self):
        self.q.put(WorkerStop)
        self.workers -= 1

    def __setstate__(self, state):
        self.__dict__ = state
        ThreadPool.__init__(self, self.min, self.max)

    def __getstate__(self):
        state = {}
        state['min'] = self.min
        state['max'] = self.max
        return state

    def _startSomeWorkers(self):
        neededSize = self.q.qsize() + len(self.working)
        # Create enough, but not too many
        while self.workers < min(self.max, neededSize):
            self.startAWorker()

    def dispatch(self, owner, func, *args, **kw):
        """
        DEPRECATED: use L{callInThread} instead.

        Dispatch a function to be a run in a thread.
        """
        warnings.warn("dispatch() is deprecated since Twisted 8.0, "
                      "use callInThread() instead",
                      DeprecationWarning, stacklevel=2)
        self.callInThread(func, *args, **kw)

    def callInThread(self, func, *args, **kw):
        if self.joined:
            return
        ctx = context.theContextTracker.currentContext().contexts[-1]
        o = (ctx, func, args, kw)
        self.q.put(o)
        if self.started:
            self._startSomeWorkers()

    def _runWithCallback(self, callback, errback, func, args, kwargs):
        try:
            result = apply(func, args, kwargs)
        except:
            errback(sys.exc_info()[1])
        else:
            callback(result)

    def dispatchWithCallback(self, owner, callback, errback, func, *args, **kw):
        """
        DEPRECATED: use L{twisted.internet.threads.deferToThread} instead.

        Dispatch a function, returning the result to a callback function.

        The callback function will be called in the thread - make sure it is
        thread-safe.
        """
        warnings.warn("dispatchWithCallback() is deprecated since Twisted 8.0, "
                      "use twisted.internet.threads.deferToThread() instead.",
                      DeprecationWarning, stacklevel=2)
        self.callInThread(
            self._runWithCallback, callback, errback, func, args, kw
        )

    def _worker(self):
        """
        Method used as target of the created threads: retrieve task to run
        from the threadpool, run it, and proceed to the next task until
        threadpool is stopped.
        """
        ct = self.currentThread()
        o = self.q.get()
        while o is not WorkerStop:
            self.working.append(ct)
            ctx, function, args, kwargs = o
            try:
                context.call(ctx, function, *args, **kwargs)
            except:
                context.call(ctx, log.err)
            self.working.remove(ct)
            del o, ctx, function, args, kwargs
            self.waiters.append(ct)
            o = self.q.get()
            self.waiters.remove(ct)

        self.threads.remove(ct)

    def stop(self):
        """
        Shutdown the threads in the threadpool.
        """
        self.joined = True
        threads = copy.copy(self.threads)
        while self.workers:
            self.q.put(WorkerStop)
            self.workers -= 1

        # and let's just make sure
        # FIXME: threads that have died before calling stop() are not joined.
        for thread in threads:
            thread.join()

    def adjustPoolsize(self, minthreads=None, maxthreads=None):
        if minthreads is None:
            minthreads = self.min
        if maxthreads is None:
            maxthreads = self.max

        assert minthreads >= 0, 'minimum is negative'
        assert minthreads <= maxthreads, 'minimum is greater than maximum'

        self.min = minthreads
        self.max = maxthreads
        if not self.started:
            return

        # Kill of some threads if we have too many.
        while self.workers > self.max:
            self.stopAWorker()
        # Start some threads if we have too few.
        while self.workers < self.min:
            self.startAWorker()
        # Start some threads if there is a need.
        self._startSomeWorkers()

    def dumpStats(self):
        log.msg('queue: %s'   % self.q.queue)
        log.msg('waiters: %s' % self.waiters)
        log.msg('workers: %s' % self.working)
        log.msg('total: %s'   % self.threads)


class ThreadSafeList:
    """
    In Jython 2.1 lists aren't thread-safe, so this wraps it.
    """

    def __init__(self):
        self.lock = threading.Lock()
        self.l = []

    def append(self, i):
        self.lock.acquire()
        try:
            self.l.append(i)
        finally:
            self.lock.release()

    def remove(self, i):
        self.lock.acquire()
        try:
            self.l.remove(i)
        finally:
            self.lock.release()

    def __len__(self):
        return len(self.l)

