//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <string.h>
#include <time.h>
#include "AutomaticVersionHeader.h"

#include "srm.h"
#include "PrintRoutine.h"

//
//////////////////////////////////////////////////////////////////////////////
static WCHAR szFillRootDirectory [ MAX_PATH ];
static WCHAR szFillDirectory [ MAX_PATH ];
static WCHAR szFillFilename [ MAX_PATH ];

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL FillNames ( WCHAR *pDrive, int count )
{
	//
	ZeroMemory ( szFillRootDirectory, sizeof(szFillRootDirectory) );
	wcscpy_s ( szFillRootDirectory, sizeof(szFillRootDirectory) / sizeof(WCHAR), pDrive );
	wcscat_s ( szFillRootDirectory, sizeof(szFillRootDirectory) / sizeof(WCHAR), L"\\TEMP" );

	BOOL bExist = PathFileExists ( szFillRootDirectory );
	if ( ! bExist )
	{
		BOOL bCreated = CreateDirectory ( szFillRootDirectory, NULL );
		if ( ! bCreated )
		{
			PrintStderrW ( L"Error - CreateDirectory Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText () );
			return FALSE;
		}
	}
	else
	{
		if ( ! isDirectory ( szFillRootDirectory ) )
		{
			PrintStderrW ( L"Error %s is not a Directory\n", szFillRootDirectory );
			return FALSE;
		}
	}

	//
	int iSubDir = 0;
	int iStep = 5000;
	for ( int i = 1; i < count; i = i + iStep )
	{
		if ( isAbortedByCtrlC() )
		{
			return FALSE;
		}

		iSubDir++;

		//
		WCHAR szSubDir [ 64 ];
		ZeroMemory ( szSubDir, sizeof(szSubDir) );
		wnsprintf ( szSubDir, sizeof(szSubDir) / sizeof(WCHAR), L"%04d", iSubDir );

		//
		ZeroMemory ( szFillDirectory, sizeof(szFillDirectory) );
		wcscpy_s ( szFillDirectory, sizeof(szFillDirectory) / sizeof(WCHAR), szFillRootDirectory );
		wcscat_s ( szFillDirectory, sizeof(szFillDirectory) / sizeof(WCHAR), L"\\" );
		wcscat_s ( szFillDirectory, sizeof(szFillDirectory) / sizeof(WCHAR), szSubDir );

		BOOL bExist = PathFileExists ( szFillDirectory );
		if ( ! bExist )
		{
			BOOL bCreated = CreateDirectory ( szFillDirectory, NULL );
			if ( ! bCreated )
			{
				PrintStderrW ( L"Error - CreateDirectory Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText () );
				return FALSE;
			}
		}
		else
		{
			if ( ! isDirectory ( szFillDirectory ) )
			{
				PrintStderrW ( L"Error %s is not a Directory\n", szFillDirectory );
				return FALSE;
			}
		}

		//
		int iFile = 0;
		for ( int j = i; j < i + iStep && j < count; j++ )
		{
			if ( isAbortedByCtrlC() )
			{
				return FALSE;
			}

			iFile++;

			//
			WCHAR szFile [ 64 ];
			ZeroMemory ( szFile, sizeof(szFile) );
			wnsprintf ( szFile, sizeof(szFile) / sizeof(WCHAR), L"fn.%010d.tmp", iFile );

			//
			ZeroMemory ( szFillFilename, sizeof(szFillFilename) );
			wcscpy_s ( szFillFilename, sizeof(szFillFilename) / sizeof(WCHAR), szFillDirectory );
			wcscat_s ( szFillFilename, sizeof(szFillFilename) / sizeof(WCHAR), L"\\" );
			wcscat_s ( szFillFilename, sizeof(szFillFilename) / sizeof(WCHAR), szFile );

			//
			PrintNormalW ( L"Creating file '%s'\n", szFillFilename );

			//
			HANDLE hFile = CreateFile ( 
				szFillFilename,						//	LPCTSTR lpFileName
				GENERIC_WRITE,							//	DWORD dwDesiredAccess
				0,										//	DWORD dwShareMode
				NULL,									//	LPSECURITY_ATTRIBUTES lpSecurityAttributes
				CREATE_NEW,								//	DWORD dwCreationDisposition
				FILE_ATTRIBUTE_NORMAL,					//	DWORD dwFlagsAndAttributes
				NULL );									//	HANDLE hTemplateFile
			if ( hFile == INVALID_HANDLE_VALUE )
			{
				PrintStderrW ( L"Error - CreateFile Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText () );
				return FALSE;
			}

			//
			//	Close File
			CloseHandle ( hFile );
		}
	}

	//
	return TRUE;
}