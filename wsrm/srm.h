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

//
class MCAutoWPtr
{
    public :
        WCHAR   *wptr;
        size_t  wlen;

    public :
        MCAutoWPtr ( )
        {
            wlen = 0;
            wptr = NULL;
        }

        MCAutoWPtr ( size_t len )
        {
            this->wlen  = len;
            wptr        = (WCHAR *) malloc ( ( len + 1 ) * sizeof(WCHAR) );
            memset ( wptr, 0, ( len + 1 ) * sizeof(WCHAR) );
        }

        ~MCAutoWPtr()
        {
            if ( wptr != NULL )
            {
                delete wptr;
            }
            wptr    = NULL;
            wlen    = 0;
        }
};

//
class MCAutoPtr
{
    public :
        char   *ptr;
        size_t  len;

    public :
        MCAutoPtr ( )
        {
            len = 0;
            ptr = NULL;
        }

        MCAutoPtr ( size_t len )
        {
            this->len   = len;
            ptr         = (char *) malloc ( len + 1 );
            memset ( ptr, 0, len + 1 );
        }

        ~MCAutoPtr()
        {
            if ( ptr != NULL )
            {
                delete ptr;
            }
            ptr    = NULL;
            len    = 0;
        }
};


#endif
