/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// Modified from Apple's version to not directly call any windows methods as
// they may not be available to us in the multiprocess 

#include "config.h"
#include "DragData.h"

#include "ClipboardWin.h"
#include "ClipboardUtilitiesWin.h"
#include "DocumentFragment.h"
#include "KURL.h"
#include "PlatformString.h"
#include "Markup.h"
#include "WCDataObject.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "net/base/base64.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"

namespace {

bool containsHTML(const WebDropData& drop_data) {
    std::wstring html;
    return drop_data.cf_html.length() > 0
        || drop_data.text_html.length() > 0;
}

// Our DragDataRef is actually a WebDropData* instead of a IDataObject*. 
// Provide a helper method for converting back.
WebDropData* dropData(DragDataRef dragData) {
    return reinterpret_cast<WebDropData*>(dragData);
}

}

namespace WebCore {

PassRefPtr<Clipboard> DragData::createClipboard(ClipboardAccessPolicy policy) const
{
    WCDataObject* data;
    WCDataObject::createInstance(&data);
    RefPtr<ClipboardWin> clipboard = ClipboardWin::create(true, data, policy);
    // The clipboard keeps a reference to the WCDataObject, so we can release
    // our reference to it.
    data->Release();

    return clipboard.release();
}

bool DragData::containsURL() const
{
    return dropData(m_platformDragData)->url.is_valid();
}

String DragData::asURL(String* title) const
{
    WebDropData* data = dropData(m_platformDragData);
    if (!data->url.is_valid())
        return String();
 
    // |title| can be NULL
    if (title)
        *title = webkit_glue::StdWStringToString(data->url_title);
    return webkit_glue::StdStringToString(data->url.spec());
}

bool DragData::containsFiles() const
{
    return !dropData(m_platformDragData)->filenames.empty();
}

void DragData::asFilenames(Vector<String>& result) const
{
    WebDropData* data = dropData(m_platformDragData);
    for (size_t i = 0; i < data->filenames.size(); ++i) {
        result.append(webkit_glue::StdWStringToString(data->filenames[i]));
    }
}

bool DragData::containsPlainText() const
{
    return !dropData(m_platformDragData)->plain_text.empty();
}

String DragData::asPlainText() const
{
    return webkit_glue::StdWStringToString(dropData(
        m_platformDragData)->plain_text);
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::canSmartReplace() const
{
    // Mimic the situations in which mac allows drag&drop to do a smart replace.
    // This is allowed whenever the drag data contains a 'range' (ie.,
    // ClipboardWin::writeRange is called).  For example, dragging a link
    // should not result in a space being added.
    WebDropData* data = dropData(m_platformDragData);
    return !data->cf_html.empty() && !data->plain_text.empty() &&
        !data->url.is_valid();
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText() || containsURL()
        || ::containsHTML(*dropData(m_platformDragData))
        || containsColor();
}

PassRefPtr<DocumentFragment> DragData::asFragment(Document* doc) const
{     
    /*
     * Order is richest format first. On OSX this is:
     * * Web Archive
     * * Filenames
     * * HTML
     * * RTF
     * * TIFF
     * * PICT
     */

     // TODO(tc): Disabled because containsFilenames is hardcoded to return
     // false.  We need to implement fragmentFromFilenames when this is
     // re-enabled in Apple's win port.
     //if (containsFilenames())
     //    if (PassRefPtr<DocumentFragment> fragment = fragmentFromFilenames(doc, m_platformDragData))
     //        return fragment;

     WebDropData* data = dropData(m_platformDragData);
     if (!data->cf_html.empty()) {
         RefPtr<DocumentFragment> fragment = fragmentFromCF_HTML(doc,
             webkit_glue::StdWStringToString(data->cf_html));
         return fragment;
     }

     if (!data->text_html.empty()) {
         String url;
         RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(doc,
             webkit_glue::StdWStringToString(data->text_html), url);
         return fragment;
     }

     return 0;
}

Color DragData::asColor() const
{
    return Color();
}

}
