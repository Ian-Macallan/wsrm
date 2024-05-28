#ifndef SRM_H
#define SRM_H

#include "stdafx.h"

#include <Windows.h>

extern WCHAR *GetLastErrorText ();
extern bool isAbortedByCtrlC();
extern BOOL endsWith ( WCHAR *pString, WCHAR c );
extern BOOL isDirectory ( WCHAR *lpFileName );

//
extern int XEMain ( WCHAR cDrive, BOOL bFindDelete, BOOL bDump, int index, WCHAR *filename );
extern BOOL FillNames ( WCHAR *pDrive, int count );

//
//  Some Macros
#define _wsizeof(x)         (sizeof(x)/sizeof(WCHAR))

#endif
