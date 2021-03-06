// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#include <new.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/icu_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/resource_bundle.h"
#include "sandbox/src/sandbox.h"
#include "tools/memory_watcher/memory_watcher.h"

extern int BrowserMain(CommandLine &, int, sandbox::BrokerServices*);
extern int RendererMain(CommandLine &, int, sandbox::TargetServices*);
extern int PluginMain(CommandLine &, int, sandbox::TargetServices*);

// TODO(erikkay): isn't this already defined somewhere?
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line, int show_command);
}

namespace {

const wchar_t kProfilingDll[] = L"memory_watcher.dll";

// Load the memory profiling DLL.  All it needs to be activated
// is to be loaded.  Return true on success, false otherwise.
bool LoadMemoryProfiler() {
  HMODULE prof_module = LoadLibrary(kProfilingDll);
  return prof_module != NULL;
}

CAppModule _Module;

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
}

void PureCall() {
  __debugbreak();
}

int OnNoMemory(size_t memory_size) {
  __debugbreak();
  // Return memory_size so it is not optimized out. Make sure the return value
  // is at least 1 so malloc/new is retried, especially useful when under a
  // debugger.
  return memory_size ? static_cast<int>(memory_size) : 1;
}

// Handlers to silently dump the current process when there is an assert in
// chrome.
void ChromeAssert(const std::string& str) {
  // Get the breakpad pointer from chrome.exe
  typedef void (__stdcall *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(L"chrome.exe"), "DumpProcess"));
  if (DumpProcess)
    DumpProcess();
}

#pragma optimize("", on)

}  // namespace

DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line, int show_command) {
  // Register the invalid param handler and pure call handler to be able to
  // notify breakpad when it happens.
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Gather allocation failure.
  _set_new_handler(&OnNoMemory);
  // Make sure malloc() calls the new handler too.
  _set_new_mode(1);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  // Initialize the command line.
  CommandLine parsed_command_line;

#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!parsed_command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif

  // Enable the low fragmentation heap for the CRT heap. The heap is not changed
  // if the process is run under the debugger is enabled or if certain gflags
  // are set.
  bool use_lfh = false;
  if (parsed_command_line.HasSwitch(switches::kUseLowFragHeapCrt))
    use_lfh = parsed_command_line.GetSwitchValue(switches::kUseLowFragHeapCrt)
        != L"false";
  if (use_lfh) {
    void* crt_heap = reinterpret_cast<void*>(_get_heap_handle());
    ULONG enable_lfh = 2;
    HeapSetInformation(crt_heap, HeapCompatibilityInformation,
                       &enable_lfh, sizeof(enable_lfh));
  }

  // Initialize the Chrome path provider.
  chrome::RegisterPathProvider();

  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
  StatsTable *stats_table = new StatsTable(chrome::kStatsFilename,
      chrome::kStatsMaxThreads, chrome::kStatsMaxCounters);
  StatsTable::set_current(stats_table);

  StatsScope<StatsCounterTimer>
      startup_timer(chrome::Counters::chrome_main());

  // Enable the heap profiler as early as possible!
  if (parsed_command_line.HasSwitch(switches::kMemoryProfiling))
    if (!LoadMemoryProfiler())
      exit(-1);

  // Enable Message Loop related state asap.
  if (parsed_command_line.HasSwitch(switches::kMessageLoopHistogrammer))
    MessageLoop::EnableHistogrammer(true);

  sandbox::TargetServices* target_services = NULL;
  sandbox::BrokerServices* broker_services = NULL;
  if (sandbox_info) {
    target_services = sandbox_info->target_services;
    broker_services = sandbox_info->broker_services;
  }

  std::wstring process_type =
    parsed_command_line.GetSwitchValue(switches::kProcessType);

  // Checks if the sandbox is enabled in this process and initializes it if this
  // is the case. The crash handler depends on this so it has to be done before
  // its initialization.
  if (target_services && !parsed_command_line.HasSwitch(switches::kNoSandbox)) {
    if ((process_type == switches::kRendererProcess) ||
        (process_type == switches::kPluginProcess &&
         parsed_command_line.HasSwitch(switches::kSafePlugins))) {
      target_services->Init();
    }
  }

  _Module.Init(NULL, instance);

  // Notice a user data directory override if any
  const std::wstring user_data_dir =
      parsed_command_line.GetSwitchValue(switches::kUserDataDir);
  if (!user_data_dir.empty())
    PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

  bool single_process =
    parsed_command_line.HasSwitch(switches::kSingleProcess);
  if (single_process)
    RenderProcessHost::set_run_renderer_in_process(true);

  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);

  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitChromeLogging(parsed_command_line, file_state);

#ifdef NDEBUG
  if (parsed_command_line.HasSwitch(switches::kSilentDumpOnDCHECK) &&
      parsed_command_line.HasSwitch(switches::kEnableDCHECK)) {
    logging::SetLogAssertHandler(ChromeAssert);
  }
#endif  // NDEBUG

  if (!process_type.empty()) {
    // Initialize ResourceBundle which handles files loaded from external
    // sources.  The language should have been passed in to us from the
    // browser process as a command line flag.
    ResourceBundle::InitSharedInstance(std::wstring());
  }

  startup_timer.Stop();  // End of Startup Time Measurement.

  int rv;
  if (process_type == switches::kRendererProcess) {
    rv = RendererMain(parsed_command_line, show_command, target_services);
  } else if (process_type == switches::kPluginProcess) {
    rv = PluginMain(parsed_command_line, show_command, target_services);
  } else if (process_type.empty()) {
    int ole_result = OleInitialize(NULL);
    DCHECK(ole_result == S_OK);
    rv = BrowserMain(parsed_command_line, show_command, broker_services);
    OleUninitialize();
  } else {
    NOTREACHED() << "Unknown process type";
    rv = -1;
  }

  if (!process_type.empty()) {
    ResourceBundle::CleanupSharedInstance();
  }

#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

  _Module.Term();

  logging::CleanupChromeLogging();

  return rv;
}

