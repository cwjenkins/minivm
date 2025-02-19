#pragma once
// The MIT License (MIT)
//
// Copyright (c) 2013 Viktor Söderqvist
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// nanbox.h
// --------
//
// This file provides a is a way to store various types of data in a 64-bit
// slot, including a type tag, using NaN-boxing.  NaN-boxing is a way to store
// various information in unused NaN-space in the IEEE754 representation.  For
// 64-bit platforms, unused bits in pointers are also used to encode various
// information.  The representation in inspired by that used by Webkit's
// JavaScriptCore.
//
// Datatypes that can be stored:
//
//  // int (int32_t)
//  // double
//  // pointer
//  // boolean (true and false)
//  // null
//  // undefined
//  // empty
//  // deleted
//  // aux 'auxillary data' (5 types of 48-bit values)
//
// Any value with the top 13 bits set represents a quiet NaN.  The remaining
// bits are called the 'payload'. NaNs produced by hardware and C-library
// functions typically produce a payload of zero.  We assume that all quiet
// NaNs with a non-zero payload can be used to encode whatever we want.

#include "libc.h"

//
// Detect OS and endianess.
//
// Most of this is inspired by WTF/wtf/Platform.h in Webkit's source code.

// Unix?
#if defined(_AIX) || defined(__APPLE__) || defined(__FreeBSD__) ||             \
    defined(__DragonFly__) || defined(__FreeBSD_kernel__) ||                   \
    defined(__GNU__) || defined(__linux__) || defined(__NetBSD__) ||           \
    defined(__OpenBSD__) || defined(__QNXNTO__) || defined(sun) ||             \
    defined(__sun) || defined(unix) || defined(__unix) || defined(__unix__)
#define NANBOX_UNIX 1
#endif

// Windows?
#if defined(WIN32) || defined(_WIN32)
#define NANBOX_WINDOWS 1
#endif

// 64-bit mode? (Mostly equivallent to how WebKit does it)
#if ((defined(__x86_64__) || defined(_M_X64)) &&                               \
     (defined(NANBOX_UNIX) || defined(NANBOX_WINDOWS))) ||                     \
    (defined(__ia64__) && defined(__LP64__)) || defined(__alpha__) ||          \
    (defined(__sparc__) && defined(__arch64__) || defined(__sparcv9)) ||       \
    defined(__s390x__) || (defined(__ppc64__) || defined(__PPC64__)) ||        \
    defined(__aarch64__) // ARM 64-bit
#define NANBOX_64 1
#else
#define NANBOX_32 1
#endif

// Big endian? (Mostly equivallent to how WebKit does it)
#if defined(__MIPSEB__) || defined(__ppc__) || defined(__PPC__) ||             \
    defined(__powerpc__) || defined(__powerpc) || defined(__POWERPC__) ||      \
    defined(_M_PPC) || defined(__PPC) || defined(__ppc64__) ||                 \
    defined(__PPC64__) || defined(__sparc) || defined(__sparc__) ||            \
    defined(__s390x__) || defined(__s390__) || defined(__ARMEB__) ||           \
    ((defined(__CC_ARM) || defined(__ARMCC__)) && defined(__BIG_ENDIAN))
#define NANBOX_BIG_ENDIAN 1
#endif

//
// In 32-bit mode, the double is unmasked. In 64-bit mode, the pointer is
// unmasked.

union vm_nanbox_u {
  uint64_t as_int64;
#if defined(NANBOX_64)
  void *pointer;
#endif
  double as_double;
#if defined(NANBOX_BIG_ENDIAN)
  struct {
    uint32_t tag;
    uint32_t payload;
  } as_bits;
#else
  struct {
    uint32_t payload;
    uint32_t tag;
  } as_bits;
#endif
};

typedef union vm_nanbox_u vm_nanbox_t;

#if defined(NANBOX_64)

//
// 64-bit platforms
//
// This range of NaN space is represented by 64-bit numbers begining with
// 13 bits of ones. That is, the first 16 bits are 0xFFF8 or higher.  In
// practice, no higher value is used for NaNs.  We rely on the fact that no
// valid double-precision numbers will be "higher" than this (compared as an
// uint64).
//
// By adding 7// 2^48 as a 64-bit integer addition, we shift the first 16 bits
// in the doubles from the range 0000..FFF8 to the range 0007..FFFF.  Doubles
// are decoded by reversing this operation, i.e. substracting the same number.
//
// The top 16-bits denote the type of the encoded vm_nanbox_t:
//
//     Pointer {  0000:PPPP:PPPP:PPPP
//             /  0001:xxxx:xxxx:xxxx
//     Aux.   {           ...
//             \  0005:xxxx:xxxx:xxxx
//     Integer {  0006:0000:IIII:IIII
//              / 0007:****:****:****
//     Double  {          ...
//              \ FFFF:****:****:****
//
// 32-bit signed integers are marked with the 16-bit tag 0x0006.
//
// The tags 0x0001..0x0005 can be used to store five additional types of
// 48-bit auxillary data, each storing up to 48 bits of payload.
//
// The tag 0x0000 denotes a pointer, or another form of tagged immediate.
// Boolean, 'null', 'undefined' and 'deleted' are represented by specific,
// invalid pointer values:
//
//     False:     0x06
//     True:      0x07
//     Undefined: 0x0a
//     Null:      0x02
//     Empty:     0x00
//     Deleted:   0x05
//
// All of these except Empty have bit 0 or bit 1 set.

#define NANBOX_VALUE_EMPTY 0x0llu
#define NANBOX_VALUE_DELETED 0x5llu

// Booleans have bits 1 and 2 set. True also has bit 0 set.
#define NANBOX_VALUE_FALSE 0x06llu
#define NANBOX_VALUE_TRUE 0x07llu

// Null and undefined both have bit 1 set. Undefined also has bit 3 set.
#define NANBOX_VALUE_UNDEFINED 0x0Allu
#define NANBOX_VALUE_NULL 0x02llu

// This value is 7// 2^48, used to encode doubles such that the encoded value
// will begin with a 16-bit pattern within the range 0x0007..0xFFFF.
#define NANBOX_DOUBLE_ENCODE_OFFSET 0x0007000000000000llu
// If the 16 first bits are 0x0002, this indicates an integer number.  Any
// larger value is a double, so we can use >= to check for either integer or
// double.
#define NANBOX_MIN_NUMBER 0x0006000000000000llu
#define NANBOX_HIGH16_TAG 0xffff000000000000llu

// There are 5// 2^48 auxillary values can be stored in the 64-bit integer
// range NANBOX_MIN_AUX..NANBOX_MAX_AUX.
#define NANBOX_MIN_AUX_TAG 0x00010000
#define NANBOX_MAX_AUX_TAG 0x0005ffff
#define NANBOX_MIN_AUX 0x0001000000000000llu
#define NANBOX_MAX_AUX 0x0005ffffffffffffllu

// NANBOX_MASK_POINTER defines the allowed non-zero bits in a pointer.
#define NANBOX_MASK_POINTER 0x0000fffffffffffcllu

// The 'empty' value is guarranteed to consist of a repeated single byte,
// so that it should be easy to memset an array of nanboxes to 'empty' using
// NANBOX_EMPTY_BYTE as the value for every byte.
#define NANBOX_EMPTY_BYTE 0x0

// Define bool nanbox_is_xxx(vm_nanbox_t val) and vm_nanbox_t nanbox_xxx(void)
// with empty, deleted, true, false, undefined and null substituted for xxx.
#define NANBOX_IMMIDIATE_VALUE_FUNCTIONS(NAME, VALUE)                          \
  static inline vm_nanbox_t vm_nanbox_##NAME(void) {                           \
    vm_nanbox_t val;                                                           \
    val.as_int64 = VALUE;                                                      \
    return val;                                                                \
  }                                                                            \
  static inline bool vm_nanbox_is_##NAME(vm_nanbox_t val) {                    \
    return val.as_int64 == VALUE;                                              \
  }
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(empty, NANBOX_VALUE_EMPTY)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(deleted, NANBOX_VALUE_DELETED)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(false, NANBOX_VALUE_FALSE)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(true, NANBOX_VALUE_TRUE)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(undefined, NANBOX_VALUE_UNDEFINED)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(null, NANBOX_VALUE_NULL)

static inline bool vm_nanbox_is_undefined_or_null(vm_nanbox_t val) {
  // Undefined and null are the same if we remove the 'undefined' bit.
  return (val.as_int64 & ~8) == NANBOX_VALUE_NULL;
}

static inline bool vm_nanbox_is_boolean(vm_nanbox_t val) {
  // True and false are the same if we remove the 'true' bit.
  return (val.as_int64 & ~1) == NANBOX_VALUE_FALSE;
}
static inline bool vm_nanbox_to_boolean(vm_nanbox_t val) {
  return val.as_int64 & 1;
}
static inline vm_nanbox_t vm_nanbox_from_boolean(bool b) {
  vm_nanbox_t val;
  val.as_int64 = b ? NANBOX_VALUE_TRUE : NANBOX_VALUE_FALSE;
  return val;
}

// true if val is a double or an int
static inline bool vm_nanbox_is_number(vm_nanbox_t val) {
  return val.as_int64 >= NANBOX_MIN_NUMBER;
}

static inline bool vm_nanbox_is_int(vm_nanbox_t val) {
  return (val.as_int64 & NANBOX_HIGH16_TAG) == NANBOX_MIN_NUMBER;
}
static inline vm_nanbox_t vm_nanbox_from_int(int32_t i) {
  vm_nanbox_t val;
  val.as_int64 = NANBOX_MIN_NUMBER | (uint32_t)i;
  return val;
}
static inline int32_t vm_nanbox_to_int(vm_nanbox_t val) {
  return (int32_t)val.as_int64;
}

static inline bool vm_nanbox_is_double(vm_nanbox_t val) {
  return vm_nanbox_is_number(val) && !vm_nanbox_is_int(val);
}
static inline vm_nanbox_t vm_nanbox_from_double(double d) {
  vm_nanbox_t val;
  val.as_double = d;
  val.as_int64 += NANBOX_DOUBLE_ENCODE_OFFSET;
  return val;
}
static inline double vm_nanbox_to_double(vm_nanbox_t val) {
  val.as_int64 -= NANBOX_DOUBLE_ENCODE_OFFSET;
  return val.as_double;
}

static inline bool vm_nanbox_is_pointer(vm_nanbox_t val) {
  return !(val.as_int64 & ~NANBOX_MASK_POINTER) && val.as_int64;
}
static inline void *vm_nanbox_to_pointer(vm_nanbox_t val) {
  return val.pointer;
}
static inline vm_nanbox_t vm_nanbox_from_pointer(void *pointer) {
  vm_nanbox_t val;
  val.pointer = pointer;
  return val;
}

static inline bool vm_nanbox_is_aux(vm_nanbox_t val) {
  return val.as_int64 >= NANBOX_MIN_AUX && val.as_int64 <= NANBOX_MAX_AUX;
}

// end if NANBOX_64
#elif defined(NANBOX_32)

//
// On 32-bit platforms we use the following NaN-boxing scheme:
//
// For values that do not contain a double value, the high 32 bits contain the
// tag values listed below, which all correspond to NaN-space. When the tag is
// 'pointer', 'integer' and 'boolean', their values (the 'payload') are store
// in the lower 32 bits. In the case of all other tags the payload is 0.

#define NANBOX_MAX_DOUBLE_TAG 0xfff80000
#define NANBOX_INT_TAG 0xfff80001
#define NANBOX_MIN_AUX_TAG 0xfff90000
#define NANBOX_MAX_AUX_TAG 0xfffdffff
#define NANBOX_POINTER_TAG 0xfffffffa
#define NANBOX_BOOLEAN_TAG 0xfffffffb
#define NANBOX_UNDEFINED_TAG 0xfffffffc
#define NANBOX_NULL_TAG 0xfffffffd
#define NANBOX_DELETED_VALUE_TAG 0xfffffffe
#define NANBOX_EMPTY_VALUE_TAG 0xffffffff

// The 'empty' value is guarranteed to consist of a repeated single byte,
// so that it should be easy to memset an array of nanboxes to 'empty' using
// NANBOX_EMPTY_BYTE as the value for every byte.
#define NANBOX_EMPTY_BYTE 0xff

// The minimum uint64_t value for the auxillary range
#define NANBOX_MIN_AUX 0xfff9000000000000llu
#define NANBOX_MAX_AUX 0xfffdffffffffffffllu

// Define nanbox_xxx and nanbox_is_xxx for deleted, undefined and null.
#define NANBOX_IMMIDIATE_VALUE_FUNCTIONS(NAME, TAG)                            \
  static inline vm_nanbox_t vm_nanbox_##NAME(void) {                       \
    vm_nanbox_t val;                                                           \
    val.as_bits.tag = TAG;                                                     \
    val.as_bits.payload = 0;                                                   \
    return val;                                                                \
  }                                                                            \
  static inline bool vm_nanbox_is_##NAME(vm_nanbox_t val) {                \
    return val.as_bits.tag == TAG;                                             \
  }
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(deleted, NANBOX_DELETED_VALUE_TAG)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(undefined, NANBOX_UNDEFINED_TAG)
NANBOX_IMMIDIATE_VALUE_FUNCTIONS(null, NANBOX_NULL_TAG)

// The undefined and null tags differ only in one bit
static inline bool vm_nanbox_is_undefined_or_null(vm_nanbox_t val) {
  return (val.as_bits.tag & ~1) == NANBOX_UNDEFINED_TAG;
}

static inline vm_nanbox_t vm_nanbox_empty(void) {
  vm_nanbox_t val;
  val.as_int64 = 0xffffffffffffffffllu;
  return val;
}
static inline bool vm_nanbox_is_empty(vm_nanbox_t val) {
  return val.as_bits.tag == 0xffffffff;
}

// Returns true if the value is auxillary space
static inline bool vm_nanbox_is_aux(vm_nanbox_t val) {
  return val.as_bits.tag >= NANBOX_MIN_AUX_TAG &&
         val.as_bits.tag < NANBOX_POINTER_TAG;
}

// Define nanbox_is_yyy, nanbox_to_yyy and nanbox_from_yyy for
// boolean, int, pointer and aux1-aux5
#define NANBOX_TAGGED_VALUE_FUNCTIONS(NAME, TYPE, TAG)                         \
  static inline bool vm_nanbox_is_##NAME(vm_nanbox_t val) {                \
    return val.as_bits.tag == TAG;                                             \
  }                                                                            \
  static inline TYPE vm_nanbox_to_##NAME(vm_nanbox_t val) {                \
    return (TYPE)val.as_bits.payload;                                          \
  }                                                                            \
  static inline vm_nanbox_t vm_nanbox_from_##NAME(TYPE a) {                \
    vm_nanbox_t val;                                                           \
    val.as_bits.tag = TAG;                                                     \
    val.as_bits.payload = (int32_t)a;                                          \
    return val;                                                                \
  }

NANBOX_TAGGED_VALUE_FUNCTIONS(boolean, bool, NANBOX_BOOLEAN_TAG)
NANBOX_TAGGED_VALUE_FUNCTIONS(int, int32_t, NANBOX_INT_TAG)
NANBOX_TAGGED_VALUE_FUNCTIONS(pointer, void *, NANBOX_POINTER_TAG)

static inline vm_nanbox_t vm_nanbox_true(void) {
  return vm_nanbox_from_boolean(true);
}
static inline vm_nanbox_t vm_nanbox_false(void) {
  return vm_nanbox_from_boolean(false);
}
static inline bool vm_nanbox_is_true(vm_nanbox_t val) {
  return val.as_bits.tag == NANBOX_BOOLEAN_TAG && val.as_bits.payload;
}
static inline bool vm_nanbox_is_false(vm_nanbox_t val) {
  return val.as_bits.tag == NANBOX_BOOLEAN_TAG && !val.as_bits.payload;
}

static inline bool vm_nanbox_is_double(vm_nanbox_t val) {
  return val.as_bits.tag < NANBOX_INT_TAG;
}
// is number = is double or is int
static inline bool vm_nanbox_is_number(vm_nanbox_t val) {
  return val.as_bits.tag <= NANBOX_INT_TAG;
}

static inline vm_nanbox_t vm_nanbox_from_double(double d) {
  vm_nanbox_t val;
  val.as_double = d;
  return val;
}
static inline double vm_nanbox_to_double(vm_nanbox_t val) {
  return val.as_double;
}

#endif

//
// Representation-independent functions

static inline double vm_nanbox_to_number(vm_nanbox_t val) {
  return vm_nanbox_is_int(val) ? vm_nanbox_to_int(val)
                               : vm_nanbox_to_double(val);
}
