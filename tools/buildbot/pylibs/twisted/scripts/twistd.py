# -*- test-case-name: twisted.test.test_twistd -*-
# Copyright (c) 2001-2008 Twisted Matrix Laboratories.
# See LICENSE for details.

"""
The Twisted Daemon: platform-independent interface.

@author: U{Christopher Armstrong<mailto:radix@twistedmatrix.com>}
"""

from twisted.application import app

from twisted.python.runtime import platformType
if platformType == "win32":
    from twisted.scripts._twistw import ServerOptions, \
        WindowsApplicationRunner as _SomeApplicationRunner
else:
    from twisted.scripts._twistd_unix import ServerOptions, \
        UnixApplicationRunner as _SomeApplicationRunner


def runApp(config):
    _SomeApplicationRunner(config).run()


def run():
    app.run(runApp, ServerOptions)


__all__ = ['run', 'runApp']
