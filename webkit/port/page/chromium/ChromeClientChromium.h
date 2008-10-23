// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChromeClientWin_h
#define ChromeClientWin_h

#include <wtf/Forward.h>

#include "ChromeClient.h"

namespace WebCore {

    class Cursor;
    class FileChooser;
    class Frame;
    class IntRect;
    class String;
    class Widget;
    
    class ChromeClientChromium : public ChromeClient {
    public:
        // Opens the file selection dialog.
        virtual void runFileChooser(const String& defaultFileName,
                                    PassRefPtr<FileChooser> file_chooser) = 0;

        // Notifies the client of a new popup widget.  The client should place
        // and size the widget with the given bounds, relative to the screen.
        virtual void popupOpened(Widget* widget, const IntRect& bounds) = 0;
    };
}

#endif // ChromeClientWin_h

