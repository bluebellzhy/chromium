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

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_process_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/perf/mem_usage.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

class MemoryTest : public UITest {
 public:
  MemoryTest() {
    show_window_ = true;

    // For now, turn off plugins because they crash like crazy.
    // TODO(mbelshe): Fix Chrome to not crash with plugins.
    CommandLine::AppendSwitch(&launch_arguments_, switches::kDisablePlugins);

    // Use the playback cache, but don't use playback events.
    CommandLine::AppendSwitch(&launch_arguments_, switches::kPlaybackMode);
    CommandLine::AppendSwitch(&launch_arguments_, switches::kNoEvents);

    // Compute the user-data-dir which contains our test cache.
    PathService::Get(base::DIR_EXE, &user_data_dir_);
    file_util::UpOneDirectory(&user_data_dir_);
    file_util::UpOneDirectory(&user_data_dir_);
    file_util::AppendToPath(&user_data_dir_, L"data");
    file_util::AppendToPath(&user_data_dir_, L"memory_test");
    file_util::AppendToPath(&user_data_dir_, L"general_mix");
    CommandLine::AppendSwitchWithValue(&launch_arguments_,
                                       switches::kUserDataDir,
                                       user_data_dir_);
  }

  // TODO(mbelshe): Separate this data to an external file.
  // This memory test loads a set of URLs across a set of tabs, maintaining the
  // number of concurrent open tabs at num_target_tabs.
  // <NEWTAB> is a special URL which informs the loop when we should create a
  // new tab.
  // <PAUSE> is a special URL that informs the loop to pause before proceeding
  // to the next URL.
  void RunTest(const wchar_t* test_name, int num_target_tabs) {
    std::string urls[] = {
      "http://www.yahoo.com/",
      "http://hotjobs.yahoo.com/career-articles-the_biggest_resume_mistake_you_can_make-436",
      "http://news.yahoo.com/s/ap/20080804/ap_on_re_mi_ea/odd_israel_home_alone",
      "http://news.yahoo.com/s/nm/20080729/od_nm/subway_dc",
      "http://search.yahoo.com/search?p=new+york+subway&ygmasrchbtn=web+search&fr=ush-news",
      "<NEWTAB>",
      "http://www.cnn.com/",
      "http://www.cnn.com/2008/SHOWBIZ/TV/08/03/applegate.cancer.ap/index.html",
      "http://www.cnn.com/2008/HEALTH/conditions/07/29/black.aids.report/index.html",
      "http://www.cnn.com/POLITICS/",
      "http://search.cnn.com/search.jsp?query=obama&type=web&sortBy=date&intl=false",
      "<NEWTAB>",
      "http://mail.google.com/",
      "http://mail.google.com/mail/?shva=1",
      "http://mail.google.com/mail/?shva=1#search/ipsec",
      "http://mail.google.com/mail/?shva=1#search/ipsec/ee29ae66165d417",
      "http://mail.google.com/mail/?shva=1#compose",
      "<NEWTAB>",
      "http://docs.google.com/",
      "<NEWTAB>",
      "http://calendar.google.com/",
      "<NEWTAB>",
      "http://maps.google.com/",
      "http://maps.google.com/maps/mpl?moduleurl=http://earthquake.usgs.gov/eqcenter/mapplets/earthquakes.xml&ie=UTF8&ll=20,170&spn=140.625336,73.828125&t=k&z=2",
      "http://maps.google.com/maps?f=q&hl=en&geocode=&q=1600+amphitheater+parkway,+mountain+view,+ca&ie=UTF8&z=13",
      "<NEWTAB>",
      "http://www.google.com/",
      "http://www.google.com/search?hl=en&q=food&btnG=Google+Search",
      "http://books.google.com/books?hl=en&q=food&um=1&ie=UTF-8&sa=N&tab=wp",
      "http://images.google.com/images?hl=en&q=food&um=1&ie=UTF-8&sa=N&tab=pi",
      "http://news.google.com/news?hl=en&q=food&um=1&ie=UTF-8&sa=N&tab=in",
      "http://www.google.com/products?sa=N&tab=nf&q=food",
      "<NEWTAB>",
      "http://www.scoundrelspoint.com/polyhedra/shuttle/index.html",
      "<PAUSE>",
      "<NEWTAB>",
      "http://ctho.ath.cx/toys/3d.html",
      "<PAUSE>",
      "<NEWTAB>",
      "http://www.youtube.com/",
      "http://www.youtube.com/results?search_query=funny&search_type=&aq=f",
      "http://www.youtube.com/watch?v=GuMMfgWhm3g",
      "<NEWTAB>",
      "http://www.craigslist.com/",
      "http://sfbay.craigslist.org/",
      "http://sfbay.craigslist.org/apa/",
      "http://sfbay.craigslist.org/sfc/apa/782398209.html",
      "http://sfbay.craigslist.org/sfc/apa/782347795.html",
      "http://sfbay.craigslist.org/sby/apa/782342791.html",
      "http://sfbay.craigslist.org/sfc/apa/782344396.html",
      "<NEWTAB>",
      "http://www.whitehouse.gov/",
      "http://www.whitehouse.gov/news/releases/2008/07/20080729.html",
      "http://www.whitehouse.gov/infocus/afghanistan/",
      "http://www.whitehouse.gov/infocus/africa/",
      "<NEWTAB>",
      "http://www.msn.com/",
      "http://msn.foxsports.com/horseracing/story/8409670/Big-Brown-rebounds-in-Haskell-Invitational?MSNHPHMA",
      "http://articles.moneycentral.msn.com/Investing/StockInvestingTrading/TheBiggestRiskToYourRetirement_SeriesHome.aspx",
      "http://articles.moneycentral.msn.com/Investing/StockInvestingTrading/TheSmartWayToGetRich.aspx",
      "http://articles.moneycentral.msn.com/Investing/ContrarianChronicles/TheFictionOfCorporateTransparency.aspx",
      "<NEWTAB>",
      "http://flickr.com/"
      "http://flickr.com/explore/interesting/2008/03/18/",
      "http://flickr.com/photos/chavals/2344906748/",
      "http://flickr.com/photos/rosemary/2343058024/",
      "http://flickr.com/photos/arbaa/2343235019/",
      "<NEWTAB>",
      "http://zh.wikipedia.org/wiki/%E6%B1%B6%E5%B7%9D%E5%A4%A7%E5%9C%B0%E9%9C%87",
      "http://zh.wikipedia.org/wiki/5%E6%9C%8812%E6%97%A5",
      "http://zh.wikipedia.org/wiki/5%E6%9C%8820%E6%97%A5",
      "http://zh.wikipedia.org/wiki/%E9%A6%96%E9%A1%B5",
      "<NEWTAB>",
      "http://www.nytimes.com/pages/technology/index.html",
      "http://pogue.blogs.nytimes.com/2008/07/17/a-candy-store-for-the-iphone/",
      "http://www.nytimes.com/2008/07/21/technology/21pc.html?_r=1&ref=technology&oref=slogin",
      "http://bits.blogs.nytimes.com/2008/07/19/a-wikipedian-challenge-convincing-arabic-speakers-to-write-in-arabic/",
      "<NEWTAB>",
      "http://www.amazon.com/exec/obidos/tg/browse/-/502394/ref=topnav_storetab_p",
      "http://www.amazon.com/Panasonic-DMC-TZ5K-Digital-Optical-Stabilized/dp/B0011Z8CCG/ref=pd_ts_p_17?ie=UTF8&s=photo",
      "http://www.amazon.com/Nikon-Coolpix-Digital-Vibration-Reduction/dp/B0012OI6HW/ref=pd_ts_p_24?ie=UTF8&s=photo",
      "http://www.amazon.com/Digital-SLRs-Cameras-Photo/b/ref=sv_p_2?ie=UTF8&node=3017941",
      "<NEWTAB>",
      "http://www.boston.com/bigpicture/2008/07/californias_continuing_fires.html",
      "http://www.boston.com/business/",
      "http://www.boston.com/business/articles/2008/07/29/staples_has_a_games_plan/",
      "http://www.boston.com/business/personalfinance/articles/2008/08/04/a_grim_forecast_for_heating_costs/",
      "<NEWTAB>",
      "http://arstechnica.com/",
      "http://arstechnica.com/news.ars/post/20080721-this-years-e3-substance-over-styleand-far-from-dead.html",
      "http://arstechnica.com/news.ars/post/20080729-ifpi-italian-police-take-down-italian-bittorrent-tracker.html",
      "http://arstechnica.com/news.ars/post/20080804-congress-wants-privacy-answers-from-google-ms-aol.html",
      "<NEWTAB>",
      "http://finance.google.com/finance?q=NASDAQ:AAPL",
      "http://finance.google.com/finance?q=GOOG&hl=en",
      "<NEWTAB>",
      "http://blog.wired.com/underwire/2008/07/futurama-gets-m.html",
      "http://blog.wired.com/cars/2008/07/gas-prices-hit.html",
      "<NEWTAB>",
      "http://del.icio.us/popular/programming",
      "http://del.icio.us/popular/",
      "http://del.icio.us/tag/",
      "<NEWTAB>",
      "http://gadgets.boingboing.net/2008/07/21/boom-computing.html",
      "http://3533.spreadshirt.com/us/US/Shop/",
      "<NEWTAB>",
      "http://www.autoblog.com/",
      "http://www.autoblog.com/2008/07/21/audi-introduces-the-next-mmi/",
      "http://www.autoblog.com/categories/auto-types/",
      "http://www.autoblog.com/category/sports/",
      "<NEWTAB>",
      "http://www.wikipedia.org/",
      "http://en.wikipedia.org/wiki/Main_Page",
      "http://fr.wikipedia.org/wiki/Accueil",
      "http://de.wikipedia.org/wiki/Hauptseite",
      "http://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8",
      "http://it.wikipedia.org/wiki/Pagina_principale",
      "http://nl.wikipedia.org/wiki/Hoofdpagina",
      "http://pt.wikipedia.org/wiki/P%C3%A1gina_principal",
      "http://es.wikipedia.org/wiki/Portada",
      "http://ru.wikipedia.org/wiki/%D0%97%D0%B0%D0%B3%D0%BB%D0%B0%D0%B2%D0%BD%D0%B0%D1%8F_%D1%81%D1%82%D1%80%D0%B0%D0%BD%D0%B8%D1%86%D0%B0",
      "<NEWTAB>",
      "http://www.google.com/translate_t?hl=en&text=This%20Is%20A%20Test%20Of%20missspellingsdfdf&sl=en&tl=ja",
    };

    // Record the initial CommitCharge.  This is a system-wide measurement,
    // so if other applications are running, they can create variance in this
    // test.
    size_t start_size = GetSystemCommitCharge();

    // Cycle through the URLs.
    scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
    scoped_ptr<TabProxy> tab(window->GetActiveTab());
    int expected_tab_count = 1;
    for (unsigned counter = 0; counter < arraysize(urls); ++counter) {
      std::string url = urls[counter];

      if (url == "<PAUSE>") {  // Special command to delay on this page
        Sleep(2000);
        continue;
      }

      if (url == "<NEWTAB>") {  // Special command to create a new tab
        if (++counter >= arraysize(urls))
          continue;  // Newtab was specified at end of list.  ignore.

        url = urls[counter];
        if (GetTabCount() < num_target_tabs) {
          EXPECT_TRUE(window->AppendTab(GURL(url)));
          expected_tab_count++;
          WaitUntilTabCount(expected_tab_count);
          tab.reset(window->GetActiveTab());
          continue;
        }

        int tab_index = counter % num_target_tabs;  // A pseudo-random tab.
        tab.reset(window->GetTab(tab_index));
      }

      tab->NavigateToURL(GURL(urls[counter]));
    }
    size_t stop_size = GetSystemCommitCharge();

    PrintResults(test_name, stop_size - start_size);
  }

  void PrintResults(const wchar_t* test_name, size_t commit_size) {
    PrintMemoryUsageInfo(test_name);
    wprintf(L"%ls_commit_charge_kb = %d\n", test_name, commit_size / 1024);
  }

  void PrintIOPerfInfo(const wchar_t* test_name) {
    printf("\n");
    BrowserProcessFilter chrome_filter(user_data_dir_);
    process_util::NamedProcessIterator
        chrome_process_itr(chrome::kBrowserProcessExecutableName,
                           &chrome_filter);

    const PROCESSENTRY32* chrome_entry;
    while (chrome_entry = chrome_process_itr.NextProcessEntry()) {
      uint32 pid = chrome_entry->th32ProcessID;
      HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION,
                                          false,
                                          pid);
      if (process_handle == NULL) {
        wprintf(L"Error opening process %d: %d\n", pid, GetLastError());
        continue;
      }

      scoped_ptr<process_util::ProcessMetrics> process_metrics;
      IO_COUNTERS io_counters;
      process_metrics.reset(
          process_util::ProcessMetrics::CreateProcessMetrics(process_handle));
      ZeroMemory(&io_counters, sizeof(io_counters));

      if (process_metrics.get()->GetIOCounters(&io_counters)) {
        wchar_t* browser_name = L"browser";
        wchar_t* render_name = L"render";
        wchar_t* chrome_name;
        if (pid == chrome_filter.browser_process_id()) {
          chrome_name = browser_name;
        } else {
          chrome_name = render_name;
        }

        // print out IO performance
        wprintf(L"%ls_%ls_read_op = %d\n",
                test_name, chrome_name, io_counters.ReadOperationCount);
        wprintf(L"%ls_%ls_write_op = %d\n",
                test_name, chrome_name, io_counters.WriteOperationCount);
        wprintf(L"%ls_%ls_other_op = %d\n",
                test_name, chrome_name, io_counters.OtherOperationCount);
        wprintf(L"%ls_%ls_read_byte = %d K\n",
                test_name, chrome_name, io_counters.ReadTransferCount / 1024);
        wprintf(L"%ls_%ls_write_byte = %d K\n",
                test_name, chrome_name, io_counters.WriteTransferCount / 1024);
        wprintf(L"%ls_%ls_other_byte = %d K\n",
                test_name, chrome_name, io_counters.OtherTransferCount / 1024);
      }
    }
  }

  void PrintMemoryUsageInfo(const wchar_t* test_name) {
    printf("\n");
    BrowserProcessFilter chrome_filter(user_data_dir_);
    process_util::NamedProcessIterator
        chrome_process_itr(chrome::kBrowserProcessExecutableName,
                           &chrome_filter);

    size_t browser_virtual_size = 0;
    size_t browser_working_set_size = 0;
    size_t virtual_size = 0;
    size_t working_set_size = 0;
    size_t num_chrome_processes = 0;
    const PROCESSENTRY32* chrome_entry;
    while (chrome_entry = chrome_process_itr.NextProcessEntry()) {
      uint32 pid = chrome_entry->th32ProcessID;
      size_t peak_virtual_size;
      size_t current_virtual_size;
      size_t peak_working_set_size;
      size_t current_working_set_size;
      if (GetMemoryInfo(pid, &peak_virtual_size, &current_virtual_size,
                        &peak_working_set_size, &current_working_set_size)) {
        if (pid == chrome_filter.browser_process_id()) {
          browser_virtual_size = current_virtual_size;
          browser_working_set_size = current_working_set_size;
        }
        virtual_size += current_virtual_size;
        working_set_size += current_working_set_size;
        num_chrome_processes++;
      }
    }
    wprintf(L"%ls_browser_vm_final_kb = %d\n", test_name,
                                           browser_virtual_size / 1024);
    wprintf(L"%ls_browser_ws_final_kb = %d\n", test_name,
                                           browser_working_set_size / 1024);
    wprintf(L"%ls_memory_vm_final_kb = %d\n", test_name,
                                          virtual_size / 1024);
    wprintf(L"%ls_memory_ws_final_kb = %d\n", test_name,
                                          working_set_size / 1024);
    wprintf(L"%ls_processes = %d\n", test_name, num_chrome_processes);
  }

 private:
  std::wstring user_data_dir_;
};

class MemoryReferenceTest : public MemoryTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    std::wstring dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    file_util::AppendToPath(&dir, L"reference_build");
    file_util::AppendToPath(&dir, L"chrome");
    browser_directory_ = dir;
    UITest::SetUp();
  }

  void RunTest(const wchar_t* test_name, int num_target_tabs) {
    std::wstring pages, timings;
    MemoryTest::RunTest(test_name, num_target_tabs);
  }
};

}  // namespace

TEST_F(MemoryTest, SingleTabTest) {
  RunTest(L"OneTabTest", 1);
}

TEST_F(MemoryTest, FiveTabTest) {
  RunTest(L"FiveTabTest", 5);
}

TEST_F(MemoryTest, TwelveTabTest) {
  RunTest(L"TwelveTabTest", 12);
}
