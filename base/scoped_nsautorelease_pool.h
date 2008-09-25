// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_NSAUTORELEASE_POOL_H_
#define BASE_SCOPED_NSAUTORELEASE_POOL_H_

#include "base/basictypes.h"

#if defined(OS_MACOSX)
#if defined(__OBJC__)
@class NSAutoreleasePool;
#else  // __OBJC__
class NSAutoreleasePool;
#endif  // __OBJC__
#endif  // OS_MACOSX

namespace base {

// On the Mac, ScopedNSAutoreleasePool allocates an NSAutoreleasePool when
// instantiated and sends it a -drain message when destroyed.  This allows an
// autorelease pool to be maintained in ordinary C++ code without bringing in
// any direct Objective-C dependency.
//
// On other platforms, ScopedNSAutoreleasePool is an empty object with no
// effects.  This allows it to be used directly in cross-platform code without
// ugly #ifdefs.
class ScopedNSAutoreleasePool {
#if defined(OS_MACOSX)
 public:
  ScopedNSAutoreleasePool();
  ~ScopedNSAutoreleasePool();

 private:
  NSAutoreleasePool* autorelease_pool_;
#endif  // OS_MACOSX

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNSAutoreleasePool);
};

}  // namespace base

#endif  // BASE_SCOPED_NSAUTORELEASE_POOL_H_
