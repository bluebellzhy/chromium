#!/usr/bin/bash -x
#
# In order to build KJS or V8 versions of Chrome, we need to create
# a custom configuration header.  This script creates it.
#
# Input
#   create-config.sh <OutputDir> <kjs|v8>
#
# Output
#   in the $Output\WebCore directory, creates a config.h
#   custom to the desired build setup
#
set -ex
#
# Step 1: Create the webkit config.h which is appropriate for our 
#         JavaScript engine.
#
if [[ "${OS}" != "Windows_NT" ]]
then
    WebCoreObjDir="$1/WebCore"
    JSHeadersDir="$1/WebCore/JavaScriptHeaders"
    CP="cp -p"
else
    WebCoreObjDir="$1\obj\WebCore"
    JSHeadersDir="$1\obj\WebCore\JavaScriptHeaders"
    CP="cp"
fi
mkdir -p "$WebCoreObjDir"
rm -f $WebCoreObjDir/definitions.h 2> /dev/null


if [[ "$2" = "kjs" ]]
then
  cat > $WebCoreObjDir/definitions.h << -=EOF=-
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
-=EOF=-
else 
  cat > $WebCoreObjDir/definitions.h << -=EOF=-
#define WTF_USE_V8_BINDING 1
#define WTF_USE_NPOBJECT 1
-=EOF=-
fi

pwd
cat ../../config.h.in $WebCoreObjDir/definitions.h > $WebCoreObjDir/config.h.new
if [[ "${OS}" = "Windows_NT" ]] || \
   ! diff -q $WebCoreObjDir/config.h.new $WebCoreObjDir/config.h >& /dev/null
then
  mv $WebCoreObjDir/config.h.new $WebCoreObjDir/config.h
else
  rm $WebCoreObjDir/config.h.new
fi

#
# Step 2: Populate the JavaScriptHeaders based on the selected
#         JavaScript engine.
#
mkdir -p $JSHeadersDir
JavaScriptCoreSrcDir="../../../third_party/WebKit/JavaScriptCore"
if [[ "$2" = "kjs" ]]
then
  mkdir -p $JSHeadersDir/JavaScriptCore
  # Note, we'd like to copy dir/*.h here, but the directories
  # are poluted with files like Node.h.  Copying the Node.h
  # from these directories is fatal.  So, we have to hand copy just
  # the right list of files.  Hooray!
  $CP $JavaScriptCoreSrcDir/API/APICast.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSBase.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSValueRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSObjectRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSContextRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSStringRef.h $JSHeadersDir/JavaScriptCore

  $CP $JavaScriptCoreSrcDir/bindings/npapi.h $JSHeadersDir
  $CP $JavaScriptCoreSrcDir/bindings/npruntime.h $JSHeadersDir
  $CP $JavaScriptCoreSrcDir/bindings/npruntime_internal.h $JSHeadersDir
  $CP $JavaScriptCoreSrcDir/bindings/npruntime_impl.h $JSHeadersDir
  $CP $JavaScriptCoreSrcDir/bindings/npruntime_priv.h $JSHeadersDir

  $CP $JavaScriptCoreSrcDir/bindings/runtime.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/bindings/np_jsobject.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/bindings/runtime_object.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/bindings/runtime_root.h $JSHeadersDir/JavaScriptCore

  $CP $JavaScriptCoreSrcDir/kjs/JSImmediate.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/JSLock.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/JSType.h $JSHeadersDir/JavaScriptCore
  $CP ../../../webkit/pending/kjs/collector.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/interpreter.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/protect.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/ustring.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/value.h $JSHeadersDir/JavaScriptCore

  $CP $JavaScriptCoreSrcDir/wtf/HashCountedSet.h $JSHeadersDir/JavaScriptCore
else 
  $CP $JavaScriptCoreSrcDir/bindings/npapi.h $JSHeadersDir
  $CP $JavaScriptCoreSrcDir/bindings/npruntime.h $JSHeadersDir
  $CP ../../../webkit/port/bindings/v8/npruntime_priv.h $JSHeadersDir
fi

if [[ "${OS}" = "Windows_NT" ]]
then
  $CP $JavaScriptCoreSrcDir/os-win32/stdint.h $JSHeadersDir
fi
