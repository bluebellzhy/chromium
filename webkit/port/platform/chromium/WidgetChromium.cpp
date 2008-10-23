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
#include "Widget.h"

#include "Assertions.h"
#include "Chrome.h"
#include "ChromeClientChromium.h"
#include "ScrollView.h"

namespace WebCore {

// This sucks
static ChromeClientChromium* chromeClientChromium(Widget* widget)
{
    ScrollView* root = widget->root();
    if (root) {
        HostWindow* host = root->hostWindow();
        if (host) {
            Chrome* chrome = static_cast<Chrome*>(host);
            return static_cast<ChromeClientChromium*>(chrome->client());
        }
    }
    return 0;
}

Widget::Widget(PlatformWidget widget)
{
    init(widget);
}

Widget::~Widget() 
{
    ASSERT(!parent());
}

void Widget::show()
{
}

void Widget::hide()
{
}

void Widget::setCursor(const Cursor& cursor)
{
    ChromeClientChromium* client = chromeClientChromium(this);
    if (client)
        client->setCursor(cursor);
}

void Widget::paint(GraphicsContext*, const IntRect&)
{
}

void Widget::setFocus()
{
    ASSERT_NOT_REACHED();
}

void Widget::setIsSelected(bool)
{
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
}

} // namespace WebCore
