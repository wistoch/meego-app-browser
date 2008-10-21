# -*- test-case-name: twisted.conch.test.test_helper -*-
# Copyright (c) 2001-2004 Twisted Matrix Laboratories.
# See LICENSE for details.

"""
Partial in-memory terminal emulator

@author: U{Jp Calderone<mailto:exarkun@twistedmatrix.com>}
"""

import re, string

from zope.interface import implements

from twisted.internet import defer, protocol, reactor
from twisted.python import log

from twisted.conch.insults import insults

FOREGROUND = 30
BACKGROUND = 40
BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE, N_COLORS = range(9)

class CharacterAttribute:
    """Represents the attributes of a single character.

    Character set, intensity, underlinedness, blinkitude, video
    reversal, as well as foreground and background colors made up a
    character's attributes.
    """
    def __init__(self, charset=insults.G0,
                 bold=False, underline=False,
                 blink=False, reverseVideo=False,
                 foreground=WHITE, background=BLACK,

                 _subtracting=False):
        self.charset = charset
        self.bold = bold
        self.underline = underline
        self.blink = blink
        self.reverseVideo = reverseVideo
        self.foreground = foreground
        self.background = background

        self._subtracting = _subtracting

    def __eq__(self, other):
        return vars(self) == vars(other)

    def __ne__(self, other):
        return not self.__eq__(other)

    def copy(self):
        c = self.__class__()
        c.__dict__.update(vars(self))
        return c

    def wantOne(self, **kw):
        k, v = kw.popitem()
        if getattr(self, k) != v:
            attr = self.copy()
            attr._subtracting = not v
            setattr(attr, k, v)
            return attr
        else:
            return self.copy()

    def toVT102(self):
        # Spit out a vt102 control sequence that will set up
        # all the attributes set here.  Except charset.
        attrs = []
        if self._subtracting:
            attrs.append(0)
        if self.bold:
            attrs.append(insults.BOLD)
        if self.underline:
            attrs.append(insults.UNDERLINE)
        if self.blink:
            attrs.append(insults.BLINK)
        if self.reverseVideo:
            attrs.append(insults.REVERSE_VIDEO)
        if self.foreground != WHITE:
            attrs.append(FOREGROUND + self.foreground)
        if self.background != BLACK:
            attrs.append(BACKGROUND + self.background)
        if attrs:
            return '\x1b[' + ';'.join(map(str, attrs)) + 'm'
        return ''

# XXX - need to support scroll regions and scroll history
class TerminalBuffer(protocol.Protocol):
    """
    An in-memory terminal emulator.
    """
    implements(insults.ITerminalTransport)

    for keyID in ('UP_ARROW', 'DOWN_ARROW', 'RIGHT_ARROW', 'LEFT_ARROW',
                  'HOME', 'INSERT', 'DELETE', 'END', 'PGUP', 'PGDN',
                  'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9',
                  'F10', 'F11', 'F12'):
        exec '%s = object()' % (keyID,)

    TAB = '\t'
    BACKSPACE = '\x7f'

    width = 80
    height = 24

    fill = ' '
    void = object()

    def getCharacter(self, x, y):
        return self.lines[y][x]

    def connectionMade(self):
        self.reset()

    def write(self, bytes):
        """
        Add the given printable bytes to the terminal.

        Line feeds in C{bytes} will be replaced with carriage return / line
        feed pairs.
        """
        for b in bytes.replace('\n', '\r\n'):
            self.insertAtCursor(b)

    def _currentCharacterAttributes(self):
        return CharacterAttribute(self.activeCharset, **self.graphicRendition)

    def insertAtCursor(self, b):
        """
        Add one byte to the terminal at the cursor and make consequent state
        updates.

        If b is a carriage return, move the cursor to the beginning of the
        current row.

        If b is a line feed, move the cursor to the next row or scroll down if
        the cursor is already in the last row.

        Otherwise, if b is printable, put it at the cursor position (inserting
        or overwriting as dictated by the current mode) and move the cursor.
        """
        if b == '\r':
            self.x = 0
        elif b == '\n':
            self._scrollDown()
        elif b in string.printable:
            if self.x >= self.width:
                self.nextLine()
            ch = (b, self._currentCharacterAttributes())
            if self.modes.get(insults.modes.IRM):
                self.lines[self.y][self.x:self.x] = [ch]
                self.lines[self.y].pop()
            else:
                self.lines[self.y][self.x] = ch
            self.x += 1

    def _emptyLine(self, width):
        return [(self.void, self._currentCharacterAttributes()) for i in xrange(width)]

    def _scrollDown(self):
        self.y += 1
        if self.y >= self.height:
            self.y -= 1
            del self.lines[0]
            self.lines.append(self._emptyLine(self.width))

    def _scrollUp(self):
        self.y -= 1
        if self.y < 0:
            self.y = 0
            del self.lines[-1]
            self.lines.insert(0, self._emptyLine(self.width))

    def cursorUp(self, n=1):
        self.y = max(0, self.y - n)

    def cursorDown(self, n=1):
        self.y = min(self.height - 1, self.y + n)

    def cursorBackward(self, n=1):
        self.x = max(0, self.x - n)

    def cursorForward(self, n=1):
        self.x = min(self.width, self.x + n)

    def cursorPosition(self, column, line):
        self.x = column
        self.y = line

    def cursorHome(self):
        self.x = self.home.x
        self.y = self.home.y

    def index(self):
        self._scrollDown()

    def reverseIndex(self):
        self._scrollUp()

    def nextLine(self):
        """
        Update the cursor position attributes and scroll down if appropriate.
        """
        self.x = 0
        self._scrollDown()

    def saveCursor(self):
        self._savedCursor = (self.x, self.y)

    def restoreCursor(self):
        self.x, self.y = self._savedCursor
        del self._savedCursor

    def setModes(self, modes):
        for m in modes:
            self.modes[m] = True

    def resetModes(self, modes):
        for m in modes:
            try:
                del self.modes[m]
            except KeyError:
                pass


    def setPrivateModes(self, modes):
        """
        Enable the given modes.

        Track which modes have been enabled so that the implementations of
        other L{insults.ITerminalTransport} methods can be properly implemented
        to respect these settings.

        @see: L{resetPrivateModes}
        @see: L{insults.ITerminalTransport.setPrivateModes}
        """
        for m in modes:
            self.privateModes[m] = True


    def resetPrivateModes(self, modes):
        """
        Disable the given modes.

        @see: L{setPrivateModes}
        @see: L{insults.ITerminalTransport.resetPrivateModes}
        """
        for m in modes:
            try:
                del self.privateModes[m]
            except KeyError:
                pass


    def applicationKeypadMode(self):
        self.keypadMode = 'app'

    def numericKeypadMode(self):
        self.keypadMode = 'num'

    def selectCharacterSet(self, charSet, which):
        self.charsets[which] = charSet

    def shiftIn(self):
        self.activeCharset = insults.G0

    def shiftOut(self):
        self.activeCharset = insults.G1

    def singleShift2(self):
        oldActiveCharset = self.activeCharset
        self.activeCharset = insults.G2
        f = self.insertAtCursor
        def insertAtCursor(b):
            f(b)
            del self.insertAtCursor
            self.activeCharset = oldActiveCharset
        self.insertAtCursor = insertAtCursor

    def singleShift3(self):
        oldActiveCharset = self.activeCharset
        self.activeCharset = insults.G3
        f = self.insertAtCursor
        def insertAtCursor(b):
            f(b)
            del self.insertAtCursor
            self.activeCharset = oldActiveCharset
        self.insertAtCursor = insertAtCursor

    def selectGraphicRendition(self, *attributes):
        for a in attributes:
            if a == insults.NORMAL:
                self.graphicRendition = {
                    'bold': False,
                    'underline': False,
                    'blink': False,
                    'reverseVideo': False,
                    'foreground': WHITE,
                    'background': BLACK}
            elif a == insults.BOLD:
                self.graphicRendition['bold'] = True
            elif a == insults.UNDERLINE:
                self.graphicRendition['underline'] = True
            elif a == insults.BLINK:
                self.graphicRendition['blink'] = True
            elif a == insults.REVERSE_VIDEO:
                self.graphicRendition['reverseVideo'] = True
            else:
                try:
                    v = int(a)
                except ValueError:
                    log.msg("Unknown graphic rendition attribute: " + repr(a))
                else:
                    if FOREGROUND <= v <= FOREGROUND + N_COLORS:
                        self.graphicRendition['foreground'] = v - FOREGROUND
                    elif BACKGROUND <= v <= BACKGROUND + N_COLORS:
                        self.graphicRendition['background'] = v - BACKGROUND
                    else:
                        log.msg("Unknown graphic rendition attribute: " + repr(a))

    def eraseLine(self):
        self.lines[self.y] = self._emptyLine(self.width)

    def eraseToLineEnd(self):
        width = self.width - self.x
        self.lines[self.y][self.x:] = self._emptyLine(width)

    def eraseToLineBeginning(self):
        self.lines[self.y][:self.x + 1] = self._emptyLine(self.x + 1)

    def eraseDisplay(self):
        self.lines = [self._emptyLine(self.width) for i in xrange(self.height)]

    def eraseToDisplayEnd(self):
        self.eraseToLineEnd()
        height = self.height - self.y - 1
        self.lines[self.y + 1:] = [self._emptyLine(self.width) for i in range(height)]

    def eraseToDisplayBeginning(self):
        self.eraseToLineBeginning()
        self.lines[:self.y] = [self._emptyLine(self.width) for i in range(self.y)]

    def deleteCharacter(self, n=1):
        del self.lines[self.y][self.x:self.x+n]
        self.lines[self.y].extend(self._emptyLine(min(self.width - self.x, n)))

    def insertLine(self, n=1):
        self.lines[self.y:self.y] = [self._emptyLine(self.width) for i in range(n)]
        del self.lines[self.height:]

    def deleteLine(self, n=1):
        del self.lines[self.y:self.y+n]
        self.lines.extend([self._emptyLine(self.width) for i in range(n)])

    def reportCursorPosition(self):
        return (self.x, self.y)

    def reset(self):
        self.home = insults.Vector(0, 0)
        self.x = self.y = 0
        self.modes = {}
        self.privateModes = {}
        self.setPrivateModes([insults.privateModes.AUTO_WRAP,
                              insults.privateModes.CURSOR_MODE])
        self.numericKeypad = 'app'
        self.activeCharset = insults.G0
        self.graphicRendition = {
            'bold': False,
            'underline': False,
            'blink': False,
            'reverseVideo': False,
            'foreground': WHITE,
            'background': BLACK}
        self.charsets = {
            insults.G0: insults.CS_US,
            insults.G1: insults.CS_US,
            insults.G2: insults.CS_ALTERNATE,
            insults.G3: insults.CS_ALTERNATE_SPECIAL}
        self.eraseDisplay()

    def unhandledControlSequence(self, buf):
        print 'Could not handle', repr(buf)

    def __str__(self):
        lines = []
        for L in self.lines:
            buf = []
            length = 0
            for (ch, attr) in L:
                if ch is not self.void:
                    buf.append(ch)
                    length = len(buf)
                else:
                    buf.append(self.fill)
            lines.append(''.join(buf[:length]))
        return '\n'.join(lines)

class ExpectationTimeout(Exception):
    pass

class ExpectableBuffer(TerminalBuffer):
    _mark = 0

    def connectionMade(self):
        TerminalBuffer.connectionMade(self)
        self._expecting = []

    def write(self, bytes):
        TerminalBuffer.write(self, bytes)
        self._checkExpected()

    def cursorHome(self):
        TerminalBuffer.cursorHome(self)
        self._mark = 0

    def _timeoutExpected(self, d):
        d.errback(ExpectationTimeout())
        self._checkExpected()

    def _checkExpected(self):
        s = str(self)[self._mark:]
        while self._expecting:
            expr, timer, deferred = self._expecting[0]
            if timer and not timer.active():
                del self._expecting[0]
                continue
            for match in expr.finditer(s):
                if timer:
                    timer.cancel()
                del self._expecting[0]
                self._mark += match.end()
                s = s[match.end():]
                deferred.callback(match)
                break
            else:
                return

    def expect(self, expression, timeout=None, scheduler=reactor):
        d = defer.Deferred()
        timer = None
        if timeout:
            timer = scheduler.callLater(timeout, self._timeoutExpected, d)
        self._expecting.append((re.compile(expression), timer, d))
        self._checkExpected()
        return d

__all__ = ['CharacterAttribute', 'TerminalBuffer', 'ExpectableBuffer']
