// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/plugin/npobject_util.h"

#include "chrome/common/plugin_messages.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/webkit_glue.h"

// true if the current process is a plugin process, false if it's a renderer
// process.
static bool g_plugin_process;

namespace {
// The next 7 functions are called by the plugin code when it's using the
// NPObject.  Plugins always ignore the functions in NPClass (except allocate
// and deallocate), and instead just use the function pointers that were
// passed in NPInitialize.
// When the renderer interacts with an NPObject from the plugin, it of course
// uses the function pointers in its NPClass structure.
static bool NPN_HasMethodPatch(NPP npp,
                               NPObject *npobj,
                               NPIdentifier methodName) {
  return NPObjectProxy::NPHasMethod(npobj, methodName);
}

static bool NPN_InvokePatch(NPP npp, NPObject *npobj,
                            NPIdentifier methodName,
                            const NPVariant *args,
                            uint32_t argCount,
                            NPVariant *result) {
  return NPObjectProxy::NPInvokePrivate(npp, npobj, false, methodName, args, argCount, result);
}

static bool NPN_InvokeDefaultPatch(NPP npp,
                                   NPObject *npobj,
                                   const NPVariant *args,
                                   uint32_t argCount,
                                   NPVariant *result) {
  return NPObjectProxy::NPInvokePrivate(npp, npobj, true, 0, args, argCount, result);
}

static bool NPN_HasPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName) {
  return NPObjectProxy::NPHasProperty(npobj, propertyName);
}

static bool NPN_GetPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName,
                                 NPVariant *result) {
  return NPObjectProxy::NPGetProperty(npobj, propertyName, result);
}

static bool NPN_SetPropertyPatch(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName,
                                 const NPVariant *value) {
  return NPObjectProxy::NPSetProperty(npobj, propertyName, value);
}

static bool NPN_RemovePropertyPatch(NPP npp,
                                    NPObject *npobj,
                                    NPIdentifier propertyName) {
  return NPObjectProxy::NPRemoveProperty(npobj, propertyName);
}

static bool NPN_EvaluatePatch(NPP npp,
                              NPObject *npobj,
                              NPString *script,
                              NPVariant *result) {
  return NPObjectProxy::NPNEvaluate(npp, npobj, script, result);
}


static void NPN_SetExceptionPatch(NPObject *obj,
                                  const NPUTF8 *message) {
  return NPObjectProxy::NPNSetException(obj, message);
}

static bool NPN_EnumeratePatch(NPP npp, NPObject *obj,
                               NPIdentifier **identifier, uint32_t *count) {
  return NPObjectProxy::NPNEnumerate(obj, identifier, count);
}

// The overrided table of functions provided to the plugin.
NPNetscapeFuncs *GetHostFunctions() {
  static bool init = false;
  static NPNetscapeFuncs host_funcs;
  if (init)
    return &host_funcs;

  memset(&host_funcs, 0, sizeof(host_funcs));
  host_funcs.invoke = NPN_InvokePatch;
  host_funcs.invokeDefault = NPN_InvokeDefaultPatch;
  host_funcs.evaluate = NPN_EvaluatePatch;
  host_funcs.getproperty = NPN_GetPropertyPatch;
  host_funcs.setproperty = NPN_SetPropertyPatch;
  host_funcs.removeproperty = NPN_RemovePropertyPatch;
  host_funcs.hasproperty = NPN_HasPropertyPatch;
  host_funcs.hasmethod = NPN_HasMethodPatch;
  host_funcs.setexception = NPN_SetExceptionPatch;
  host_funcs.enumerate = NPN_EnumeratePatch;

  init = true;
  return &host_funcs;
}

}

void PatchNPNFunctions() {
  g_plugin_process = true;
  NPNetscapeFuncs* funcs = GetHostFunctions();
  NPAPI::PluginHost::Singleton()->PatchNPNetscapeFuncs(funcs);
}

bool IsPluginProcess() {
  return g_plugin_process;
}

void CreateNPIdentifierParam(NPIdentifier id, NPIdentifier_Param* param) {
  param->is_string = NPN_IdentifierIsString(id);
  if (param->is_string) {
    NPUTF8* utf8 = NPN_UTF8FromIdentifier(id);
    param->string = utf8;
    NPN_MemFree(utf8);
  } else {
    param->number = NPN_IntFromIdentifier(id);
  }
}

NPIdentifier CreateNPIdentifier(const  NPIdentifier_Param& param) {
  if (param.is_string) {
    return NPN_GetStringIdentifier(param.string.c_str());
  } else {
    return NPN_GetIntIdentifier(param.number);
  }
}

void CreateNPVariantParam(const NPVariant& variant,
                          PluginChannelBase* channel,
                          NPVariant_Param* param,
                          bool release) {
  switch (variant.type) {
    case NPVariantType_Void:
      param->type = NPVARIANT_PARAM_VOID;
      break;
    case NPVariantType_Null:
      param->type = NPVARIANT_PARAM_NULL;
      break;
    case NPVariantType_Bool:
      param->type = NPVARIANT_PARAM_BOOL;
      param->bool_value = variant.value.boolValue;
      break;
    case NPVariantType_Int32:
      param->type = NPVARIANT_PARAM_INT;
      param->int_value = variant.value.intValue;
      break;
    case NPVariantType_Double:
      param->type = NPVARIANT_PARAM_DOUBLE;
      param->double_value = variant.value.doubleValue;
      break;
    case NPVariantType_String:
      param->type = NPVARIANT_PARAM_STRING;
      if (variant.value.stringValue.UTF8Length) {
        param->string_value = std::string(variant.value.stringValue.UTF8Characters,
                                          variant.value.stringValue.UTF8Length);
      }
      break;
    case NPVariantType_Object:
    {
      if (variant.value.objectValue->_class == NPObjectProxy::npclass()) {
        param->type = NPVARIANT_PARAM_OBJECT_POINTER;
        param->npobject_pointer =
            NPObjectProxy::GetProxy(variant.value.objectValue)->npobject_ptr();
        // Don't release, because our original variant is the same as our proxy.
        release = false;
      } else {
        // NPObjectStub adds its own reference to the NPObject it owns, so if we
        // were supposed to release the corresponding variant (release==true),
        // we should still do that.
        param->type = NPVARIANT_PARAM_OBJECT_ROUTING_ID;
        int route_id = channel->GenerateRouteID();
        NPObjectStub* object_stub = new NPObjectStub(
            variant.value.objectValue, channel, route_id);
        param->npobject_routing_id = route_id;
        param->npobject_pointer = variant.value.objectValue;
      }
      break;
    }
    default:
      NOTREACHED();
  }

  if (release)
    NPN_ReleaseVariantValue(const_cast<NPVariant*>(&variant));
}

void CreateNPVariant(const NPVariant_Param& param,
                     PluginChannelBase* channel,
                     NPVariant* result,
                     HANDLE modal_dialog_event) {
  switch (param.type) {
    case NPVARIANT_PARAM_VOID:
      result->type = NPVariantType_Void;
      break;
    case NPVARIANT_PARAM_NULL:
      result->type = NPVariantType_Null;
      break;
    case NPVARIANT_PARAM_BOOL:
      result->type = NPVariantType_Bool;
      result->value.boolValue = param.bool_value;
      break;
    case NPVARIANT_PARAM_INT:
      result->type = NPVariantType_Int32;
      result->value.intValue = param.int_value;
      break;
    case NPVARIANT_PARAM_DOUBLE:
      result->type = NPVariantType_Double;
      result->value.doubleValue = param.double_value;
      break;
    case NPVARIANT_PARAM_STRING:
      result->type = NPVariantType_String;
      result->value.stringValue.UTF8Characters =
          static_cast<NPUTF8 *>(_strdup(param.string_value.c_str()));
      result->value.stringValue.UTF8Length =
          static_cast<int>(param.string_value.size());
      break;
    case NPVARIANT_PARAM_OBJECT_ROUTING_ID:
      result->type = NPVariantType_Object;
      result->value.objectValue = NPObjectProxy::Create(channel,
                                                        param.npobject_routing_id,
                                                        param.npobject_pointer,
                                                        modal_dialog_event);
      break;
    case NPVARIANT_PARAM_OBJECT_POINTER:
      result->type = NPVariantType_Object;
      result->value.objectValue = static_cast<NPObject*>(param.npobject_pointer);
      NPN_RetainObject(result->value.objectValue);
      break;
    default:
      NOTREACHED();
  }
}
