deps = {
  "src/breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@285",

  "src/googleurl":
    "http://google-url.googlecode.com/svn/trunk@94",

  "src/sdch/open-vcdiff":
    "http://open-vcdiff.googlecode.com/svn/trunk@22",

  "src/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@63",

  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@3882",

  "src/third_party/cygwin":
    "/trunk/deps/third_party/cygwin@3248",

  "src/third_party/icu38":
    "/trunk/deps/third_party/icu38@3658",

  "src/third_party/python_24":
    "/trunk/deps/third_party/python_24@3247",

  "src/third_party/svn":
    "/trunk/deps/third_party/svn@3230",

  "src/v8":
    "http://v8.googlecode.com/svn/trunk@572",

  "src/webkit/data/layout_tests/LayoutTests":
    "http://svn.webkit.org/repository/webkit/trunk/LayoutTests@36102",
  
  "src/gears/gears":
    "http://gears.googlecode.com/svn/trunk/gears@2919",

  "src/gears/third_party":
    "http://gears.googlecode.com/svn/trunk/third_party@2919",
}

include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode"
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
