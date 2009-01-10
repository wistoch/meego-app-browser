# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool module for adding, to a construction environment, Chromium-specific
wrappers around Hammer builders.  This gives us a central place for any
customization we need to make to the different things we build.
"""

import SCons.Node
import _Node_MSVS as MSVS

class Null(object):
  def __new__(cls, *args, **kwargs):
    if '_inst' not in vars(cls):
      cls._inst = super(type, cls).__new__(cls, *args, **kwargs)
    return cls._inst
  def __init__(self, *args, **kwargs): pass
  def __call__(self, *args, **kwargs): return self
  def __repr__(self): return "Null()"
  def __nonzero__(self): return False
  def __getattr__(self, name): return self
  def __setattr__(self, name, val): return self
  def __delattr__(self, name): return self
  def __getitem__(self, name): return self

class ChromeFileList(MSVS.FileList):
  def Append(self, *args):
    for element in args:
      self.append(element)
  def Extend(self, *args):
    for element in args:
      self.extend(element)
  def Remove(self, *args):
    for top, lists, nonlists in MSVS.FileListWalk(self, topdown=False):
      for element in args:
        try:
          top.remove(element)
        except ValueError:
          pass
  def Replace(self, old, new):
    for top, lists, nonlists in MSVS.FileListWalk(self, topdown=False):
      try:
        i = top.index(old)
      except ValueError:
        pass
      else:
        top[i] = new

import __builtin__
__builtin__.ChromeFileList = ChromeFileList

def compilable_files(sources):
  if not hasattr(sources, 'entries'):
    return [x for x in sources if not str(x).endswith('.h')
                               and not str(x).endswith('.dat')]
  result = []
  for top, folders, nonfolders in MSVS.FileListWalk(sources):
    result.extend([x for x in nonfolders if not str(x).endswith('.h')
                                         and not str(x).endswith('.dat')])
  return result

def ChromeProgram(env, target, source, *args, **kw):
  source = compilable_files(source)
  result = env.ComponentProgram(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def ChromeTestProgram(env, target, source, *args, **kw):
  source = compilable_files(source)
  result = env.ComponentTestProgram(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(*result)
  return result

def ChromeLibrary(env, target, source, *args, **kw):
  source = compilable_files(source)
  return env.ComponentLibrary(target, source, *args, **kw)

def ChromeStaticLibrary(env, target, source, *args, **kw):
  source = compilable_files(source)
  kw['COMPONENT_STATIC'] = True
  return env.ComponentLibrary(target, source, *args, **kw)

def ChromeSharedLibrary(env, target, source, *args, **kw):
  source = compilable_files(source)
  kw['COMPONENT_STATIC'] = False
  result = [env.ComponentLibrary(target, source, *args, **kw)[0]]
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def ChromeObject(env, *args, **kw):
  return env.ComponentObject(*args, **kw)

def ChromeMSVSFolder(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  return env.MSVSFolder(*args, **kw)

def ChromeMSVSProject(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  return env.MSVSProject(*args, **kw)

def ChromeMSVSSolution(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  return env.MSVSSolution(*args, **kw)

def generate(env):
  env.AddMethod(ChromeProgram)
  env.AddMethod(ChromeTestProgram)
  env.AddMethod(ChromeLibrary)
  env.AddMethod(ChromeStaticLibrary)
  env.AddMethod(ChromeSharedLibrary)
  env.AddMethod(ChromeObject)
  env.AddMethod(ChromeMSVSFolder)
  env.AddMethod(ChromeMSVSProject)
  env.AddMethod(ChromeMSVSSolution)

def exists(env):
  return True
