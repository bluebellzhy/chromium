/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#if PLATFORM(WIN_OS)
#include <shlwapi.h>
#endif

#pragma warning(push, 0)
#include "ChromeClientChromium.h"
#include "Document.h"
#include "Frame.h"
#include "FileChooser.h"
#include "LocalizedStrings.h"
#include "NotImplemented.h"
#include "Page.h"
#include "StringTruncator.h"
#pragma warning(pop)

namespace WebCore {

void FileChooser::openFileChooser(Document* document)
{
    Frame* frame = document->frame();
    if (!frame)
        return;

    ChromeClientChromium* client =
        static_cast<ChromeClientChromium*>(frame->page()->chrome()->client());

    String result;
    client->runFileChooser(m_filename, &*this);
}

String FileChooser::basenameForWidth(const Font& font, int width) const
{
    if (width <= 0)
        return String();

    String string;
    if (m_filename.isEmpty())
        string = fileButtonNoFileSelectedLabel();
    else {
#if PLATFORM(WIN_OS)
        String tmpFilename = m_filename;
        // Apple's code has a LPTSTR here, which will compile and run, but is wrong.
        wchar_t* basename = PathFindFileName(tmpFilename.charactersWithNullTermination());
        string = String(basename);
#else
        notImplemented();
        string = "fixme";
#endif
    }

    return StringTruncator::centerTruncate(string, static_cast<float>(width), font, false);
}

}
