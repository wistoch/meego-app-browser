/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "TextBoundaries.h"

#include <unicode/ubrk.h>

#include "StringImpl.h"
#include "TextBreakIterator.h"

namespace WebCore {

int findNextWordFromIndex(const UChar* chars, int len, int position, bool forward)
{
    UBreakIterator* it = wordBreakIterator(chars, len);

    if (forward) {
        position = ubrk_following(it, position);
        while (position != UBRK_DONE) {
            // We stop searching when the character preceeding the break
            // is alphanumeric.
            if (position < len && u_isalnum(chars[position - 1]))
                return position;

            position = ubrk_following(it, position);
        }

        return len;
    } else {
        position = ubrk_preceding(it, position);
        while (position != UBRK_DONE) {
            // We stop searching when the character following the break
            // is alphanumeric.
            if (position > 0 && u_isalnum(chars[position]))
                return position;

            position = ubrk_preceding(it, position);
        }

        return 0;
    }
}

void findWordBoundary(const UChar* chars, int len, int position, int* start, int* end)
{
    UBreakIterator* it = wordBreakIterator(chars, len);
    *end = ubrk_following(it, position);
    if (*end < 0)
        *end = ubrk_last(it);
    *start = ubrk_previous(it);
}

} // namespace WebCore
