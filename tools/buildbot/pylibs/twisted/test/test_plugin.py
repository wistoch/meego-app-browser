# Copyright (c) 2005 Divmod, Inc.
# Copyright (c) 2007 Twisted Matrix Laboratories.
# See LICENSE for details.

"""
Tests for Twisted plugin system.
"""

import sys, errno, os, time
import compileall

from zope.interface import Interface

from twisted.trial import unittest
from twisted.python.filepath import FilePath
from twisted.python.util import mergeFunctionMetadata

from twisted import plugin



class ITestPlugin(Interface):
    """
    A plugin for use by the plugin system's unit tests.

    Do not use this.
    """



class ITestPlugin2(Interface):
    """
    See L{ITestPlugin}.
    """



class PluginTestCase(unittest.TestCase):
    """
    Tests which verify the behavior of the current, active Twisted plugins
    directory.
    """

    def setUp(self):
        """
        Save C{sys.path} and C{sys.modules}, and create a package for tests.
        """
        self.originalPath = sys.path[:]
        self.savedModules = sys.modules.copy()

        self.root = FilePath(self.mktemp())
        self.root.createDirectory()
        self.package = self.root.child('mypackage')
        self.package.createDirectory()
        self.package.child('__init__.py').setContent("")

        FilePath(__file__).sibling('plugin_basic.py'
            ).copyTo(self.package.child('testplugin.py'))

        self.originalPlugin = "testplugin"

        sys.path.insert(0, self.root.path)
        import mypackage
        self.module = mypackage


    def tearDown(self):
        """
        Restore C{sys.path} and C{sys.modules} to their original values.
        """
        sys.path[:] = self.originalPath
        sys.modules.clear()
        sys.modules.update(self.savedModules)


    def _unimportPythonModule(self, module, deleteSource=False):
        modulePath = module.__name__.split('.')
        packageName = '.'.join(modulePath[:-1])
        moduleName = modulePath[-1]

        delattr(sys.modules[packageName], moduleName)
        del sys.modules[module.__name__]
        for ext in ['c', 'o'] + (deleteSource and [''] or []):
            try:
                os.remove(module.__file__ + ext)
            except OSError, ose:
                if ose.errno != errno.ENOENT:
                    raise


    def _clearCache(self):
        """
        Remove the plugins B{droping.cache} file.
        """
        self.package.child('dropin.cache').remove()


    def _withCacheness(meth):
        """
        This is a paranoid test wrapper, that calls C{meth} 2 times, clear the
        cache, and calls it 2 other times. It's supposed to ensure that the
        plugin system behaves correctly no matter what the state of the cache
        is.
        """
        def wrapped(self):
            meth(self)
            meth(self)
            self._clearCache()
            meth(self)
            meth(self)
        return mergeFunctionMetadata(meth, wrapped)


    def test_cache(self):
        """
        Check that the cache returned by L{plugin.getCache} hold the plugin
        B{testplugin}, and that this plugin has the properties we expect:
        provide L{TestPlugin}, has the good name and description, and can be
        loaded successfully.
        """
        cache = plugin.getCache(self.module)

        dropin = cache[self.originalPlugin]
        self.assertEquals(dropin.moduleName,
                          'mypackage.%s' % (self.originalPlugin,))
        self.assertIn("I'm a test drop-in.", dropin.description)

        # Note, not the preferred way to get a plugin by its interface.
        p1 = [p for p in dropin.plugins if ITestPlugin in p.provided][0]
        self.assertIdentical(p1.dropin, dropin)
        self.assertEquals(p1.name, "TestPlugin")

        # Check the content of the description comes from the plugin module
        # docstring
        self.assertEquals(
            p1.description.strip(),
            "A plugin used solely for testing purposes.")
        self.assertEquals(p1.provided, [ITestPlugin, plugin.IPlugin])
        realPlugin = p1.load()
        # The plugin should match the class present in sys.modules
        self.assertIdentical(
            realPlugin,
            sys.modules['mypackage.%s' % (self.originalPlugin,)].TestPlugin)

        # And it should also match if we import it classicly
        import mypackage.testplugin as tp
        self.assertIdentical(realPlugin, tp.TestPlugin)

    test_cache = _withCacheness(test_cache)


    def test_plugins(self):
        """
        L{plugin.getPlugins} should return the list of plugins matching the
        specified interface (here, L{ITestPlugin2}), and these plugins
        should be instances of classes with a C{test} method, to be sure
        L{plugin.getPlugins} load classes correctly.
        """
        plugins = list(plugin.getPlugins(ITestPlugin2, self.module))

        self.assertEquals(len(plugins), 2)

        names = ['AnotherTestPlugin', 'ThirdTestPlugin']
        for p in plugins:
            names.remove(p.__name__)
            p.test()

    test_plugins = _withCacheness(test_plugins)


    def test_detectNewFiles(self):
        """
        Check that L{plugin.getPlugins} is able to detect plugins added at
        runtime.
        """
        FilePath(__file__).sibling('plugin_extra1.py'
            ).copyTo(self.package.child('pluginextra.py'))
        try:
            # Check that the current situation is clean
            self.failIfIn('mypackage.pluginextra', sys.modules)
            self.failIf(hasattr(sys.modules['mypackage'], 'pluginextra'),
                        "mypackage still has pluginextra module")

            plgs = list(plugin.getPlugins(ITestPlugin, self.module))

            # We should find 2 plugins: the one in testplugin, and the one in
            # pluginextra
            self.assertEquals(len(plgs), 2)

            names = ['TestPlugin', 'FourthTestPlugin']
            for p in plgs:
                names.remove(p.__name__)
                p.test1()
        finally:
            self._unimportPythonModule(
                sys.modules['mypackage.pluginextra'],
                True)

    test_detectNewFiles = _withCacheness(test_detectNewFiles)


    def test_detectFilesChanged(self):
        """
        Check that if the content of a plugin change, L{plugin.getPlugins} is
        able to detect the new plugins added.
        """
        FilePath(__file__).sibling('plugin_extra1.py'
            ).copyTo(self.package.child('pluginextra.py'))
        try:
            plgs = list(plugin.getPlugins(ITestPlugin, self.module))
            # Sanity check
            self.assertEquals(len(plgs), 2)

            FilePath(__file__).sibling('plugin_extra2.py'
                ).copyTo(self.package.child('pluginextra.py'))

            # Fake out Python.
            self._unimportPythonModule(sys.modules['mypackage.pluginextra'])

            # Make sure additions are noticed
            plgs = list(plugin.getPlugins(ITestPlugin, self.module))

            self.assertEquals(len(plgs), 3)

            names = ['TestPlugin', 'FourthTestPlugin', 'FifthTestPlugin']
            for p in plgs:
                names.remove(p.__name__)
                p.test1()
        finally:
            self._unimportPythonModule(
                sys.modules['mypackage.pluginextra'],
                True)

    test_detectFilesChanged = _withCacheness(test_detectFilesChanged)


    def test_detectFilesRemoved(self):
        """
        Check that when a dropin file is removed, L{plugin.getPlugins} doesn't
        return it anymore.
        """
        FilePath(__file__).sibling('plugin_extra1.py'
            ).copyTo(self.package.child('pluginextra.py'))
        try:
            # Generate a cache with pluginextra in it.
            list(plugin.getPlugins(ITestPlugin, self.module))

        finally:
            self._unimportPythonModule(
                sys.modules['mypackage.pluginextra'],
                True)
        plgs = list(plugin.getPlugins(ITestPlugin, self.module))
        self.assertEquals(1, len(plgs))

    test_detectFilesRemoved = _withCacheness(test_detectFilesRemoved)


    def test_nonexistentPathEntry(self):
        """
        Test that getCache skips over any entries in a plugin package's
        C{__path__} which do not exist.
        """
        path = self.mktemp()
        self.failIf(os.path.exists(path))
        # Add the test directory to the plugins path
        self.module.__path__.append(path)
        try:
            plgs = list(plugin.getPlugins(ITestPlugin, self.module))
            self.assertEqual(len(plgs), 1)
        finally:
            self.module.__path__.remove(path)

    test_nonexistentPathEntry = _withCacheness(test_nonexistentPathEntry)


    def test_nonDirectoryChildEntry(self):
        """
        Test that getCache skips over any entries in a plugin package's
        C{__path__} which refer to children of paths which are not directories.
        """
        path = FilePath(self.mktemp())
        self.failIf(path.exists())
        path.touch()
        child = path.child("test_package").path
        self.module.__path__.append(child)
        try:
            plgs = list(plugin.getPlugins(ITestPlugin, self.module))
            self.assertEqual(len(plgs), 1)
        finally:
            self.module.__path__.remove(child)

    test_nonDirectoryChildEntry = _withCacheness(test_nonDirectoryChildEntry)


    def test_deployedMode(self):
        """
        The C{dropin.cache} file may not be writable: the cache should still be
        attainable, but an error should be logged to show that the cache
        couldn't be updated.
        """
        # Generate the cache
        plugin.getCache(self.module)

        # Add a new plugin
        FilePath(__file__).sibling('plugin_extra1.py'
            ).copyTo(self.package.child('pluginextra.py'))

        os.chmod(self.package.path, 0500)
        # Change the right of dropin.cache too for windows
        os.chmod(self.package.child('dropin.cache').path, 0400)
        self.addCleanup(os.chmod, self.package.path, 0700)
        self.addCleanup(os.chmod,
            self.package.child('dropin.cache').path, 0700)

        cache = plugin.getCache(self.module)
        # The new plugin should be reported
        self.assertIn('pluginextra', cache)
        self.assertIn(self.originalPlugin, cache)

        errors = self.flushLoggedErrors()
        self.assertEquals(len(errors), 1)
        # Windows report OSError, others IOError
        errors[0].trap(OSError, IOError)



# This is something like the Twisted plugins file.
pluginInitFile = """
from twisted.plugin import pluginPackagePaths
__path__.extend(pluginPackagePaths(__name__))
__all__ = []
"""

def pluginFileContents(name):
    return (
        "from zope.interface import classProvides\n"
        "from twisted.plugin import IPlugin\n"
        "from twisted.test.test_plugin import ITestPlugin\n"
        "\n"
        "class %s(object):\n"
        "    classProvides(IPlugin, ITestPlugin)\n") % (name,)


def _createPluginDummy(entrypath, pluginContent, real, pluginModule):
    """
    Create a plugindummy package.
    """
    entrypath.createDirectory()
    pkg = entrypath.child('plugindummy')
    pkg.createDirectory()
    if real:
        pkg.child('__init__.py').setContent('')
    plugs = pkg.child('plugins')
    plugs.createDirectory()
    if real:
        plugs.child('__init__.py').setContent(pluginInitFile)
    plugs.child(pluginModule + '.py').setContent(pluginContent)
    return plugs



class DeveloperSetupTests(unittest.TestCase):
    """
    These tests verify things about the plugin system without actually
    interacting with the deployed 'twisted.plugins' package, instead creating a
    temporary package.
    """

    def setUp(self):
        """
        Create a complex environment with multiple entries on sys.path, akin to
        a developer's environment who has a development (trunk) checkout of
        Twisted, a system installed version of Twisted (for their operating
        system's tools) and a project which provides Twisted plugins.
        """
        self.savedPath = sys.path[:]
        self.savedModules = sys.modules.copy()
        self.fakeRoot = FilePath(self.mktemp())
        self.fakeRoot.createDirectory()
        self.systemPath = self.fakeRoot.child('system_path')
        self.devPath = self.fakeRoot.child('development_path')
        self.appPath = self.fakeRoot.child('application_path')
        self.systemPackage = _createPluginDummy(
            self.systemPath, pluginFileContents('system'),
            True, 'plugindummy_builtin')
        self.devPackage = _createPluginDummy(
            self.devPath, pluginFileContents('dev'),
            True, 'plugindummy_builtin')
        self.appPackage = _createPluginDummy(
            self.appPath, pluginFileContents('app'),
            False, 'plugindummy_app')

        # Now we're going to do the system installation.
        sys.path.extend([x.path for x in [self.systemPath,
                                          self.appPath]])
        # Run all the way through the plugins list to cause the
        # L{plugin.getPlugins} generator to write cache files for the system
        # installation.
        self.getAllPlugins()
        self.sysplug = self.systemPath.child('plugindummy').child('plugins')
        self.syscache = self.sysplug.child('dropin.cache')
        # Make sure there's a nice big difference in modification times so that
        # we won't re-build the system cache.
        now = time.time()
        os.utime(
            self.sysplug.child('plugindummy_builtin.py').path,
            (now - 5000,) * 2)
        os.utime(self.syscache.path, (now - 2000,) * 2)
        # For extra realism, let's make sure that the system path is no longer
        # writable.
        self.lockSystem()
        self.resetEnvironment()


    def lockSystem(self):
        """
        Lock the system directories, as if they were unwritable by this user.
        """
        os.chmod(self.sysplug.path, 0555)
        os.chmod(self.syscache.path, 0555)


    def unlockSystem(self):
        """
        Unlock the system directories, as if they were writable by this user.
        """
        os.chmod(self.sysplug.path, 0777)
        os.chmod(self.syscache.path, 0777)


    def getAllPlugins(self):
        """
        Get all the plugins loadable from our dummy package, and return their
        short names.
        """
        # Import the module we just added to our path.  (Local scope because
        # this package doesn't exist outside of this test.)
        import plugindummy.plugins
        x = list(plugin.getPlugins(ITestPlugin, plugindummy.plugins))
        return [plug.__name__ for plug in x]


    def resetEnvironment(self):
        """
        Change the environment to what it should be just as the test is
        starting.
        """
        self.unsetEnvironment()
        sys.path.extend([x.path for x in [self.devPath,
                                          self.systemPath,
                                          self.appPath]])

    def unsetEnvironment(self):
        """
        Change the Python environment back to what it was before the test was
        started.
        """
        sys.modules.clear()
        sys.modules.update(self.savedModules)
        sys.path[:] = self.savedPath


    def tearDown(self):
        """
        Reset the Python environment to what it was before this test ran, and
        restore permissions on files which were marked read-only so that the
        directory may be cleanly cleaned up.
        """
        self.unsetEnvironment()
        # Normally we wouldn't "clean up" the filesystem like this (leaving
        # things for post-test inspection), but if we left the permissions the
        # way they were, we'd be leaving files around that the buildbots
        # couldn't delete, and that would be bad.
        self.unlockSystem()


    def test_developmentPluginAvailability(self):
        """
        Plugins added in the development path should be loadable, even when
        the (now non-importable) system path contains its own idea of the
        list of plugins for a package.  Inversely, plugins added in the
        system path should not be available.
        """
        # Run 3 times: uncached, cached, and then cached again to make sure we
        # didn't overwrite / corrupt the cache on the cached try.
        for x in range(3):
            names = self.getAllPlugins()
            names.sort()
            self.assertEqual(names, ['app', 'dev'])


    def test_freshPyReplacesStalePyc(self):
        """
        Verify that if a stale .pyc file on the PYTHONPATH is replaced by a
        fresh .py file, the plugins in the new .py are picked up rather than
        the stale .pyc, even if the .pyc is still around.
        """
        mypath = self.appPackage.child("stale.py")
        mypath.setContent(pluginFileContents('one'))
        # Make it super stale
        x = time.time() - 1000
        os.utime(mypath.path, (x, x))
        pyc = mypath.sibling('stale.pyc')
        # compile it
        compileall.compile_dir(self.appPackage.path, quiet=1)
        os.utime(pyc.path, (x, x))
        # Eliminate the other option.
        mypath.remove()
        # Make sure it's the .pyc path getting cached.
        self.resetEnvironment()
        # Sanity check.
        self.assertIn('one', self.getAllPlugins())
        self.failIfIn('two', self.getAllPlugins())
        self.resetEnvironment()
        mypath.setContent(pluginFileContents('two'))
        self.failIfIn('one', self.getAllPlugins())
        self.assertIn('two', self.getAllPlugins())


    def test_newPluginsOnReadOnlyPath(self):
        """
        Verify that a failure to write the dropin.cache file on a read-only
        path will not affect the list of plugins returned.

        Note: this test should pass on both Linux and Windows, but may not
        provide useful coverage on Windows due to the different meaning of
        "read-only directory".
        """
        self.unlockSystem()
        self.sysplug.child('newstuff.py').setContent(pluginFileContents('one'))
        self.lockSystem()

        # Take the developer path out, so that the system plugins are actually
        # examined.
        sys.path.remove(self.devPath.path)

        # Sanity check to make sure we're only flushing the error logged
        # below...
        self.assertEqual(len(self.flushLoggedErrors()), 0)
        self.assertIn('one', self.getAllPlugins())
        self.assertEqual(len(self.flushLoggedErrors()), 1)



class AdjacentPackageTests(unittest.TestCase):
    """
    Tests for the behavior of the plugin system when there are multiple
    installed copies of the package containing the plugins being loaded.
    """

    def setUp(self):
        """
        Save the elements of C{sys.path} and the items of C{sys.modules}.
        """
        self.originalPath = sys.path[:]
        self.savedModules = sys.modules.copy()


    def tearDown(self):
        """
        Restore C{sys.path} and C{sys.modules} to their original values.
        """
        sys.path[:] = self.originalPath
        sys.modules.clear()
        sys.modules.update(self.savedModules)


    def createDummyPackage(self, root, name, pluginName):
        """
        Create a directory containing a Python package named I{dummy} with a
        I{plugins} subpackage.

        @type root: L{FilePath}
        @param root: The directory in which to create the hierarchy.

        @type name: C{str}
        @param name: The name of the directory to create which will contain
            the package.

        @type pluginName: C{str}
        @param pluginName: The name of a module to create in the
            I{dummy.plugins} package.

        @rtype: L{FilePath}
        @return: The directory which was created to contain the I{dummy}
            package.
        """
        directory = root.child(name)
        package = directory.child('dummy')
        package.makedirs()
        package.child('__init__.py').setContent('')
        plugins = package.child('plugins')
        plugins.makedirs()
        plugins.child('__init__.py').setContent(pluginInitFile)
        pluginModule = plugins.child(pluginName + '.py')
        pluginModule.setContent(pluginFileContents(name))
        return directory


    def test_hiddenPackageSamePluginModuleNameObscured(self):
        """
        Only plugins from the first package in sys.path should be returned by
        getPlugins in the case where there are two Python packages by the same
        name installed, each with a plugin module by a single name.
        """
        root = FilePath(self.mktemp())
        root.makedirs()

        firstDirectory = self.createDummyPackage(root, 'first', 'someplugin')
        secondDirectory = self.createDummyPackage(root, 'second', 'someplugin')

        sys.path.append(firstDirectory.path)
        sys.path.append(secondDirectory.path)

        import dummy.plugins

        plugins = list(plugin.getPlugins(ITestPlugin, dummy.plugins))
        self.assertEqual(['first'], [p.__name__ for p in plugins])


    def test_hiddenPackageDifferentPluginModuleNameObscured(self):
        """
        Plugins from the first package in sys.path should be returned by
        getPlugins in the case where there are two Python packages by the same
        name installed, each with a plugin module by a different name.
        """
        root = FilePath(self.mktemp())
        root.makedirs()

        firstDirectory = self.createDummyPackage(root, 'first', 'thisplugin')
        secondDirectory = self.createDummyPackage(root, 'second', 'thatplugin')

        sys.path.append(firstDirectory.path)
        sys.path.append(secondDirectory.path)

        import dummy.plugins

        plugins = list(plugin.getPlugins(ITestPlugin, dummy.plugins))
        self.assertEqual(['first'], [p.__name__ for p in plugins])



class PackagePathTests(unittest.TestCase):
    """
    Tests for L{plugin.pluginPackagePaths} which constructs search paths for
    plugin packages.
    """

    def setUp(self):
        """
        Save the elements of C{sys.path}.
        """
        self.originalPath = sys.path[:]


    def tearDown(self):
        """
        Restore C{sys.path} to its original value.
        """
        sys.path[:] = self.originalPath


    def test_pluginDirectories(self):
        """
        L{plugin.pluginPackagePaths} should return a list containing each
        directory in C{sys.path} with a suffix based on the supplied package
        name.
        """
        foo = FilePath('foo')
        bar = FilePath('bar')
        sys.path = [foo.path, bar.path]
        self.assertEqual(
            plugin.pluginPackagePaths('dummy.plugins'),
            [foo.child('dummy').child('plugins').path,
             bar.child('dummy').child('plugins').path])


    def test_pluginPackagesExcluded(self):
        """
        L{plugin.pluginPackagePaths} should exclude directories which are
        Python packages.  The only allowed plugin package (the only one
        associated with a I{dummy} package which Python will allow to be
        imported) will already be known to the caller of
        L{plugin.pluginPackagePaths} and will most commonly already be in
        the C{__path__} they are about to mutate.
        """
        root = FilePath(self.mktemp())
        foo = root.child('foo').child('dummy').child('plugins')
        foo.makedirs()
        foo.child('__init__.py').setContent('')
        sys.path = [root.child('foo').path, root.child('bar').path]
        self.assertEqual(
            plugin.pluginPackagePaths('dummy.plugins'),
            [root.child('bar').child('dummy').child('plugins').path])
