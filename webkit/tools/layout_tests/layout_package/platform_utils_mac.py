#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Platform-specific utility methods shared by several scripts."""

import os
import re
import subprocess
import sys

import google.path_utils

# Distinguish the path_utils.py in this dir from google.path_utils.
import path_utils as layout_package_path_utils

# This will be a native path to the directory this file resides in.
# It can either be relative or absolute depending how it's executed.
THISDIR = os.path.dirname(os.path.abspath(__file__))
def PathFromBase(*pathies):
  return google.path_utils.FindUpward(THISDIR, *pathies)

class PlatformUtility(object):
  def __init__(self, base_dir):
    """Args:
         base_dir: a directory above which third_party/cygwin can be found,
             used to locate the cygpath executable for path conversions.
    """
    self._cygwin_root = None
    self._base_dir = base_dir

  LAYOUTTEST_HTTP_DIR = "LayoutTests/http/tests/"
  PENDING_HTTP_DIR    = "pending/http/tests/"

  def GetAbsolutePath(self, path, force=False):
    """Returns an absolute UNIX path."""
    return os.path.abspath(path)

  # TODO(pinkerton): would be great to get rid of the duplication with
  # platform_utils_win.py for the next two functions, but the inheritance
  # from tools/python/google on the windows side makes that a bit difficult.

  def _FilenameToUri(self, path, use_http=False, use_ssl=False, port=8000):
    """Convert a Windows style path to a URI.

    Args:
      path: For an http URI, the path relative to the httpd server's
          DocumentRoot; for a file URI, the full path to the file.
      use_http: if True, returns a URI of the form http://127.0.0.1:8000/.
          If False, returns a file:/// URI.
      use_ssl: if True, returns HTTPS URL (https://127.0.0.1:8000/).
          This parameter is ignored if use_http=False.
      port: The port number to append when returning an HTTP URI
    """
    if use_http:
      protocol = 'http'
      if use_ssl:
        protocol = 'https'
      path = path.replace("\\", "/")
      return "%s://127.0.0.1:%s/%s" % (protocol, str(port), path)
    return "file:///" + self.GetAbsolutePath(path)

  def FilenameToUri(self, full_path):
    relative_path = layout_package_path_utils.RelativeTestFilename(full_path)
    port = None
    use_ssl = False

    # LayoutTests/http/tests/ run off port 8000 and ssl/ off 8443
    if relative_path.startswith(self.LAYOUTTEST_HTTP_DIR):
      relative_path = relative_path[len(self.LAYOUTTEST_HTTP_DIR):]
      port = 8000
    # pending/http/tests/ run off port 9000 and ssl/ off 9443
    elif relative_path.startswith(self.PENDING_HTTP_DIR):
      relative_path = relative_path[len(self.PENDING_HTTP_DIR):]
      port = 9000
    # chrome/http/tests run off of port 8081 with the full path
    elif relative_path.find("/http/") >= 0:
      print relative_path
      port = 8081

    # We want to run off of the http server
    if port:
      if relative_path.startswith("ssl/"):
        port += 443
        use_ssl = True
      return PlatformUtility._FilenameToUri(self,
                                            relative_path, 
                                            use_http=True,
                                            use_ssl=use_ssl,
                                            port=port)

    # Run off file://
    return PlatformUtility._FilenameToUri(self, full_path, use_http=False, 
                                          use_ssl=False, port=0)

  def LigHTTPdExecutablePath(self):
    """Returns the executable path to start LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'mac', 'bin', 'lighttpd')

  def LigHTTPdModulePath(self):
    """Returns the library module path for LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'mac', 'lib')

  def LigHTTPdPHPPath(self):
    """Returns the PHP executable path for LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'mac', 'bin', 'php-cgi')

  def ShutDownHTTPServer(self, server_process):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.
    
    Args:
      server_process: The subprocess object representing the running server
    """
    subprocess.Popen(('kill', '-TERM', '%d' % server_process.pid),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()
  
  def TestShellBinary(self):
    """The name of the binary for TestShell."""
    return 'TestShell'
  
  def TestShellBinaryPath(self, target):
    """Return the platform-specific binary path for our TestShell.
    
    Args:
      target: Build target mode (debug or release)
    """
    # TODO(pinkerton): make |target| happy with case-sensitive file systems.
    return PathFromBase('xcodebuild', target, 'TestShell.app',
                        'Contents', 'MacOS', self.TestShellBinary())

  def TestListPlatformDir(self):
    """Return the platform-specific directory for where the test lists live"""
    return 'mac'
