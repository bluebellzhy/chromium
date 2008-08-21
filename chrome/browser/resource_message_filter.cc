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

#include "chrome/browser/resource_message_filter.h"

#include "base/histogram.h"
#include "base/thread.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_widget_helper.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/ipc_message_macros.h"
#include "chrome/common/render_messages.h"
#include "net/base/cookie_monster.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webplugin.h"

void ResourceMessageFilter::OnGetMonitorInfoForWindow(
    HWND window, MONITORINFOEX* monitor_info) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
  monitor_info->cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(monitor, monitor_info);
}

void ResourceMessageFilter::OnLoadFont(LOGFONT font) {
  // If renderer is running in a sandbox, GetTextMetrics
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics will try to load the font
  // from the font file. However, the sandboxed renderer does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.

  // Maintain a circular queue for the fonts and DCs to be cached.
  // font_index maintains next available location in the queue.
  static const int kFontCacheSize = 32;
  static HFONT fonts[kFontCacheSize] = {0};
  static HDC hdcs[kFontCacheSize] = {0};
  static size_t font_index = 0;

  UMA_HISTOGRAM_COUNTS_100(L"Memory.CachedFontAndDC",
      fonts[kFontCacheSize-1] ? kFontCacheSize : static_cast<int>(font_index));

  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  if (fonts[font_index] || hdcs[font_index]) {
    // We already have too many fonts, we will delete one and take it's place.
    DeleteObject(fonts[font_index]);
    ReleaseDC(NULL, hdcs[font_index]);
  }

  fonts[font_index] = font_handle;
  hdcs[font_index] = hdc;
  font_index = (font_index + 1) % kFontCacheSize;
}

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    int render_process_host_id,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    SpellChecker* spellchecker)
    : channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      render_process_host_id_(render_process_host_id),
      render_handle_(NULL),
      request_context_(profile->GetRequestContext()),
      render_widget_helper_(render_widget_helper),
      spellchecker_(spellchecker) {

  DCHECK(request_context_.get());
  DCHECK(request_context_->cookie_store());
}

ResourceMessageFilter::~ResourceMessageFilter() {
  if (render_handle_)
    CloseHandle(render_handle_);
}

// Called on the IPC thread:
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!render_handle_);
  render_handle_ = OpenProcess(PROCESS_DUP_HANDLE|PROCESS_TERMINATE,
                               FALSE, peer_pid);
  CHECK(render_handle_);
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(render_process_host_id_);
}

// Called on the IPC thread:
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ResourceMessageFilter, message, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateView, OnMsgCreateView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
    // TODO(brettw): we should get the view ID for this so the resource
    // dispatcher can prioritize things based on the visible view.
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UploadProgress_ACK, OnUploadProgressACK)

    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncLoad, OnSyncLoad)

    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetDataDir, OnGetDataDir)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginMessage, OnPluginMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMonitorInfoForWindow,
                        OnGetMonitorInfoForWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginPath, OnGetPluginPath)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
        OnReceiveContextMenuMsg(message))
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SpellCheck, OnSpellCheck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DnsPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_PaintRect,
        render_widget_helper_->DidReceivePaintMsg(message))
    IPC_MESSAGE_FORWARD(ViewHostMsg_ClipboardClear,
                        static_cast<Clipboard*>(GetClipboardService()),
                        Clipboard::Clear)
    IPC_MESSAGE_FORWARD(ViewHostMsg_ClipboardWriteText,
                        static_cast<Clipboard*>(GetClipboardService()),
                        Clipboard::WriteText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteHTML,
                        OnClipboardWriteHTML)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteBookmark,
                        OnClipboardWriteBookmark)
    // We need to do more work to marshall around bitmaps
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteBitmap,
                        OnClipboardWriteBitmap)
    IPC_MESSAGE_FORWARD(ViewHostMsg_ClipboardWriteWebSmartPaste,
                        static_cast<Clipboard*>(GetClipboardService()),
                        Clipboard::WriteWebSmartPaste)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardIsFormatAvailable,
                        OnClipboardIsFormatAvailable)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadText, OnClipboardReadText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadAsciiText,
                        OnClipboardReadAsciiText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadHTML,
                        OnClipboardReadHTML)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetWindowRect, OnGetWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromExtension,
                        OnGetMimeTypeFromExtension)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromFile,
                        OnGetMimeTypeFromFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPreferredExtensionForMimeType,
                        OnGetPreferredExtensionForMimeType)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                        OnGetCPBrowsingContext)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                    OnScriptedPrint)
    IPC_MESSAGE_UNHANDLED(
        handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    RenderProcessHost::BadMessageTerminateProcess(message.type(),
                                                  render_handle_);
  }

  return handled;
}

class ContextMenuMessageDispatcher : public Task {
 public:
  ContextMenuMessageDispatcher(
      int render_process_host_id,
      const ViewHostMsg_ContextMenu& context_menu_message)
      : render_process_host_id_(render_process_host_id),
        context_menu_message_(context_menu_message) {
  }

  void Run() {
    // Forward message to normal routing route.
    RenderProcessHost* host =
        RenderProcessHost::FromID(render_process_host_id_);
    if (host)
      host->OnMessageReceived(context_menu_message_);
  }

 private:
  int render_process_host_id_;
  const ViewHostMsg_ContextMenu context_menu_message_;
};

void ResourceMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ViewHostMsg_ContextMenu_Params params;
  if (!IPC::ParamTraits<ViewHostMsg_ContextMenu_Params>::
      Read(&msg, &iter, &params))
    return;

  // Fill in the dictionary suggestions if required.
  if (!params.misspelled_word.empty() &&
      spellchecker_ != NULL) {
    int misspell_location, misspell_length;
    spellchecker_->SpellCheckWord(params.misspelled_word.c_str(),
       static_cast<int>(params.misspelled_word.length()),
       &misspell_location, &misspell_length,
       &params.dictionary_suggestions);
  }

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  render_widget_helper_->ui_loop()->PostTask(FROM_HERE,
      new ContextMenuMessageDispatcher(render_process_host_id_,
                                       context_menu_message));
}

// Called on the IPC thread:
bool ResourceMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void ResourceMessageFilter::OnMsgCreateView(int opener_id,
                                            bool user_gesture,
                                            int* route_id,
                                            HANDLE* modal_dialog_event) {
  render_widget_helper_->CreateView(opener_id, user_gesture, route_id,
                                    modal_dialog_event, render_handle_);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id, int* route_id) {
  render_widget_helper_->CreateWidget(opener_id, route_id);
}

void ResourceMessageFilter::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ViewHostMsg_Resource_Request& request) {
  resource_dispatcher_host_->BeginRequest(this,
                                          render_handle_,
                                          render_process_host_id_,
                                          message.routing_id(),
                                          request_id,
                                          request,
                                          request_context_,
                                          NULL);
}

void ResourceMessageFilter::OnDataReceivedACK(int request_id) {
  resource_dispatcher_host_->OnDataReceivedACK(render_process_host_id_,
                                               request_id);
}

void ResourceMessageFilter::OnUploadProgressACK(int request_id) {
  resource_dispatcher_host_->OnUploadProgressACK(render_process_host_id_,
                                                 request_id);
}

void ResourceMessageFilter::OnCancelRequest(int request_id) {
  resource_dispatcher_host_->CancelRequest(render_process_host_id_, request_id,
                                           true);
}

void ResourceMessageFilter::OnClosePageACK(int new_render_process_host_id,
                                           int new_request_id) {
  resource_dispatcher_host_->OnClosePageACK(new_render_process_host_id,
                                            new_request_id);
}

void ResourceMessageFilter::OnSyncLoad(
    int request_id,
    const ViewHostMsg_Resource_Request& request,
    IPC::Message* sync_result) {
  resource_dispatcher_host_->BeginRequest(this,
                                          render_handle_,
                                          render_process_host_id_,
                                          sync_result->routing_id(),
                                          request_id,
                                          request,
                                          request_context_,
                                          sync_result);
}

void ResourceMessageFilter::OnSetCookie(const GURL& url,
                                        const GURL& policy_url,
                                        const std::string& cookie) {
  if (request_context_->cookie_policy()->CanSetCookie(url, policy_url))
    request_context_->cookie_store()->SetCookie(url, cookie);
}

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& policy_url,
                                         std::string* cookies) {
  if (request_context_->cookie_policy()->CanGetCookies(url, policy_url))
    *cookies = request_context_->cookie_store()->GetCookies(url);
}

void ResourceMessageFilter::OnGetDataDir(std::wstring* data_dir) {
  *data_dir = plugin_service_->GetChromePluginDataDir();
}

void ResourceMessageFilter::OnPluginMessage(const std::wstring& dll_path,
                                            const std::vector<uint8>& data) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(dll_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}

void ResourceMessageFilter::OnGetPlugins(bool refresh,
                                         std::vector<WebPluginInfo>* plugins) {
  plugin_service_->GetPlugins(refresh, plugins);
}

void ResourceMessageFilter::OnGetPluginPath(const GURL& url,
                                            const std::string& mime_type,
                                            const std::string& clsid,
                                            std::wstring* filename,
                                            std::string* url_mime_type) {
  *filename = plugin_service_->GetPluginPath(url, mime_type, clsid,
                                             url_mime_type);
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::string& clsid,
                                                  const std::wstring& locale,
                                                  IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(this, url, mime_type, clsid,
                                       locale, reply_msg);
}

void ResourceMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                          const GURL& url,
                                          const GURL& referrer) {
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           render_process_host_id_,
                                           message.routing_id(),
                                           request_context_);
}

void ResourceMessageFilter::OnClipboardWriteHTML(const std::wstring& markup,
                                                 const GURL& src_url) {
  GetClipboardService()->WriteHTML(markup, src_url.spec());
}

void ResourceMessageFilter::OnClipboardWriteBookmark(const std::wstring& title,
                                                     const GURL& url) {
  GetClipboardService()->WriteBookmark(title, url.spec());
}

void ResourceMessageFilter::OnClipboardWriteBitmap(
    SharedMemoryHandle bitmap_buf, gfx::Size size) {
  // hbitmap here is only valid in the context of the renderer.  We need to
  // import it into our process using SharedMemory in order to get a handle
  // that is valid.
  //
  // We need to ask for write permission to the shared memory in order to
  // call WriteBitmapFromSharedMemory
  SharedMemory shared_mem(bitmap_buf, false, render_handle_);
  GetClipboardService()->WriteBitmapFromSharedMemory(shared_mem, size);
}

void ResourceMessageFilter::OnClipboardIsFormatAvailable(unsigned int format,
                                                         bool* result) {
  DCHECK(result);
  *result = GetClipboardService()->IsFormatAvailable(format);
}

void ResourceMessageFilter::OnClipboardReadText(std::wstring* result) {
  GetClipboardService()->ReadText(result);
}

void ResourceMessageFilter::OnClipboardReadAsciiText(std::string* result) {
  GetClipboardService()->ReadAsciiText(result);
}

void ResourceMessageFilter::OnClipboardReadHTML(std::wstring* markup,
                                                GURL* src_url) {
  std::string src_url_str;
  GetClipboardService()->ReadHTML(markup, &src_url_str);
  *src_url = GURL(src_url_str);
}

void ResourceMessageFilter::OnGetWindowRect(HWND hwnd_view_container,
                                            gfx::Rect *rect) {
  RECT window_rect = {0};
  HWND window = ::GetAncestor(hwnd_view_container, GA_ROOT);
  GetWindowRect(window, &window_rect);
  *rect = window_rect;
}

void ResourceMessageFilter::OnGetMimeTypeFromExtension(
    const std::wstring& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void ResourceMessageFilter::OnGetMimeTypeFromFile(
    const std::wstring& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

void ResourceMessageFilter::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, std::wstring* ext) {
  net::GetPreferredExtensionForMimeType(mime_type, ext);
}

void ResourceMessageFilter::OnGetCPBrowsingContext(uint32* context) {
  // Always allocate a new context when a plugin requests one, since it needs to
  // be unique for that plugin instance.
  *context =
      CPBrowsingContextManager::Instance()->Allocate(request_context_.get());
}

void ResourceMessageFilter::OnDuplicateSection(
    SharedMemoryHandle renderer_handle,
    SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  SharedMemory shared_buf(renderer_handle, true, render_handle_);
  shared_buf.GiveToProcess(GetCurrentProcess(), browser_handle);
}

void ResourceMessageFilter::OnResourceTypeStats(
    const CacheManager::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS(L"WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.css_stylesheets.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xsl_stylesheets.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));
}

void ResourceMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             task);
}

void ResourceMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    printer_query->settings().RenderParams(&params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query->cookie() && printer_query->settings().dpi()) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

void ResourceMessageFilter::OnScriptedPrint(HWND host_window,
                                            int cookie,
                                            int expected_pages_count,
                                            IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnScriptedPrintReply,
      printer_query,
      reply_msg);
  // Shows the Print... dialog box. This is asynchronous, only the IPC message
  // sender will hang until the Print dialog is dismissed.
  if (!host_window || !IsWindow(host_window)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    host_window = GetDesktopWindow();
  } else {
    host_window = GetAncestor(host_window, GA_ROOTOWNER);
  }
  DCHECK(host_window);
  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_window,
                             expected_pages_count,
                             task);
}

void ResourceMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    printer_query->settings().RenderParams(&params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages =
        printing::PageRange::GetPages(printer_query->settings().ranges);
  }
  ViewHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

// static
ClipboardService* ResourceMessageFilter::GetClipboardService() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ClipboardService* clipboard_service = new ClipboardService();

  return clipboard_service;
}

// Notes about SpellCheck.
//
// Spellchecking generally uses a fair amount of RAM.  For this reason, we load
// the spellcheck dictionaries into the browser process, and all renderers ask
// the browsers to do SpellChecking.
//
// Requests to do SpellCheck come in on the IO thread.  Unfortunately, the
// first spell check requires loading of the spell check tables, which is an
// expensive operation, both in terms of disk access and memory usage.  Even
// though initialization only happens once, we don't want to do it on the IO
// thread to keep the browser snappy.
//
// To implement:
// If SpellCheck has not been initialized, we send a request to the
// browser's file_thread to do the initialization.  Once completed, it
// requests back to the IO thread to send the response to the renderer.
// If SpellCheck has been initialized, we go ahead and do the lookup
// directly on the IO thread, as this operation is fairly quick.
//

// The SpellCheckReplyTask replies to a Renderer for a pending SpellCheck
// request.  The task is created on the Browser's file_thread, and
// executes on the io_thread.
class SpellCheckReplyTask : public Task {
 public:
  explicit SpellCheckReplyTask(ResourceMessageFilter* filter,
                               IPC::Message* reply_msg,
                               int misspell_location,
                               int misspell_length) :
     filter_(filter),
     misspell_location_(misspell_location),
     misspell_length_(misspell_length),
     reply_msg_(reply_msg) {
  }

  void Run() {
    ViewHostMsg_SpellCheck::WriteReplyParams(reply_msg_, misspell_location_,
                                             misspell_length_);
    filter_->Send(reply_msg_);
  }

 private:
  scoped_refptr<ResourceMessageFilter> filter_;
  IPC::Message* reply_msg_;
  int misspell_location_;
  int misspell_length_;
};

// The SpellCheckTask initializes the SpellCheck library and does a
// a SpellCheck Lookup.  The task is initiated from the io_thread
// but executes on the browser's file_thread.
class SpellCheckTask : public Task {
 public:
  SpellCheckTask(ResourceMessageFilter *filter,
                 const std::wstring& word,
                 IPC::Message* reply_msg) :
    filter_(filter),
    word_(word),
    reply_msg_(reply_msg) {
  }
  void Run() {
    SpellChecker *checker = filter_->spellchecker();
    int misspell_location = 0;
    int misspell_length = 0;
    if (checker)
      checker->SpellCheckWord(word_.c_str(), static_cast<int>(word_.length()),
                              &misspell_location, &misspell_length, NULL);
    Thread* io_thread = g_browser_process->io_thread();
    if (io_thread) {
      io_thread->message_loop()->PostTask(FROM_HERE,
          new SpellCheckReplyTask(filter_, reply_msg_,
              misspell_location, misspell_length));
    } else {
      // If the io_thread is NULL, we're tearing down.  This shouldn't happen,
      // but if it does, just leave this request pending and let things go down
      // on their own.
      // We can't send the reply from this thread, so there is nothing we can
      // do.
    }
  }

 private:
  scoped_refptr<ResourceMessageFilter> filter_;
  const std::wstring word_;
  IPC::Message* reply_msg_;
};

void ResourceMessageFilter::OnSpellCheck(const std::wstring& word,
                                         IPC::Message* reply_msg) {
  // Get the SpellChecker.  If initialized, do the work on this thread.
  if (spellchecker_ != NULL) {
    int misspell_location;
    int misspell_length;
    spellchecker_->SpellCheckWord(word.c_str(),
                                  static_cast<int>(word.length()),
                                  &misspell_location, &misspell_length, NULL);
    ViewHostMsg_SpellCheck::WriteReplyParams(reply_msg, misspell_location,
                                             misspell_length);
    Send(reply_msg);
    return;
  }

  // The SpellChecker is not initialized.  We can't run this on the IO
  // thread.  Sent it over to the file thread and let it do the dirty work.
  MessageLoop* file_loop = ChromeThread::GetMessageLoop(ChromeThread::FILE);
  if (file_loop) {
    // TODO(abarth): What if the file thread is being destroyed on the UI?
    file_loop->PostTask(FROM_HERE, new SpellCheckTask(this, word, reply_msg));
  } else {
    // If the file_loop is NULL, we're tearing down.  This shouldn't happen,
    // but if it does, just leave this request pending and let things go down
    // on their own.
    // We could send a reply, but it doesn't seem necessary.
  }
}

void ResourceMessageFilter::OnDnsPrefetch(
         const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}
