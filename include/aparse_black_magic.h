/*
MIT License

Copyright (c) 2025 binaryfox0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef APARSE_BLACK_MAGIC_H
#define APARSE_BLACK_MAGIC_H

// Token pasting helper
#define __cat(a, b) __cat_impl(a, b)
#define __cat_impl(a, b) a##b

// Count number of arguments passed to a variadic macro (supports up to 16)
#define __count_args(...) __count_args_impl(__VA_ARGS__, \
     16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
#define __count_args_impl( \
     _1, _2, _3, _4, _5, _6, _7, _8, \
     _9, _10,_11,_12,_13,_14,_15,_16,N,...) N

// Macro expansion helpers
#define __expand(...) __VA_ARGS__

// Mapper macros for variadic input expansion
#define __map_1(m, a, x)               m(a, x)
#define __map_2(m, a, x, ...)          m(a, x), __map_1(m, a, __VA_ARGS__)
#define __map_3(m, a, x, ...)          m(a, x), __map_2(m, a, __VA_ARGS__)
#define __map_4(m, a, x, ...)          m(a, x), __map_3(m, a, __VA_ARGS__)
#define __map_5(m, a, x, ...)          m(a, x), __map_4(m, a, __VA_ARGS__)
#define __map_6(m, a, x, ...)          m(a, x), __map_5(m, a, __VA_ARGS__)
#define __map_7(m, a, x, ...)          m(a, x), __map_6(m, a, __VA_ARGS__)
#define __map_8(m, a, x, ...)          m(a, x), __map_7(m, a, __VA_ARGS__)
#define __map_9(m, a, x, ...)          m(a, x), __map_8(m, a, __VA_ARGS__)
#define __map_10(m, a, x, ...)         m(a, x), __map_9(m, a, __VA_ARGS__)
#define __map_11(m, a, x, ...)         m(a, x), __map_10(m, a, __VA_ARGS__)
#define __map_12(m, a, x, ...)         m(a, x), __map_11(m, a, __VA_ARGS__)
#define __map_13(m, a, x, ...)         m(a, x), __map_12(m, a, __VA_ARGS__)
#define __map_14(m, a, x, ...)         m(a, x), __map_13(m, a, __VA_ARGS__)
#define __map_15(m, a, x, ...)         m(a, x), __map_14(m, a, __VA_ARGS__)
#define __map_16(m, a, x, ...)         m(a, x), __map_15(m, a, __VA_ARGS__)
#define __map(m, a, ...) __cat(__map_, __count_args(__VA_ARGS__))(m, a, __VA_ARGS__)

#endif