/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 G. Elian Gidoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MFMT_H_
#define MFMT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFMT_ASSERT(cond)                                               \
        do{                                                             \
                if (! (cond)){                                          \
                        while(1);                                       \
                }                                                       \
        } while(0)

int
mfmt_print(char *str, size_t len, const char *fmt,__builtin_va_list args) ;

int
mfmt_scan(const char *str, const char *fmt, ...) ;

#ifdef __cplusplus
}
#endif
#endif /* MFMT_H_ */
