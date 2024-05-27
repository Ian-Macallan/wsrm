#pragma once

#ifdef _WIN64
#ifdef UNICODE
#define ORIGINAL_FILENAME   L"wsrm (x64 Unicode) (MSVC)\0"
#define PRODUCT_NAME        L"wsrm - Version 1.0.15.003\r\n(Build 187) - (x64 Unicode) (MSVC)\0"
#else
#define ORIGINAL_FILENAME   "wsrm (x64 MBCS) (MSVC)\0"
#define PRODUCT_NAME        "wsrm - Version 1.0.15.003\r\n(Build 187) - (x64 MBCS) (MSVC)\0"
#endif
#elif _WIN32
#ifdef UNICODE
#define ORIGINAL_FILENAME   L"wsrm (x86 Unicode) (MSVC)\0"
#define PRODUCT_NAME        L"wsrm - Version 1.0.15.003\r\n(Build 187) - (x86 Unicode) (MSVC)\0"
#else
#define ORIGINAL_FILENAME   "wsrm (x86 MBCS) (MSVC)\0"
#define PRODUCT_NAME        "wsrm - Version 1.0.15.003\r\n(Build 187) - (x86 MBCS) (MSVC)\0"
#endif
#else
#define ORIGINAL_FILENAME   "wsrm (MSVC)\0"
#define PRODUCT_NAME        "wsrm - Version 1.0.15.003\r\n(Build 187) - (MSVC)\0"
#endif
