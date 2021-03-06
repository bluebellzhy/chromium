// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CLIPBOARD_H_
#define BASE_CLIPBOARD_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "base/shared_memory.h"

#if defined(OS_MACOSX)
#if defined(__OBJC__)
@class NSString;
#else
class NSString;
#endif
#endif

class Clipboard {
 public:
#if defined(OS_WIN)
  typedef unsigned int FormatType;
#elif defined(OS_MACOSX)
  typedef NSString *FormatType;
#elif defined(OS_LINUX)
  typedef struct _GdkAtom* FormatType;
  typedef struct _GtkClipboard GtkClipboard;
  // A mapping from target (format) string to the relevant data (usually a
  // string, but potentially arbitrary data).
  typedef std::map<std::string, std::pair<uint8*, size_t> > TargetMap;
#endif

  Clipboard();
  ~Clipboard();

  // Clears the clipboard.  It is usually a good idea to clear the clipboard
  // before writing content to the clipboard.
  void Clear();

  // Adds UNICODE and ASCII text to the clipboard.
  void WriteText(const std::wstring& text);

  // Adds HTML to the clipboard.  The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links
  void WriteHTML(const std::wstring& markup, const std::string& src_url);

  // Adds a bookmark to the clipboard
  void WriteBookmark(const std::wstring& title, const std::string& url);

  // Adds both a bookmark and an HTML hyperlink to the clipboard.  It is a
  // convenience wrapper around WriteBookmark and WriteHTML.
  void WriteHyperlink(const std::wstring& title, const std::string& url);

#if defined(OS_WIN)
  // Adds a bitmap to the clipboard
  // This is the slowest way to copy a bitmap to the clipboard as we must first
  // memcpy the pixels into GDI and the blit the bitmap to the clipboard.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmap(const void* pixels, const gfx::Size& size);

  // Adds a bitmap to the clipboard
  // This function requires read and write access to the bitmap, but does not
  // actually modify the shared memory region.
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmapFromSharedMemory(const SharedMemory& bitmap,
                                   const gfx::Size& size);

  // Adds a bitmap to the clipboard
  // This is the fastest way to copy a bitmap to the clipboard.  The HBITMAP
  // may either be device-dependent or device-independent.
  void WriteBitmapFromHandle(HBITMAP hbitmap, const gfx::Size& size);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();
#endif

  // Adds a file or group of files to the clipboard.
  void WriteFile(const std::wstring& file);
  void WriteFiles(const std::vector<std::wstring>& files);

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(FormatType format) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(std::wstring* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(std::string* result) const;

  // Reads HTML from the clipboard, if available.
  void ReadHTML(std::wstring* markup, std::string* src_url) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(std::wstring* title, std::string* url) const;

  // Reads a file or group of files from the clipboard, if available, into the
  // out parameter.
  void ReadFile(std::wstring* file) const;
  void ReadFiles(std::vector<std::wstring>* files) const;

  // Get format Identifiers for various types.
  static FormatType GetUrlFormatType();
  static FormatType GetUrlWFormatType();
  static FormatType GetMozUrlFormatType();
  static FormatType GetPlainTextFormatType();
  static FormatType GetPlainTextWFormatType();
  static FormatType GetFilenameFormatType();
  static FormatType GetFilenameWFormatType();
  // Win: MS HTML Format, Other: Generic HTML format
  static FormatType GetHtmlFormatType();
#if defined(OS_WIN)
  static FormatType GetBitmapFormatType();
  // Firefox text/html
  static FormatType GetTextHtmlFormatType();
  static FormatType GetCFHDropFormatType();
  static FormatType GetFileDescriptorFormatType();
  static FormatType GetFileContentFormatZeroType();
  static FormatType GetWebKitSmartPasteFormatType();
#endif

 private:
#if defined(OS_WIN)
  static void MarkupToHTMLClipboardFormat(const std::wstring& markup,
                                          const std::string& src_url,
                                          std::string* html_fragment);

  static void ParseHTMLClipboardFormat(const std::string& html_fragment,
                                       std::wstring* markup,
                                       std::string* src_url);

  static void ParseBookmarkClipboardFormat(const std::wstring& bookmark,
                                           std::wstring* title,
                                           std::string* url);

  HWND clipboard_owner_;
#elif defined(OS_LINUX)
  // Write changes to gtk clipboard.
  void SetGtkClipboard();
  // Free pointers in clipboard_data_ and clear() the map.
  void FreeTargetMap();
  // Insert a mapping into clipboard_data_, or change an existing mapping.
  uint8* InsertOrOverwrite(std::string target, uint8* data, size_t data_len);

  // We have to be able to store multiple formats on the clipboard
  // simultaneously. The Chrome Clipboard class accomplishes this by
  // expecting that consecutive calls to Write* (WriteHTML, WriteText, etc.)
  // The GTK clipboard interface does not naturally support consecutive calls
  // building on one another. So we keep all data in a map, and always pass the
  // map when we are setting the gtk clipboard. Consecutive calls to Write*
  // will write to the map and set the GTK clipboard, then add to the same
  // map and set the GTK clipboard again. GTK thinks it is wiping out the
  // clipboard but we are actually keeping the previous data.
  TargetMap clipboard_data_;
  GtkClipboard* clipboard_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(Clipboard);
};

#endif  // BASE_CLIPBOARD_H_

