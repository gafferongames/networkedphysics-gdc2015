/*
    Network Protocol Library.
    Copyright (c) 2014 The Network Protocol Company, Inc.
*/

#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

//#define PROTOCOL_DEBUG_MEMORY_LEAKS 1

#if PROTOCOL_DEBUG_MEMORY_LEAKS
#include <map>
#include <algorithm>
#endif

#define PROTOCOL_USE_RESOLVER 0
#define PROTOCOL_USE_SCRATCH_ALLOCATOR 1

#define PROTOCOL_PLATFORM_WINDOWS  1
#define PROTOCOL_PLATFORM_MAC      2
#define PROTOCOL_PLATFORM_UNIX     3

#if defined(_WIN32)
#define PROTOCOL_PLATFORM PROTOCOL_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PROTOCOL_PLATFORM PROTOCOL_PLATFORM_MAC
#else
#define PROTOCOL_PLATFORM PROTOCOL_PLATFORM_UNIX
#endif

#define PROTOCOL_LITTLE_ENDIAN 1
#define PROTOCOL_BIG_ENDIAN 2

#if    defined(__386__) || defined(i386)    || defined(__i386__)  \
    || defined(__X86)   || defined(_M_IX86)                       \
    || defined(_M_X64)  || defined(__x86_64__)                    \
    || defined(alpha)   || defined(__alpha) || defined(__alpha__) \
    || defined(_M_ALPHA)                                          \
    || defined(ARM)     || defined(_ARM)    || defined(__arm__)   \
    || defined(WIN32)   || defined(_WIN32)  || defined(__WIN32__) \
    || defined(_WIN32_WCE) || defined(__NT__)                     \
    || defined(__MIPSEL__)
  #define PROTOCOL_ENDIAN PROTOCOL_LITTLE_ENDIAN
#else
  #define PROTOCOL_ENDIAN PROTOCOL_BIG_ENDIAN
#endif

#endif