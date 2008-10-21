# -*- test-case-name: twisted.trial.test.test_reporter -*-
#
# Copyright (c) 2001-2007 Twisted Matrix Laboratories.
# See LICENSE for details.
#
# Maintainer: Jonathan Lange <jml@twistedmatrix.com>

"""
Defines classes that handle the results of tests.
"""

import sys, os
import time
import warnings

from twisted.python import reflect, log
from twisted.python.components import proxyForInterface
from twisted.python.failure import Failure
from twisted.python.util import untilConcludes
from twisted.trial import itrial, util

from zope.interface import implements

pyunit = __import__('unittest')


class BrokenTestCaseWarning(Warning):
    """emitted as a warning when an exception occurs in one of
    setUp, tearDown, setUpClass, or tearDownClass"""


class SafeStream(object):
    """
    Wraps a stream object so that all C{write} calls are wrapped in
    L{untilConcludes}.
    """

    def __init__(self, original):
        self.original = original

    def __getattr__(self, name):
        return getattr(self.original, name)

    def write(self, *a, **kw):
        return untilConcludes(self.original.write, *a, **kw)


class TestResult(pyunit.TestResult, object):
    """
    Accumulates the results of several L{twisted.trial.unittest.TestCase}s.

    @ivar successes: count the number of successes achieved by the test run.
    @type successes: C{int}
    """
    implements(itrial.IReporter)

    def __init__(self):
        super(TestResult, self).__init__()
        self.skips = []
        self.expectedFailures = []
        self.unexpectedSuccesses = []
        self.successes = 0
        self._timings = []

    def __repr__(self):
        return ('<%s run=%d errors=%d failures=%d todos=%d dones=%d skips=%d>'
                % (reflect.qual(self.__class__), self.testsRun,
                   len(self.errors), len(self.failures),
                   len(self.expectedFailures), len(self.skips),
                   len(self.unexpectedSuccesses)))

    def _getTime(self):
        return time.time()

    def _getFailure(self, error):
        """
        Convert a C{sys.exc_info()}-style tuple to a L{Failure}, if necessary.
        """
        if isinstance(error, tuple):
            return Failure(error[1], error[0], error[2])
        return error

    def startTest(self, test):
        """This must be called before the given test is commenced.

        @type test: L{pyunit.TestCase}
        """
        super(TestResult, self).startTest(test)
        self._testStarted = self._getTime()

    def stopTest(self, test):
        """This must be called after the given test is completed.

        @type test: L{pyunit.TestCase}
        """
        super(TestResult, self).stopTest(test)
        self._lastTime = self._getTime() - self._testStarted

    def addFailure(self, test, fail):
        """Report a failed assertion for the given test.

        @type test: L{pyunit.TestCase}
        @type fail: L{Failure} or L{tuple}
        """
        self.failures.append((test, self._getFailure(fail)))

    def addError(self, test, error):
        """Report an error that occurred while running the given test.

        @type test: L{pyunit.TestCase}
        @type fail: L{Failure} or L{tuple}
        """
        self.errors.append((test, self._getFailure(error)))

    def addSkip(self, test, reason):
        """
        Report that the given test was skipped.

        In Trial, tests can be 'skipped'. Tests are skipped mostly because there
        is some platform or configuration issue that prevents them from being
        run correctly.

        @type test: L{pyunit.TestCase}
        @type reason: L{str}
        """
        self.skips.append((test, reason))

    def addUnexpectedSuccess(self, test, todo):
        """Report that the given test succeeded against expectations.

        In Trial, tests can be marked 'todo'. That is, they are expected to fail.
        When a test that is expected to fail instead succeeds, it should call
        this method to report the unexpected success.

        @type test: L{pyunit.TestCase}
        @type todo: L{unittest.Todo}
        """
        # XXX - 'todo' should just be a string
        self.unexpectedSuccesses.append((test, todo))

    def addExpectedFailure(self, test, error, todo):
        """Report that the given test failed, and was expected to do so.

        In Trial, tests can be marked 'todo'. That is, they are expected to fail.

        @type test: L{pyunit.TestCase}
        @type error: L{Failure}
        @type todo: L{unittest.Todo}
        """
        # XXX - 'todo' should just be a string
        self.expectedFailures.append((test, error, todo))

    def addSuccess(self, test):
        """Report that the given test succeeded.

        @type test: L{pyunit.TestCase}
        """
        self.successes += 1

    def upDownError(self, method, error, warn, printStatus):
        warnings.warn("upDownError is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=3)

    def cleanupErrors(self, errs):
        """Report an error that occurred during the cleanup between tests.
        """
        warnings.warn("Cleanup errors are actual errors. Use addError. "
                      "Deprecated in Twisted 8.0",
                      category=DeprecationWarning, stacklevel=2)

    def startSuite(self, name):
        warnings.warn("startSuite deprecated in Twisted 8.0",
                      category=DeprecationWarning, stacklevel=2)

    def endSuite(self, name):
        warnings.warn("endSuite deprecated in Twisted 8.0",
                      category=DeprecationWarning, stacklevel=2)


    def done(self):
        """
        The test suite has finished running.
        """



class TestResultDecorator(proxyForInterface(itrial.IReporter,
                                            "_originalReporter")):
    """
    Base class for TestResult decorators.

    @ivar _originalReporter: The wrapped instance of reporter.
    @type _originalReporter: A provider of L{itrial.IReporter}
    """

    implements(itrial.IReporter)



class UncleanWarningsReporterWrapper(TestResultDecorator):
    """
    A wrapper for a reporter that converts L{util.DirtyReactorError}s
    to warnings.
    """
    implements(itrial.IReporter)

    def addError(self, test, error):
        """
        If the error is a L{util.DirtyReactorError}, instead of
        reporting it as a normal error, throw a warning.
        """

        if (isinstance(error, Failure)
            and error.check(util.DirtyReactorAggregateError)):
            warnings.warn(error.getErrorMessage())
        else:
            self._originalReporter.addError(test, error)



class _AdaptedReporter(TestResultDecorator):
    """
    TestResult decorator that makes sure that addError only gets tests that
    have been adapted with a particular test adapter.
    """

    def __init__(self, original, testAdapter):
        """
        Construct an L{_AdaptedReporter}.

        @param original: An {itrial.IReporter}.
        @param testAdapter: A callable that returns an L{itrial.ITestCase}.
        """
        TestResultDecorator.__init__(self, original)
        self.testAdapter = testAdapter


    def addError(self, test, error):
        """
        See L{itrial.IReporter}.
        """
        test = self.testAdapter(test)
        return self._originalReporter.addError(test, error)


    def addExpectedFailure(self, test, failure, todo):
        """
        See L{itrial.IReporter}.
        """
        return self._originalReporter.addExpectedFailure(
            self.testAdapter(test), failure, todo)


    def addFailure(self, test, failure):
        """
        See L{itrial.IReporter}.
        """
        test = self.testAdapter(test)
        return self._originalReporter.addFailure(test, failure)


    def addSkip(self, test, skip):
        """
        See L{itrial.IReporter}.
        """
        test = self.testAdapter(test)
        return self._originalReporter.addSkip(test, skip)


    def addUnexpectedSuccess(self, test, todo):
        """
        See L{itrial.IReporter}.
        """
        test = self.testAdapter(test)
        return self._originalReporter.addUnexpectedSuccess(test, todo)


    def startTest(self, test):
        """
        See L{itrial.IReporter}.
        """
        return self._originalReporter.startTest(self.testAdapter(test))


    def stopTest(self, test):
        """
        See L{itrial.IReporter}.
        """
        return self._originalReporter.stopTest(self.testAdapter(test))



class Reporter(TestResult):
    """
    A basic L{TestResult} with support for writing to a stream.
    """

    implements(itrial.IReporter)

    _separator = '-' * 79
    _doubleSeparator = '=' * 79

    def __init__(self, stream=sys.stdout, tbformat='default', realtime=False):
        super(Reporter, self).__init__()
        self._stream = SafeStream(stream)
        self.tbformat = tbformat
        self.realtime = realtime
        # The time when the first test was started.
        self._startTime = None


    def stream(self):
        warnings.warn("stream is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=4)
        return self._stream
    stream = property(stream)


    def separator(self):
        warnings.warn("separator is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=4)
        return self._separator
    separator = property(separator)


    def startTest(self, test):
        """
        Called when a test begins to run. Records the time when it was first
        called.

        @param test: L{ITestCase}
        """
        super(Reporter, self).startTest(test)
        if self._startTime is None:
            self._startTime = time.time()


    def addFailure(self, test, fail):
        """
        Called when a test fails. If L{realtime} is set, then it prints the
        error to the stream.

        @param test: L{ITestCase} that failed.
        @param fail: L{failure.Failure} containing the error.
        """
        super(Reporter, self).addFailure(test, fail)
        if self.realtime:
            fail = self.failures[-1][1] # guarantee it's a Failure
            self._write(self._formatFailureTraceback(fail))


    def addError(self, test, error):
        """
        Called when a test raises an error. If L{realtime} is set, then it
        prints the error to the stream.

        @param test: L{ITestCase} that raised the error.
        @param error: L{failure.Failure} containing the error.
        """
        error = self._getFailure(error)
        super(Reporter, self).addError(test, error)
        if self.realtime:
            error = self.errors[-1][1] # guarantee it's a Failure
            self._write(self._formatFailureTraceback(error))


    def write(self, format, *args):
        warnings.warn("write is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=2)
        self._write(format, *args)


    def _write(self, format, *args):
        """
        Safely write to the reporter's stream.

        @param format: A format string to write.
        @param *args: The arguments for the format string.
        """
        s = str(format)
        assert isinstance(s, type(''))
        if args:
            self._stream.write(s % args)
        else:
            self._stream.write(s)
        untilConcludes(self._stream.flush)


    def writeln(self, format, *args):
        warnings.warn("writeln is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=2)
        self._writeln(format, *args)


    def _writeln(self, format, *args):
        """
        Safely write a line to the reporter's stream. Newline is appended to
        the format string.

        @param format: A format string to write.
        @param *args: The arguments for the format string.
        """
        self._write(format, *args)
        self._write('\n')


    def upDownError(self, method, error, warn, printStatus):
        super(Reporter, self).upDownError(method, error, warn, printStatus)
        if warn:
            tbStr = self._formatFailureTraceback(error)
            log.msg(tbStr)
            msg = ("caught exception in %s, your TestCase is broken\n\n%s"
                   % (method, tbStr))
            warnings.warn(msg, BrokenTestCaseWarning, stacklevel=2)


    def cleanupErrors(self, errs):
        super(Reporter, self).cleanupErrors(errs)
        warnings.warn("%s\n%s" % ("REACTOR UNCLEAN! traceback(s) follow: ",
                                  self._formatFailureTraceback(errs)),
                      BrokenTestCaseWarning)


    def _trimFrames(self, frames):
        # when a method fails synchronously, the stack looks like this:
        #  [0]: defer.maybeDeferred()
        #  [1]: utils.runWithWarningsSuppressed()
        #  [2:-2]: code in the test method which failed
        #  [-1]: unittest.fail

        # when a method fails inside a Deferred (i.e., when the test method
        # returns a Deferred, and that Deferred's errback fires), the stack
        # captured inside the resulting Failure looks like this:
        #  [0]: defer.Deferred._runCallbacks
        #  [1:-2]: code in the testmethod which failed
        #  [-1]: unittest.fail

        # as a result, we want to trim either [maybeDeferred,runWWS] or
        # [Deferred._runCallbacks] from the front, and trim the
        # [unittest.fail] from the end.

        # There is also another case, when the test method is badly defined and
        # contains extra arguments.

        newFrames = list(frames)

        if len(frames) < 2:
            return newFrames

        first = newFrames[0]
        second = newFrames[1]
        if (first[0] == "maybeDeferred"
            and os.path.splitext(os.path.basename(first[1]))[0] == 'defer'
            and second[0] == "runWithWarningsSuppressed"
            and os.path.splitext(os.path.basename(second[1]))[0] == 'utils'):
            newFrames = newFrames[2:]
        elif (first[0] == "_runCallbacks"
              and os.path.splitext(os.path.basename(first[1]))[0] == 'defer'):
            newFrames = newFrames[1:]

        if not newFrames:
            # The method fails before getting called, probably an argument problem
            return newFrames

        last = newFrames[-1]
        if (last[0].startswith('fail')
            and os.path.splitext(os.path.basename(last[1]))[0] == 'unittest'):
            newFrames = newFrames[:-1]

        return newFrames


    def _formatFailureTraceback(self, fail):
        if isinstance(fail, str):
            return fail.rstrip() + '\n'
        fail.frames, frames = self._trimFrames(fail.frames), fail.frames
        result = fail.getTraceback(detail=self.tbformat, elideFrameworkCode=True)
        fail.frames = frames
        return result


    def _printResults(self, flavour, errors, formatter):
        """
        Print a group of errors to the stream.

        @param flavour: A string indicating the kind of error (e.g. 'TODO').
        @param errors: A list of errors, often L{failure.Failure}s, but
            sometimes 'todo' errors.
        @param formatter: A callable that knows how to format the errors.
        """
        for content in errors:
            self._writeln(self._doubleSeparator)
            self._writeln('%s: %s' % (flavour, content[0].id()))
            self._writeln('')
            self._write(formatter(*(content[1:])))


    def _printExpectedFailure(self, error, todo):
        return 'Reason: %r\n%s' % (todo.reason,
                                   self._formatFailureTraceback(error))


    def _printUnexpectedSuccess(self, todo):
        ret = 'Reason: %r\n' % (todo.reason,)
        if todo.errors:
            ret += 'Expected errors: %s\n' % (', '.join(todo.errors),)
        return ret


    def printErrors(self):
        """
        Print all of the non-success results in full to the stream.
        """
        warnings.warn("printErrors is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=2)
        self._printErrors()


    def _printErrors(self):
        """
        Print all of the non-success results to the stream in full.
        """
        self._write('\n')
        self._printResults('[SKIPPED]', self.skips, lambda x : '%s\n' % x)
        self._printResults('[TODO]', self.expectedFailures,
                           self._printExpectedFailure)
        self._printResults('[FAIL]', self.failures,
                           self._formatFailureTraceback)
        self._printResults('[ERROR]', self.errors,
                           self._formatFailureTraceback)
        self._printResults('[SUCCESS!?!]', self.unexpectedSuccesses,
                           self._printUnexpectedSuccess)


    def _getSummary(self):
        """
        Return a formatted count of tests status results.
        """
        summaries = []
        for stat in ("skips", "expectedFailures", "failures", "errors",
                     "unexpectedSuccesses"):
            num = len(getattr(self, stat))
            if num:
                summaries.append('%s=%d' % (stat, num))
        if self.successes:
           summaries.append('successes=%d' % (self.successes,))
        summary = (summaries and ' ('+', '.join(summaries)+')') or ''
        return summary


    def printSummary(self):
        """
        Print a line summarising the test results to the stream.
        """
        warnings.warn("printSummary is deprecated in Twisted 8.0.",
                      category=DeprecationWarning, stacklevel=2)
        self._printSummary()


    def _printSummary(self):
        """
        Print a line summarising the test results to the stream.
        """
        summary = self._getSummary()
        if self.wasSuccessful():
            status = "PASSED"
        else:
            status = "FAILED"
        self._write("%s%s\n", status, summary)


    def done(self):
        """
        Summarize the result of the test run.

        The summary includes a report of all of the errors, todos, skips and
        so forth that occurred during the run. It also includes the number of
        tests that were run and how long it took to run them (not including
        load time).

        Expects that L{_printErrors}, L{_writeln}, L{_write}, L{_printSummary}
        and L{_separator} are all implemented.
        """
        self._printErrors()
        self._writeln(self._separator)
        if self._startTime is not None:
            self._writeln('Ran %d tests in %.3fs', self.testsRun,
                          time.time() - self._startTime)
        self._write('\n')
        self._printSummary()



class MinimalReporter(Reporter):
    """
    A minimalist reporter that prints only a summary of the test result, in
    the form of (timeTaken, #tests, #tests, #errors, #failures, #skips).
    """

    def _printErrors(self):
        """
        Don't print a detailed summary of errors. We only care about the
        counts.
        """


    def _printSummary(self):
        """
        Print out a one-line summary of the form:
        '%(runtime) %(number_of_tests) %(number_of_tests) %(num_errors)
        %(num_failures) %(num_skips)'
        """
        numTests = self.testsRun
        t = (self._startTime - self._getTime(), numTests, numTests,
             len(self.errors), len(self.failures), len(self.skips))
        self._writeln(' '.join(map(str, t)))



class TextReporter(Reporter):
    """
    Simple reporter that prints a single character for each test as it runs,
    along with the standard Trial summary text.
    """

    def addSuccess(self, test):
        super(TextReporter, self).addSuccess(test)
        self._write('.')


    def addError(self, *args):
        super(TextReporter, self).addError(*args)
        self._write('E')


    def addFailure(self, *args):
        super(TextReporter, self).addFailure(*args)
        self._write('F')


    def addSkip(self, *args):
        super(TextReporter, self).addSkip(*args)
        self._write('S')


    def addExpectedFailure(self, *args):
        super(TextReporter, self).addExpectedFailure(*args)
        self._write('T')


    def addUnexpectedSuccess(self, *args):
        super(TextReporter, self).addUnexpectedSuccess(*args)
        self._write('!')



class VerboseTextReporter(Reporter):
    """
    A verbose reporter that prints the name of each test as it is running.

    Each line is printed with the name of the test, followed by the result of
    that test.
    """

    # This is actually the bwverbose option

    def startTest(self, tm):
        self._write('%s ... ', tm.id())
        super(VerboseTextReporter, self).startTest(tm)


    def addSuccess(self, test):
        super(VerboseTextReporter, self).addSuccess(test)
        self._write('[OK]')


    def addError(self, *args):
        super(VerboseTextReporter, self).addError(*args)
        self._write('[ERROR]')


    def addFailure(self, *args):
        super(VerboseTextReporter, self).addFailure(*args)
        self._write('[FAILURE]')


    def addSkip(self, *args):
        super(VerboseTextReporter, self).addSkip(*args)
        self._write('[SKIPPED]')


    def addExpectedFailure(self, *args):
        super(VerboseTextReporter, self).addExpectedFailure(*args)
        self._write('[TODO]')


    def addUnexpectedSuccess(self, *args):
        super(VerboseTextReporter, self).addUnexpectedSuccess(*args)
        self._write('[SUCCESS!?!]')


    def stopTest(self, test):
        super(VerboseTextReporter, self).stopTest(test)
        self._write('\n')



class TimingTextReporter(VerboseTextReporter):
    """
    Prints out each test as it is running, followed by the time taken for each
    test to run.
    """

    def stopTest(self, method):
        """
        Mark the test as stopped, and write the time it took to run the test
        to the stream.
        """
        super(TimingTextReporter, self).stopTest(method)
        self._write("(%.03f secs)\n" % self._lastTime)



class _AnsiColorizer(object):
    """
    A colorizer is an object that loosely wraps around a stream, allowing
    callers to write text to the stream in a particular color.

    Colorizer classes must implement C{supported()} and C{write(text, color)}.
    """
    _colors = dict(black=30, red=31, green=32, yellow=33,
                   blue=34, magenta=35, cyan=36, white=37)

    def __init__(self, stream):
        self.stream = stream

    def supported(self):
        """
        A class method that returns True if the current platform supports
        coloring terminal output using this method. Returns False otherwise.
        """
        # assuming stderr
        # isatty() returns False when SSHd into Win32 machine
        if 'CYGWIN' in os.environ:
            return True
        if not sys.stderr.isatty():
            return False # auto color only on TTYs
        try:
            import curses
            curses.setupterm()
            return curses.tigetnum("colors") > 2
        except:
            # guess false in case of error
            return False
    supported = classmethod(supported)

    def write(self, text, color):
        """
        Write the given text to the stream in the given color.

        @param text: Text to be written to the stream.

        @param color: A string label for a color. e.g. 'red', 'white'.
        """
        color = self._colors[color]
        self.stream.write('\x1b[%s;1m%s\x1b[0m' % (color, text))


class _Win32Colorizer(object):
    """
    See _AnsiColorizer docstring.
    """
    def __init__(self, stream):
        from win32console import GetStdHandle, STD_OUTPUT_HANDLE, \
             FOREGROUND_RED, FOREGROUND_BLUE, FOREGROUND_GREEN, \
             FOREGROUND_INTENSITY
        red, green, blue, bold = (FOREGROUND_RED, FOREGROUND_GREEN,
                                  FOREGROUND_BLUE, FOREGROUND_INTENSITY)
        self.stream = stream
        self.screenBuffer = GetStdHandle(STD_OUTPUT_HANDLE)
        self._colors = {
            'normal': red | green | blue,
            'red': red | bold,
            'green': green | bold,
            'blue': blue | bold,
            'yellow': red | green | bold,
            'magenta': red | blue | bold,
            'cyan': green | blue | bold,
            'white': red | green | blue | bold
            }

    def supported(self):
        try:
            import win32console
            screenBuffer = win32console.GetStdHandle(
                win32console.STD_OUTPUT_HANDLE)
        except ImportError:
            return False
        import pywintypes
        try:
            screenBuffer.SetConsoleTextAttribute(
                win32console.FOREGROUND_RED |
                win32console.FOREGROUND_GREEN |
                win32console.FOREGROUND_BLUE)
        except pywintypes.error:
            return False
        else:
            return True
    supported = classmethod(supported)

    def write(self, text, color):
        color = self._colors[color]
        self.screenBuffer.SetConsoleTextAttribute(color)
        self.stream.write(text)
        self.screenBuffer.SetConsoleTextAttribute(self._colors['normal'])


class _NullColorizer(object):
    """
    See _AnsiColorizer docstring.
    """
    def __init__(self, stream):
        self.stream = stream

    def supported(self):
        return True
    supported = classmethod(supported)

    def write(self, text, color):
        self.stream.write(text)



class TreeReporter(Reporter):
    """
    Print out the tests in the form a tree.

    Tests are indented according to which class and module they belong.
    Results are printed in ANSI color.
    """

    currentLine = ''
    indent = '  '
    columns = 79

    FAILURE = 'red'
    ERROR = 'red'
    TODO = 'blue'
    SKIP = 'blue'
    TODONE = 'red'
    SUCCESS = 'green'

    def __init__(self, stream=sys.stdout, tbformat='default', realtime=False):
        super(TreeReporter, self).__init__(stream, tbformat, realtime)
        self._lastTest = []
        for colorizer in [_Win32Colorizer, _AnsiColorizer, _NullColorizer]:
            if colorizer.supported():
                self._colorizer = colorizer(stream)
                break

    def getDescription(self, test):
        """
        Return the name of the method which 'test' represents.  This is
        what gets displayed in the leaves of the tree.

        e.g. getDescription(TestCase('test_foo')) ==> test_foo
        """
        return test.id().split('.')[-1]

    def addSuccess(self, test):
        super(TreeReporter, self).addSuccess(test)
        self.endLine('[OK]', self.SUCCESS)

    def addError(self, *args):
        super(TreeReporter, self).addError(*args)
        self.endLine('[ERROR]', self.ERROR)

    def addFailure(self, *args):
        super(TreeReporter, self).addFailure(*args)
        self.endLine('[FAIL]', self.FAILURE)

    def addSkip(self, *args):
        super(TreeReporter, self).addSkip(*args)
        self.endLine('[SKIPPED]', self.SKIP)

    def addExpectedFailure(self, *args):
        super(TreeReporter, self).addExpectedFailure(*args)
        self.endLine('[TODO]', self.TODO)

    def addUnexpectedSuccess(self, *args):
        super(TreeReporter, self).addUnexpectedSuccess(*args)
        self.endLine('[SUCCESS!?!]', self.TODONE)

    def _write(self, format, *args):
        if args:
            format = format % args
        self.currentLine = format
        super(TreeReporter, self)._write(self.currentLine)


    def _getPreludeSegments(self, testID):
        """
        Return a list of all non-leaf segments to display in the tree.

        Normally this is the module and class name.
        """
        segments = testID.split('.')[:-1]
        if len(segments) == 0:
            return segments
        segments = [
            seg for seg in '.'.join(segments[:-1]), segments[-1]
            if len(seg) > 0]
        return segments


    def _testPrelude(self, testID):
        """
        Write the name of the test to the stream, indenting it appropriately.

        If the test is the first test in a new 'branch' of the tree, also
        write all of the parents in that branch.
        """
        segments = self._getPreludeSegments(testID)
        indentLevel = 0
        for seg in segments:
            if indentLevel < len(self._lastTest):
                if seg != self._lastTest[indentLevel]:
                    self._write('%s%s\n' % (self.indent * indentLevel, seg))
            else:
                self._write('%s%s\n' % (self.indent * indentLevel, seg))
            indentLevel += 1
        self._lastTest = segments


    def cleanupErrors(self, errs):
        self._colorizer.write('    cleanup errors', self.ERROR)
        self.endLine('[ERROR]', self.ERROR)
        super(TreeReporter, self).cleanupErrors(errs)

    def upDownError(self, method, error, warn, printStatus):
        self._colorizer.write("  %s" % method, self.ERROR)
        if printStatus:
            self.endLine('[ERROR]', self.ERROR)
        super(TreeReporter, self).upDownError(method, error, warn, printStatus)

    def startTest(self, test):
        """
        Called when C{test} starts. Writes the tests name to the stream using
        a tree format.
        """
        self._testPrelude(test.id())
        self._write('%s%s ... ' % (self.indent * (len(self._lastTest)),
                                   self.getDescription(test)))
        super(TreeReporter, self).startTest(test)


    def endLine(self, message, color):
        """
        Print 'message' in the given color.

        @param message: A string message, usually '[OK]' or something similar.
        @param color: A string color, 'red', 'green' and so forth.
        """
        spaces = ' ' * (self.columns - len(self.currentLine) - len(message))
        super(TreeReporter, self)._write(spaces)
        self._colorizer.write(message, color)
        super(TreeReporter, self)._write("\n")


    def _printSummary(self):
        """
        Print a line summarising the test results to the stream, and color the
        status result.
        """
        summary = self._getSummary()
        if self.wasSuccessful():
            status = "PASSED"
            color = self.SUCCESS
        else:
            status = "FAILED"
            color = self.FAILURE
        self._colorizer.write(status, color)
        self._write("%s\n", summary)
