vars = {
  "webkit_trunk":
    "http://svn.webkit.org/repository/webkit/trunk",
  "webkit_revision": "40875",
}


deps = {
  "src/breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@285",

  "src/googleurl":
    "http://google-url.googlecode.com/svn/trunk@95",

  "src/sdch/open-vcdiff":
    "http://open-vcdiff.googlecode.com/svn/trunk@22",

  "src/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@167",

  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@9792",

  "src/third_party/icu38":
    "/trunk/deps/third_party/icu38@9252",

  "src/v8":
    "http://v8.googlecode.com/svn/trunk@1266",

  "src/webkit/data/layout_tests/LayoutTests":
    Var("webkit_trunk") + "/LayoutTests@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKit":
    Var("webkit_trunk") + "/WebKit@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKitLibraries":
    Var("webkit_trunk") + "/WebKitLibraries@" + Var("webkit_revision"),
}


deps_os = {
  "win": {
    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@3248",

    "src/third_party/python_24":
      "/trunk/deps/third_party/python_24@7444",

    "src/third_party/svn":
      "/trunk/deps/third_party/svn@3230",
  },
  "mac": {
    "src/third_party/GTM":
      "http://google-toolbox-for-mac.googlecode.com/svn/trunk/@77",
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
   "sdch",
   "skia",
   "testing",
   "third_party",
   "v8",
]
