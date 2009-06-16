vars = {
  "webkit_trunk":
    "http://svn.webkit.org/repository/webkit/trunk",
  "webkit_revision": "44726",
}


deps = {
  "src/breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@346",

  "src/googleurl":
    "http://google-url.googlecode.com/svn/trunk@106",

  "src/sdch/open-vcdiff":
    "http://open-vcdiff.googlecode.com/svn/trunk@26",

  "src/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@243",

  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@15773",

  "src/third_party/icu38":
    "/trunk/deps/third_party/icu38@16445",

  # TODO(mark): Remove once this has moved into depot_tools.
  "src/tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@517",

  "src/v8":
    "http://v8.googlecode.com/svn/trunk@2181",

  "src/third_party/skia":
    "http://skia.googlecode.com/svn/trunk@198",

  "src/webkit/data/layout_tests/LayoutTests":
    Var("webkit_trunk") + "/LayoutTests@" + Var("webkit_revision"),

  "src/third_party/WebKit/JavaScriptCore":
    Var("webkit_trunk") + "/JavaScriptCore@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebCore":
    Var("webkit_trunk") + "/WebCore@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKitLibraries":
    Var("webkit_trunk") + "/WebKitLibraries@" + Var("webkit_revision"),

  "src/third_party/tcmalloc/tcmalloc":
    "http://google-perftools.googlecode.com/svn/trunk@74",
}


deps_os = {
  "win": {
    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@11984",

    "src/third_party/python_24":
      "/trunk/deps/third_party/python_24@7444",
  },
  "mac": {
    "src/third_party/GTM":
      "http://google-toolbox-for-mac.googlecode.com/svn/trunk@119",
    "src/third_party/pdfsqueeze":
      "http://pdfsqueeze.googlecode.com/svn/trunk@2",
    "src/third_party/WebKit/WebKit/mac":
      Var("webkit_trunk") + "/WebKit/mac@" + Var("webkit_revision"),
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode",
  "+testing",

  # Allow anybody to include files from the "public" Skia directory in the
  # webkit port. This is shared between the webkit port and Chrome.
  "+webkit/port/platform/graphics/skia/public",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "breakpad",
   "gears",
   "o3d",
   "sdch",
   "skia",
   "testing",
   "third_party",
   "v8",
]


hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself shound run the generator.
    "pattern": "\\.gypi?$|[/\\\\]src[/\\\\]tools[/\\\\]gyp[/\\\\]",
    "action": ["python", "src/tools/gyp/gyp_dogfood", "src/build/all.gyp"],
  },
]

