/*
 ************************************************************************************
 *
 *		File:			ExceptionErrorHandler.cpp
 *		Class:			CExceptErrorHandler
 *
 ************************************************************************************
 *		Description:
 *
 ************************************************************************************
 *		Notes:
 *
 ************************************************************************************
 *
 *		E/Mail: ian.macallan@gmail.com
 *		(c) 2003 - 2014 / Macallan 
 *		
 ************************************************************************************
 *
 *		Matt Pietrek
 *		MSDN Magazine, 2002
 *
 *		Litely adapted by Macallan.
 *		FILE: MacallanExceptionHandler.CPP
 *
 ************************************************************************************
 */
#include "stdafx.h"

#ifndef	WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		// Exclude Windows Headers rarely used
#endif

#ifndef _WIN32_WINNT
#ifdef _M_X64
#define _WIN32_WINNT 0x0502		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#else
#define _WIN32_WINNT 0x0501		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif
#endif

#include <windows.h>
#include <WinBase.h>
#include <WinNT.h>

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include <strsafe.h>
#include <time.h>
#include <locale.h>
#include <share.h>

#define	EXCEPTION_ERROR_HANDLER	1
#include "ExceptionErrorHandler.h"

#include "PrintRoutine.h"
#include "strstr.h"

//
#ifndef	_wsizeof
#define	_wsizeof(x)	(sizeof(x)/sizeof(WCHAR))
#endif

#pragma warning( disable : 4311 )
#pragma warning( disable : 4312 )

// #pragma comment(linker, "/defaultlib:dbghelp.lib")

//
//====================================================================================
//
//		Global Variables
//
//====================================================================================
static CExceptErrorHandler		*handler	= NULL;

//
typeSymInitialize				CExceptErrorHandler::pfSymInitialize			= NULL;
typeSymGetTypeInfo				CExceptErrorHandler::pfSymGetTypeInfo			= NULL;
typeSymSetOptions				CExceptErrorHandler::pfSymSetOptions			= NULL;
typeSymGetOptions				CExceptErrorHandler::pfSymGetOptions			= NULL;
typeSymCleanup					CExceptErrorHandler::pfSymCleanup				= NULL;
typeSymEnumSymbols				CExceptErrorHandler::pfSymEnumSymbols			= NULL;
typeSymSetContext				CExceptErrorHandler::pfSymSetContext			= NULL;
typeSymFromAddr					CExceptErrorHandler::pfSymFromAddr				= NULL;
typeSymGetLineFromAddr			CExceptErrorHandler::pfSymGetLineFromAddr		= NULL;
typeSymGetLineFromAddr64		CExceptErrorHandler::pfSymGetLineFromAddr64		= NULL;
typeStackWalk					CExceptErrorHandler::pfStackWalk				= NULL;
typeStackWalk64					CExceptErrorHandler::pfStackWalk64				= NULL;
typeSymFunctionTableAccess		CExceptErrorHandler::pfSymFunctionTableAccess	= NULL;
typeSymFunctionTableAccess64	CExceptErrorHandler::pfSymFunctionTableAccess64	= NULL;
typeSymGetModuleBase			CExceptErrorHandler::pfSymGetModuleBase			= NULL;
typeSymGetModuleBase64			CExceptErrorHandler::pfSymGetModuleBase64		= NULL;

typeRtlCaptureStackBackTrace	CExceptErrorHandler::pfRtlCaptureStackBackTrace	= NULL;

//
//====================================================================================
//
//		Declare the static variables of the CExceptErrorHandler class
//
//====================================================================================
char	CExceptErrorHandler::m_szDebugFileName [ MAX_PATH + 1 ]											= "";
char	CExceptErrorHandler::m_szCrashFileName [ MAX_PATH + 1 ]											= "";
char	CExceptErrorHandler::m_szDirectory [ MAX_PATH + 1 ]												= "";
char	CExceptErrorHandler::m_szSearchPath [ MAX_PATH + 1 ]											= "";
char	CExceptErrorHandler::m_szFilename [ MAX_PATH + 1 ]												= "";
char	CExceptErrorHandler::m_szModule [ MAX_PATH + 1 ]												= "";
char	CExceptErrorHandler::m_szFaultingModule [ MAX_PATH + 1 ]										= "";
char	CExceptErrorHandler::m_szBuffer [ MAX_PATH*4 + 1 ]												= "";
WCHAR	CExceptErrorHandler::m_szwBuffer [ MAX_PATH*4 + 1 ]												= L"";
char	CExceptErrorHandler::m_szSymbolBuffer [ MAX_PATH*8 + 1 ]										= "";
char	CExceptErrorHandler::m_szErrorText [ MAX_PATH*2 + 1 ]											= "";

char	CExceptErrorHandler::m_SymbolInfo[ sizeof(SYMBOL_INFO) + SYMBOL_NAME_LEN * sizeof( char )]		= "";
SYMBOL_INFO	*CExceptErrorHandler::m_pSymbolInfo															= NULL;

HANDLE	CExceptErrorHandler::m_hCrashFile																= NULL;
HANDLE	CExceptErrorHandler::m_hDebugFile																= NULL;
HANDLE	CExceptErrorHandler::m_hProcess																	= NULL;

_invalid_parameter_handler CExceptErrorHandler::m__invalid_parameter_handler							= NULL;
LPTOP_LEVEL_EXCEPTION_FILTER CExceptErrorHandler::m_previousFilter										= NULL;


//
//	Crash to Stderr
static	bool	EEHCrashToStderr = false;

//
//====================================================================================
//
//====================================================================================
static void StaticInvalidParameterHandler ( const wchar_t *expression, const wchar_t *function, 
										const wchar_t *file, unsigned int line, uintptr_t pReserved )
{
	CExceptErrorHandler::EEHInvalidParameterHandler( expression, function, file, line, pReserved );
}

//
//====================================================================================
//
//		Class Methods
//
//====================================================================================
CExceptErrorHandler::CExceptErrorHandler ( )   // Constructor
{
    //		Install the unhandled exception filter function
    m_previousFilter = NULL;
	
	m_pSymbolInfo = (SYMBOL_INFO *)( & m_SymbolInfo );

	//
	//	Get Process
    m_hProcess = GetCurrentProcess ( );

	//	Use TEMP Directory
	ZeroMemory ( m_szDirectory, sizeof(m_szDirectory) );
	size_t	iEnv	= sizeof ( m_szDirectory );

	getenv_s ( &iEnv, m_szDirectory, sizeof ( m_szDirectory ), "TEMP" );
	if ( *m_szDirectory == 0 )
	{
		ZeroMemory ( m_szDirectory, sizeof(m_szDirectory) );
		EEHGetDirectory ( m_szDirectory, sizeof(m_szDirectory) );
	}

	//		Figure out what the trace file will be named, and store it away
	ZeroMemory ( m_szDebugFileName, sizeof(m_szDebugFileName) );
	strcat_s ( m_szDebugFileName, sizeof ( m_szDebugFileName ), m_szDirectory );
	strcat_s ( m_szDebugFileName, sizeof ( m_szDebugFileName ), "\\" );
	ZeroMemory ( m_szFilename, sizeof(m_szFilename) );
    GetModuleFileNameA ( NULL, m_szFilename, sizeof ( m_szFilename ) - 1 );
	char *pName = EEHGetFilename ( m_szFilename );
	strcat_s ( m_szDebugFileName, sizeof ( m_szDebugFileName ), pName );
    strcat_s ( m_szDebugFileName, sizeof ( m_szDebugFileName ), ".log" );   // ".log" -> "Error Report"

	EEHOpenDebugFile ();

	CExceptErrorHandler::EEHPrintDebug ( "INFO: Current Process Handle : %d (-1 is Current Process)\r\n", m_hProcess );

	//
	EEHSetInvalidParameterHandler();

	//	Now the Work to do

    //	Figure out what the report file will be named, and store it away
	ZeroMemory ( m_szCrashFileName, sizeof(m_szCrashFileName) );
    GetModuleFileNameA ( NULL, m_szCrashFileName, sizeof ( m_szCrashFileName ) - 1 );

	time_t	theClock = time ( NULL );

	struct tm	tmLocal		= {};
	localtime_s ( &tmLocal, &theClock );

	ZeroMemory ( m_szFilename, sizeof(m_szFilename) );
	pName = EEHGetFilename ( m_szCrashFileName );
	strcpy_s ( m_szFilename, sizeof(m_szFilename), pName );


	ZeroMemory ( m_szCrashFileName, sizeof(m_szCrashFileName) );
	sprintf_s ( m_szCrashFileName, sizeof ( m_szCrashFileName ) - 1,
		"%s\\%04d%02d%02d_%02d%02d_%s",
		m_szDirectory,
		tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday,
		tmLocal.tm_hour, tmLocal.tm_min, 
		m_szFilename );

    //		Look for the '.' before the "EXE" extension.  Replace the extension
    //		with "rpt"
    char *pszDot = strchr ( m_szCrashFileName, '.' );
    if ( pszDot )
    {
        pszDot++;   // Advance past the '.'
        if ( strlen ( pszDot ) >= 3 )
		{
            strcpy_s ( pszDot, 4, "err" );   // "err" -> "Error Report"
		}
    }

	EEHPrintDebug ( "INFO: Error Dump will be done in %s\r\n", m_szCrashFileName );

	//
	//		Load Function Pointers
	HMODULE hDbgHelp = LoadLibraryA ( "dbghelp.dll" );
	if ( hDbgHelp != NULL )
	{
		pfSymGetTypeInfo = ( typeSymGetTypeInfo ) GetProcAddress ( hDbgHelp, "SymGetTypeInfo" );
		if ( pfSymGetTypeInfo == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetTypeInfo not found\r\n" );
		}

		pfSymSetOptions = ( typeSymSetOptions ) GetProcAddress ( hDbgHelp, "SymSetOptions" );
		if ( pfSymSetOptions == NULL )
		{
			EEHPrintDebug ( "ERROR: SymSetOptions not found\r\n" );
		}

		pfSymGetOptions = ( typeSymGetOptions ) GetProcAddress ( hDbgHelp, "SymGetOptions" );
		if ( pfSymGetOptions == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetOptions not found\r\n" );
		}

		pfSymInitialize = ( typeSymInitialize ) GetProcAddress ( hDbgHelp, "SymInitialize" );
		if ( pfSymInitialize == NULL )
		{
			EEHPrintDebug ( "ERROR: SymInitialize not found\r\n" );
		}

		pfSymCleanup = ( typeSymCleanup ) GetProcAddress ( hDbgHelp, "SymCleanup" );
		if ( pfSymCleanup == NULL )
		{
			EEHPrintDebug ( "ERROR: SymCleanup not found\r\n" );
		}

		pfSymEnumSymbols = ( typeSymEnumSymbols ) GetProcAddress ( hDbgHelp, "SymEnumSymbols" );
		if ( pfSymEnumSymbols == NULL )
		{
			EEHPrintDebug ( "ERROR: SymEnumSymbols not found\r\n" );
		}

		pfSymSetContext = ( typeSymSetContext ) GetProcAddress ( hDbgHelp, "SymSetContext" );
		if ( pfSymSetContext == NULL )
		{
			EEHPrintDebug ( "ERROR: SymSetContext not found\r\n" );
		}

		pfSymFromAddr = ( typeSymFromAddr ) GetProcAddress ( hDbgHelp, "SymFromAddr" );
		if ( pfSymFromAddr  == NULL )
		{
			EEHPrintDebug ( "ERROR: SymFromAddr not found\r\n" );
		}

		pfSymGetLineFromAddr = ( typeSymGetLineFromAddr ) GetProcAddress ( hDbgHelp, "SymGetLineFromAddr" );
		if ( pfSymGetLineFromAddr  == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetLineFromAddr not found\r\n" );
		}

		pfSymGetLineFromAddr64 = ( typeSymGetLineFromAddr64 ) GetProcAddress ( hDbgHelp, "SymGetLineFromAddr64" );
		if ( pfSymGetLineFromAddr64  == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetLineFromAddr64 not found\r\n" );
		}

		pfStackWalk = ( typeStackWalk ) GetProcAddress ( hDbgHelp, "StackWalk" );
		if ( pfStackWalk  == NULL )
		{
			EEHPrintDebug ( "ERROR: StackWalk not found\r\n" );
		}

		pfStackWalk64 = ( typeStackWalk64 ) GetProcAddress ( hDbgHelp, "StackWalk64" );
		if ( pfStackWalk64  == NULL )
		{
			EEHPrintDebug ( "ERROR: StackWalk64 not found\r\n" );
		}

		pfSymFunctionTableAccess = ( typeSymFunctionTableAccess ) GetProcAddress ( hDbgHelp, "SymFunctionTableAccess" );
		if ( pfSymFunctionTableAccess  == NULL )
		{
			EEHPrintDebug ( "ERROR: SymFunctionTableAccess not found\r\n" );
		}

		pfSymFunctionTableAccess64 = ( typeSymFunctionTableAccess64 ) GetProcAddress ( hDbgHelp, "SymFunctionTableAccess64" );
		if ( pfSymFunctionTableAccess64  == NULL )
		{
			EEHPrintDebug ( "ERROR: SymFunctionTableAccess64 not found\r\n" );
		}

		pfSymGetModuleBase = ( typeSymGetModuleBase ) GetProcAddress ( hDbgHelp, "SymGetModuleBase" );
		if ( pfSymGetModuleBase == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetModuleBase not found\r\n" );
		}

		pfSymGetModuleBase64 = ( typeSymGetModuleBase64 ) GetProcAddress ( hDbgHelp, "SymGetModuleBase64" );
		if ( pfSymGetModuleBase64 == NULL )
		{
			EEHPrintDebug ( "ERROR: SymGetModuleBase64 not found\r\n" );
		}
	}
	else
	{
		EEHPrintDebug ( "ERROR: LoadLibrary DbgHelp.dll not found\r\n" );
	}

	//
	HMODULE hKernel32 = LoadLibraryA ( "kernel32.dll" );
	if ( hKernel32 != NULL )
	{
		pfRtlCaptureStackBackTrace = ( typeRtlCaptureStackBackTrace ) GetProcAddress ( hKernel32, "RtlCaptureStackBackTrace" );
		if ( pfRtlCaptureStackBackTrace == NULL )
		{
			EEHPrintDebug ( "ERROR: RtlCaptureStackBackTrace not found\r\n" );
		}
	}
	else
	{
		EEHPrintDebug ( "ERROR: LoadLibrary kernel32.dll not found\r\n" );
	}

	//	Enable Crash Dump
	BOOL bResult = EEHEnableCrashingOnCrashes();
	if ( ! bResult )
	{
		EEHPrintDebug ( "ERROR: enableCrashingOnCrashes fails\r\n" );
	}

	//
	EEHInitializeSymbols ();

	//
    m_previousFilter = SetUnhandledExceptionFilter ( (LPTOP_LEVEL_EXCEPTION_FILTER ) EEHMacallanUnhandledExceptionFilter );
	EEHPrintDebug ( "INFO: SetUnhandledExceptionFilter done\r\n" );

	EEHCloseDebugFile ();

}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHInvalidParameterHandler (	const wchar_t * expression, const wchar_t * function, 
														const wchar_t * file, unsigned int line, uintptr_t pReserved )
{
	//
	//		Write Stack Trace
	EEHOpenCrashFile ();
    if ( m_hCrashFile != NULL )
    {
		EEHWriteStackTrace ();

		EEHCloseCrashFile();
	}


	//
	//		Append report for Parameter
	FILE *hFile = NULL;
	hFile = _fsopen ( m_szCrashFileName, "a", _SH_DENYWR );
    if ( hFile != NULL )
    {
		fwprintf ( hFile, L"\n");
		fwprintf ( hFile, L"**** Report Invalid Parameter\n");
		fwprintf ( hFile, L"Invalid parameter detected\n" );

		//
		if ( function != NULL )
		{
			fwprintf ( hFile, L"Function %s\n", function );
		}

		//
		if ( file != NULL )
		{
			fwprintf ( hFile, L"File: %s Line: %d\n", file, line);
		}

		//
		if ( expression != NULL )
		{
			fwprintf ( hFile, L"Expression: %s\n", expression);
		}

		int errnum = errno;
		_wcserror_s ( m_szwBuffer, sizeof(m_szwBuffer) / sizeof(WCHAR), errnum );

		fwprintf ( hFile, L"Error : %d - %s\n", errnum, m_szwBuffer );

		fclose ( hFile );
	}

	//
	abort();

}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHOpenCrashFile()
{
	//
	BOOL bResult = DeleteFileA ( m_szCrashFileName );

    m_hCrashFile = CreateFileA( m_szCrashFileName,
                                GENERIC_WRITE,
                                0,
                                0,
                                OPEN_ALWAYS,
                                FILE_FLAG_WRITE_THROUGH,
                                0 );
    if ( m_hCrashFile != NULL )
    {
        SetFilePointer( m_hCrashFile, 0, 0, FILE_BEGIN );
	}
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHCloseCrashFile()
{
    if ( m_hCrashFile )
    {
		CloseHandle ( m_hCrashFile );
		m_hCrashFile = NULL;
	}
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHOpenDebugFile()
{
	if ( DebugMode )
	{
		BOOL bResult = DeleteFileA ( m_szDebugFileName );

		m_hDebugFile = CreateFileA ( m_szDebugFileName,
									GENERIC_WRITE,
									0,
									0,
									OPEN_ALWAYS,
									FILE_FLAG_WRITE_THROUGH,
									0 );
		if ( m_hDebugFile )
		{
			SetFilePointer( m_hDebugFile, 0, 0, FILE_BEGIN );
		}
	}
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHCloseDebugFile()
{
	if ( m_hDebugFile != NULL )
	{
		CloseHandle ( m_hDebugFile );
		m_hDebugFile = NULL;
	}
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHSetInvalidParameterHandler()
{
	m__invalid_parameter_handler = _set_invalid_parameter_handler ( StaticInvalidParameterHandler );
	EEHPrintDebug ( "INFO: Setting Invalid Parameter Handler \r\n" );
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHInitializeSymbols ()
{
	//
	//	Get Process
    m_hProcess = GetCurrentProcess ( );

	BOOL bResult = FALSE;

	//
	if ( pfSymInitialize != NULL )
	{
		ZeroMemory(m_szDirectory, sizeof(m_szDirectory));
		strcpy_s ( m_szDirectory, sizeof(m_szDirectory), m_szCrashFileName );
		EEHGetDirectory ( m_szDirectory, sizeof(m_szDirectory) );

		ZeroMemory(m_szSearchPath, sizeof(m_szSearchPath));
		sprintf_s ( m_szSearchPath, sizeof ( m_szSearchPath ) - 1,
			"%s;%s\\PDB", m_szDirectory,  m_szDirectory );
		EEHPrintDebug ( "INFO: Search Path : %s\r\n", m_szSearchPath );

		// Initialize DbgHelp
		if ( pfSymCleanup != NULL )
		{
			bResult = pfSymCleanup(m_hProcess);
		}

		//
		//		Initialize
		bResult = pfSymInitialize ( m_hProcess, m_szSearchPath, TRUE );
		if ( ! bResult )
		{
			EEHPrintDebug ( "ERROR: SymInitialize has failed (TRUE parameter): %s\r\n", EEHGetLastErrorText ( ));

			// Stop with a banner
			bResult = pfSymInitialize ( m_hProcess, m_szSearchPath, FALSE );
			if ( ! bResult )
			{
				// Stop with a banner
				EEHPrintDebug ( "ERROR: SymInitialize has failed again: %s\r\n", EEHGetLastErrorText ( ));
			}
			else
			{
				EEHPrintDebug ( "INFO: SymInitialize has succeeded (FALSE parameter)\r\n" );
			}
		}
	}
	else
	{
		EEHPrintDebug ( "ERROR: SymInitialize not initialized\r\n" );
	}

}

//
//====================================================================================
//  FUNCTION: GetLastErrorText
//
//  PURPOSE: copies error message text to string
//
//  PARAMETERS:
//    lpszBuf - destination buffer
//    dwSize - size of buffer
//
//  RETURN VALUE:
//    destination buffer
//
//  COMMENTS:
//====================================================================================
const char *CExceptErrorHandler::EEHGetLastErrorText ( )
{
	DWORD		dwRet;
	char *		lpszTemp = NULL;

	dwRet =
		FormatMessageA( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL,
			GetLastError ( ),
			LANG_NEUTRAL,
			(char *)&lpszTemp,
			0,
			NULL );

	// supplied buffer is not long enough
	if ( ! dwRet )
	{
		ZeroMemory ( m_szErrorText, sizeof(m_szErrorText) );
	}
	else
	{
		lpszTemp [ strlen(lpszTemp)-2 ] = '\0';  //remove cr and newline character
		sprintf_s ( m_szErrorText, sizeof(m_szErrorText), "%s (0x%x)", lpszTemp, GetLastError ( ) );
	}

	if ( lpszTemp )
	{
		LocalFree ( ( HLOCAL ) lpszTemp );
	}

	return m_szErrorText;

}


//
//====================================================================================
//
//====================================================================================
BOOL CExceptErrorHandler::EEHEnableCrashingOnCrashes()
{
	//	GetProcessMitigationPolicy is the replacement ?
	//	SetProcessMitigationPolicy
	BOOL bResult = false;

    typedef BOOL (WINAPI *typeGetPolicy)(LPDWORD lpFlags);
    typedef BOOL (WINAPI *typeSetPolicy)(DWORD dwFlags);
    const DWORD EXCEPTION_SWALLOWING = 0x1;

    HMODULE kernel32 = LoadLibraryA("kernel32.dll");

    typeGetPolicy procGetPolicy = (typeGetPolicy)GetProcAddress(kernel32, "GetProcessUserModeExceptionPolicy");
	if ( procGetPolicy == NULL )
	{
		EEHPrintDebug ( "ERROR: GetProcessUserModeExceptionPolicy not found\r\n" );
	}

	typeSetPolicy procSetPolicy = (typeSetPolicy)GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy");
	if ( procSetPolicy == NULL )
	{
		EEHPrintDebug ( "ERROR: SetProcessUserModeExceptionPolicy not found\r\n" );
	}

    if (procGetPolicy != NULL && procSetPolicy != NULL )
    {
       DWORD dwFlags;
	   bResult = procGetPolicy(&dwFlags);
       if ( bResult )
       {
           // Turn off the filter
           bResult = procSetPolicy(dwFlags & ~EXCEPTION_SWALLOWING);
		   if ( ! bResult )
		   {
				EEHPrintDebug ( "ERROR: SetProcessUserModeExceptionPolicy fails\r\n" );
		   }
       }
	   else
	   {
			EEHPrintDebug ( "ERROR: GetProcessUserModeExceptionPolicy fails\r\n" );
	   }
   }

	return bResult;
}

//
//====================================================================================
//
//		Destructor 
//
//====================================================================================
CExceptErrorHandler::~CExceptErrorHandler( )
{
    SetUnhandledExceptionFilter( m_previousFilter );
}

//
//====================================================================================
//
//		Lets user change the name of the report file to be generated 
//
//====================================================================================
void CExceptErrorHandler::EEHSetLogFileName( char * pszLogFileName, size_t cbSize )
{
    strcpy_s ( m_szCrashFileName, cbSize, pszLogFileName );
}

//
//====================================================================================
//
//		Entry point where control comes on an unhandled exception 
//
//====================================================================================
LONG WINAPI CExceptErrorHandler::EEHMacallanUnhandledExceptionFilter ( PEXCEPTION_POINTERS pExceptionInfo )
{
	//
	EEHInitializeSymbols ();

	//
	EEHOpenCrashFile ();

    if ( m_hCrashFile != NULL )
    {
        EEHGenerateExceptionReport( pExceptionInfo );

		EEHCloseCrashFile();
    }

    if ( m_previousFilter != NULL )
	{
        return m_previousFilter( pExceptionInfo );
	}
    else
	{
        return EXCEPTION_CONTINUE_SEARCH;
	}
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHWriteStackTrace ()
{
	const ULONG maxStack = 63;
	unsigned int   i;
	void         * stack[ maxStack ];
	unsigned short frames;

	if ( pfRtlCaptureStackBackTrace != NULL )
	{
		EEHPrintCrash("\r\n**** RtlCaptureStackBackTrace\r\n");

		ULONG skipFrames = 0;
		frames               = pfRtlCaptureStackBackTrace ( skipFrames, maxStack, stack, NULL );
		m_pSymbolInfo->MaxNameLen		= SYMBOL_NAME_LEN;
		m_pSymbolInfo->SizeOfStruct		= sizeof( SYMBOL_INFO );

		if ( pfSymFromAddr != NULL )
		{
			EEHPrintCrash("**** Frames : %d\r\n", frames );

			for( i = 0; i < frames; i++ )
			{
				BOOL bResult = pfSymFromAddr( m_hProcess, ( DWORD64 )( stack[ i ] ), 0, m_pSymbolInfo );
				if ( ! bResult )
				{
					EEHPrintCrash( "Error during pfSymFromAddr: %s\r\n", EEHGetLastErrorText() );
				}

				EEHPrintCrash( "%02i: %-64s - 0x%016I64X", frames - i - 1, m_pSymbolInfo->Name, m_pSymbolInfo->Address );

				ZeroMemory(m_szModule, sizeof(m_szModule));
				DWORD section = 0;
				DWORD offset = 0;
				PVOID addr = stack[ i ];
				bResult = EEHGetLogicalAddress ( addr, m_szModule, sizeof(m_szModule) - 1, section, offset );
				if ( bResult )
				{
					EEHPrintCrash( " - %04X:%08X (%s)", section, offset, m_szModule );
				}
#ifdef _M_IX86
				if ( pfSymGetLineFromAddr64 != NULL )
				{
					// Get the source line for this stack frame entry
					IMAGEHLP_LINE64 lineInfo = { sizeof (IMAGEHLP_LINE64) };
					DWORD dwLineDisplacement;

					bResult = 
						pfSymGetLineFromAddr64 ( 
							m_hProcess, ( DWORD64 )( stack[ i ] ), &dwLineDisplacement, &lineInfo );
					if ( bResult )
					{
						EEHPrintCrash(" %s line %u", lineInfo.FileName, lineInfo.LineNumber); 
					}
					else
					{
						EEHPrintCrash(" (No Line)" );
					}
				}
				else if ( pfSymGetLineFromAddr != NULL )
				{
					// Get the source line for this stack frame entry
					IMAGEHLP_LINE lineInfo = { sizeof (IMAGEHLP_LINE) };
					DWORD dwLineDisplacement;

					bResult = 
						pfSymGetLineFromAddr (	
							m_hProcess, ( DWORD ) stack[ i ], &dwLineDisplacement, &lineInfo );
					if ( bResult )
					{
						EEHPrintCrash(" %s line %u",lineInfo.FileName,lineInfo.LineNumber); 
					}
					else
					{
						EEHPrintCrash(" (No Line)" );
					}
				}
				else
				{
					EEHPrintCrash(" (No Line)" );
				}
#endif

#ifdef _M_X64
				if ( pfSymGetLineFromAddr64 != NULL )
				{
					// Get the source line for this stack frame entry
					IMAGEHLP_LINE64 lineInfo = { sizeof (IMAGEHLP_LINE64) };
					DWORD dwLineDisplacement;

					if ( pfSymGetLineFromAddr64 (	m_hProcess, ( DWORD64 )( stack[ i ] ),
													&dwLineDisplacement, &lineInfo ) )
					{
						EEHPrintCrash(" %s line %u",lineInfo.FileName,lineInfo.LineNumber); 
					}
					else
					{
						EEHPrintCrash(" (No Line)" );
					}
				}
				else
				{
					EEHPrintCrash(" (No Line)" );
				}
#endif
				EEHPrintCrash("\r\n");
			}
		}
		else
		{
			EEHPrintCrash("**** SymFromAddr is NULL\r\n");
		}

	}
	else
	{
		EEHPrintCrash( "**** RtlCaptureStackBackTrace is NULL\r\n" );
	}

	EEHPrintCrash( "**** RtlCaptureStackBackTrace End\r\n" );
}

//
//====================================================================================
//
//		Open the report file, and write the desired information to it.  Called by 
//		MacallanUnhandledExceptionFilter                                               
//
//====================================================================================
void CExceptErrorHandler::EEHGenerateExceptionReport ( PEXCEPTION_POINTERS pExceptionInfo )
{
    // Start out with a banner
    EEHPrintCrash("**** Exception Handler Report\r\n");

    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    // First print information about the type of fault
    EEHPrintCrash(	"Exception code: %08X %s\r\n",
					pExceptionRecord->ExceptionCode,
	                EEHGetExceptionString(pExceptionRecord->ExceptionCode) );

    // Now print information about where the fault occured
	ZeroMemory ( m_szFaultingModule, sizeof(m_szFaultingModule) );

    DWORD section, offset;
    EEHGetLogicalAddress (	pExceptionRecord->ExceptionAddress,
						m_szFaultingModule,
						sizeof ( m_szFaultingModule ) - 1,
						section, offset );
#ifdef _M_IX86
    EEHPrintCrash( "Fault address:  %08X %04X:%08X %s\r\n", pExceptionRecord->ExceptionAddress, section, offset, m_szFaultingModule );
#endif

#ifdef _M_X64
    EEHPrintCrash( "Fault address:  %016I64X %04X:%08X %s\r\n", pExceptionRecord->ExceptionAddress, section, offset, m_szFaultingModule );
#endif

    PCONTEXT pCtx = pExceptionInfo->ContextRecord;

// Show the registers
#ifdef _M_IX86  // X86 Only!

    EEHPrintCrash( "\r\n**** Registers:\r\n" );

    EEHPrintCrash("EAX:%08X\r\nEBX:%08X\r\nECX:%08X\r\nEDX:%08X\r\nESI:%08X\r\nEDI:%08X\r\n",
			pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx,
            pCtx->Esi, pCtx->Edi );

    EEHPrintCrash( "CS:EIP:%04X:%08X\r\n", pCtx->SegCs, pCtx->Eip );
    EEHPrintCrash( "SS:ESP:%04X:%08X  EBP:%08X\r\n",
                pCtx->SegSs, pCtx->Esp, pCtx->Ebp );
    EEHPrintCrash( "DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n",
                pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
    EEHPrintCrash( "Flags:%08X\r\n", pCtx->EFlags );
#endif

#ifdef _M_X64  
    EEHPrintCrash( "\r\n**** Registers:\r\n" );

    EEHPrintCrash("RAX:%016I64X\r\nRBX:%016I64X\r\nRCX:%016I64X\r\nRDX:%016I64X\r\nRSI:%016I64X\r\nRDI:%016I64X\r\n",
			pCtx->Rax, pCtx->Rbx, pCtx->Rcx, pCtx->Rdx,
            pCtx->Rsi, pCtx->Rdi );

    EEHPrintCrash( "CS:RIP:%04X:%016I64X\r\n", pCtx->SegCs, pCtx->Rip );
    EEHPrintCrash( "SS:RSP:%04X:%016I64X  EBP:%016I64X\r\n",
                pCtx->SegSs, pCtx->Rsp, pCtx->Rbp );
    EEHPrintCrash( "DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n",
                pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
    EEHPrintCrash( "Flags:%08X\r\n", pCtx->EFlags );
#endif

	if ( pfSymGetOptions != NULL )
	{
		DWORD options = pfSymGetOptions();
		EEHPrintCrash( "**** Symbols Get Options : 0x%08X\r\n", options );
	}
	else
	{
		EEHPrintCrash( "**** pfSymGetOptions is NULL\r\n" );
	}

	if ( pfSymSetOptions != NULL )
	{
		pfSymSetOptions ( SYMOPT_DEFERRED_LOADS );
	}
	else
	{
		EEHPrintCrash( "**** pfSymSetOptions is NULL\r\n" );
	}

	if ( pfSymInitialize != NULL )
	{
		CONTEXT trashableContext = *pCtx;

		//
#ifdef _M_IX86
		EEHWriteStackDetails( &trashableContext, false );
		EEHWriteStackTrace();
#endif

#ifdef _M_X64
		EEHWriteStackDetails( &trashableContext, false );
		EEHWriteStackTrace();
#endif

#ifdef _M_IX86  // X86 Only!

		EEHPrintCrash( "\r\n**** Local Variables And Parameters\r\n" );

		trashableContext = *pCtx;
		EEHWriteStackDetails ( &trashableContext, true );

		if ( TraceMode )
		{
			EEHPrintCrash( "\r\n**** Global Variables\r\n" );

			if ( pfSymEnumSymbols != NULL )
			{
				pfSymEnumSymbols (	m_hProcess,
									(DWORD64)GetModuleHandleA(m_szFaultingModule),
									0, EEHEnumerateSymbolsCallback, 0 );
			}
		}
#endif

#if _M_X64
		EEHPrintCrash( "\r\n**** Local Variables And Parameters\r\n" );

		trashableContext = *pCtx;
		EEHWriteStackDetails ( &trashableContext, true );

		if ( TraceMode )
		{
			EEHPrintCrash( "\r\n**** Global Variables\r\n" );

			if ( pfSymEnumSymbols != NULL )
			{
				pfSymEnumSymbols (	m_hProcess,
									(DWORD64)GetModuleHandleA(m_szFaultingModule),
									0, EEHEnumerateSymbolsCallback, 0 );
			}
		}
#endif

	}
	else
	{
		EEHPrintCrash( "**** pfSymInitialize is NULL : 0x%0x\r\n", pfSymInitialize );
	}

	if ( pfSymCleanup != NULL )
	{
		pfSymCleanup ( m_hProcess );
	}

    EEHPrintCrash( "\r\n" );

	// Stop with a banner
	EEHPrintCrash("**** End Of Report\r\n");

}

//
//====================================================================================
//
//		Given an exception code, returns a pointer to a static string with a 
//		description of the exception                                         
//
//====================================================================================
char * CExceptErrorHandler::EEHGetExceptionString( DWORD dwCode )
{
    #define EXCEPTION( x ) case EXCEPTION_##x: return (#x);

    switch ( dwCode )
    {
        EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )
        EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )
        EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )
        EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )
        EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )
        EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )
        EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )
        EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )
        EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )
        EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
    }

    // If not one of the "known" exceptions, try to get the string
    // from NTDLL.DLL's message table.

    static char szBuffer[512] = { 0 };

    FormatMessageA(	FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
					GetModuleHandleA( "NTDLL.DLL"),
					dwCode, 0, szBuffer, sizeof ( szBuffer ) - 1, 0 );

    return szBuffer;
}

//
//====================================================================================
//
//		Given a linear address, locates the module, section, and offset containing  
//		that address.                                                               
//                                                                             
//		Note: the szModule paramater buffer is an output buffer of length specified 
//		by the len parameter (in characters!)                                       
//
//====================================================================================
BOOL CExceptErrorHandler::EEHGetLogicalAddress( PVOID addr, char * pModule, DWORD len, DWORD &section, DWORD &offset )
{
#ifdef _M_IX86
    MEMORY_BASIC_INFORMATION32 mbi;
#endif

#ifdef _M_X64
    MEMORY_BASIC_INFORMATION64 mbi;
#endif

	section = 0xffff;
	offset	= 0xffffffff;

    if ( ! VirtualQuery( addr, (PMEMORY_BASIC_INFORMATION) &mbi, sizeof(mbi) ) )
	{
        return FALSE;
	}

#ifdef _M_IX86
	DWORD mbiAllocationBase		= mbi.AllocationBase;
#endif

#ifdef _M_X64
	ULONGLONG mbiAllocationBase	= mbi.AllocationBase;
#endif

	HMODULE	hModule	= NULL;
 	BOOL bResult = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (char *) addr, &hModule);
	if ( ! bResult )
	{
		return FALSE;
	}

	if ( ! GetModuleFileNameA( (HMODULE) hModule, pModule, len ) )
	{
		return FALSE;
	}

#ifdef _M_IX86
    // Point to the DOS header in memory
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)mbiAllocationBase;

    // From the DOS header, find the NT (PE) header
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(mbiAllocationBase + pDosHdr->e_lfanew);

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );

    DWORD rva = (DWORD)addr - mbiAllocationBase; // RVA is offset from module load address

    // Iterate through the section table, looking for the one that encompasses
    // the linear address.
    for (   unsigned i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++, pSection++ )
    {
        DWORD sectionStart = pSection->VirtualAddress;
        DWORD sectionEnd = sectionStart + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);

        // Is the address in this section???
        if ( (rva >= sectionStart) && (rva <= sectionEnd) )
        {
            // Yes, address is in the section.  Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.
            section = i+1;
            offset = rva - sectionStart;
            return TRUE;
        }
    }
#endif

#ifdef _M_X64

    // Point to the DOS header in memory
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)mbiAllocationBase;

    // From the DOS header, find the NT (PE) header
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(mbiAllocationBase + pDosHdr->e_lfanew);

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );

    ULONGLONG rva = (ULONGLONG) addr - mbiAllocationBase; // RVA is offset from module load address

    // Iterate through the section table, looking for the one that encompasses
    // the linear address.
    for (   unsigned i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++, pSection++ )
    {
        DWORD sectionStart	= pSection->VirtualAddress;
        DWORD sectionEnd	= sectionStart + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);

        // Is the address in this section???
        if ( (rva >= sectionStart) && (rva <= sectionEnd) )
        {
            // Yes, address is in the section.  Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.
            section = i+1;
            offset = (DWORD ) (  rva - sectionStart );
            return TRUE;
        }
    }
#endif
	
	return FALSE;   // Should never get here!
}

//
//====================================================================================
//
//		Walks the stack, and writes the results to the report file 
//
//====================================================================================
void CExceptErrorHandler::EEHWriteStackDetails( PCONTEXT pContext, bool bWriteVariables )  // true if local/params should be output
{
    EEHPrintCrash( "\r\n**** Call stack (WriteStackDetails) - Write Variable : %d:\r\n", bWriteVariables );


    DWORD dwMachineType = 0;
    // Could use SymSetOptions here to add the SYMOPT_DEFERRED_LOADS flag

    STACKFRAME sf;
    memset( &sf, 0, sizeof(sf) );

	//
	//	Only for X86
#ifdef _M_IX86
    EEHPrintCrash( "Address   Frame     Function and SourceFile\r\n" );

    // Initialize the STACKFRAME structure for the first call.  This is only
    // necessary for Intel CPUs, and isn't mentioned in the documentation.
    sf.AddrPC.Offset       = pContext->Eip;
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Esp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Ebp;
    sf.AddrFrame.Mode      = AddrModeFlat;

    dwMachineType = IMAGE_FILE_MACHINE_I386;

#endif


#ifdef _M_X64
    EEHPrintCrash( "Address           Frame             Function andSourceFile\r\n" );
    // Initialize the STACKFRAME structure for the first call.  This is only
    // necessary for Intel CPUs, and isn't mentioned in the documentation.
    sf.AddrPC.Offset       = pContext->Rip;
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Rsp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Rbp;
    sf.AddrFrame.Mode      = AddrModeFlat;

    dwMachineType = IMAGE_FILE_MACHINE_AMD64;

#endif


//
//	Only for X86
#ifdef _M_IX86

	if ( pfStackWalk != NULL && pfSymFunctionTableAccess != NULL && pfSymGetModuleBase != NULL )
	{
		while ( true )
		{
			// Get the next stack frame
			BOOL bResult =
				pfStackWalk(  dwMachineType, m_hProcess, GetCurrentThread(),
								&sf, pContext, 0, pfSymFunctionTableAccess, pfSymGetModuleBase, 0 );
			if ( ! bResult )
			{
				break;
			}

			if ( 0 == sf.AddrFrame.Offset ) // Basic sanity check to make sure
			{
				break;                      // the frame is OK.  Bail if not.
			}

			EEHPrintCrash( "%08X  %08X  ", sf.AddrPC.Offset, sf.AddrFrame.Offset );

			DWORD64 symDisplacement = 0;    // Displacement of the input address,
											// relative to the start of the symbol

			m_pSymbolInfo->MaxNameLen		= SYMBOL_NAME_LEN;
			m_pSymbolInfo->SizeOfStruct		= sizeof( SYMBOL_INFO );

			if ( pfSymFromAddr != NULL )
			{
				if ( pfSymFromAddr(m_hProcess, sf.AddrPC.Offset, &symDisplacement, m_pSymbolInfo))
				{
					EEHPrintCrash( "%hs+%08I64X", m_pSymbolInfo->Name, symDisplacement );
				}
				else    // No symbol found.  Print out the logical address instead.
				{
					ZeroMemory ( m_szModule, sizeof(m_szModule) );
					DWORD section = 0, offset = 0;

					EEHGetLogicalAddress(  (PVOID) sf.AddrPC.Offset,
											m_szModule, sizeof ( m_szModule ) - 1, section, offset );

					EEHPrintCrash( "%04X:%08X %s", section, offset, m_szModule );
				}
			}
			else
			{
				ZeroMemory ( m_szModule, sizeof(m_szModule) );

				DWORD section = 0, offset = 0;

				EEHGetLogicalAddress(  (PVOID)sf.AddrPC.Offset,
										m_szModule, sizeof ( m_szModule ) - 1, section, offset );

				EEHPrintCrash( "%04X:%08X %s", section, offset, m_szModule );
			}

			// Get the source line for this stack frame entry
			IMAGEHLP_LINE lineInfo = { sizeof (IMAGEHLP_LINE) };
			DWORD dwLineDisplacement;
			if ( pfSymGetLineFromAddr != NULL )
			{
				if ( pfSymGetLineFromAddr (	m_hProcess, sf.AddrPC.Offset,
											&dwLineDisplacement, &lineInfo ) )
				{
					EEHPrintCrash(" %s line %u", lineInfo.FileName, lineInfo.LineNumber); 
				}
			}
			else
			{
				EEHPrintCrash( "**** pfSymGetLineFromAddr is NULL\r\n" );
			}

			EEHPrintCrash( "\r\n");

			// Write out the variables, if desired
			if ( bWriteVariables )
			{
				// Use SymSetContext to get just the locals/params for this frame
				IMAGEHLP_STACK_FRAME imagehlpStackFrame;
				imagehlpStackFrame.InstructionOffset = sf.AddrPC.Offset;
				if ( pfSymSetContext != NULL )
				{
					pfSymSetContext( m_hProcess, &imagehlpStackFrame, 0 );

					// Enumerate the locals/parameters
					if ( pfSymEnumSymbols != NULL )
					{
						pfSymEnumSymbols( m_hProcess, 0, 0, EEHEnumerateSymbolsCallback, &sf );
					}

				}
				else
				{
					EEHPrintCrash( "**** pfSymSetContext is NULL\r\n" );
				}
			//	EEHPrintCrash( "\r\n") );
			}
		}
    }

#endif

//
#ifdef _M_X64

	if ( pfStackWalk64 != NULL && pfSymFunctionTableAccess64 != NULL && pfSymGetModuleBase64 != NULL )
	{
		while ( true )
		{
			// Get the next stack frame
			BOOL bResult = 
				pfStackWalk64 (  dwMachineType, m_hProcess, GetCurrentThread(),
								&sf, pContext, 0, pfSymFunctionTableAccess64, pfSymGetModuleBase64, 0 );
			if ( ! bResult )
			{
				break;
			}

			if ( 0 == sf.AddrFrame.Offset ) // Basic sanity check to make sure
			{
				break;                      // the frame is OK.  Bail if not.
			}

			EEHPrintCrash( "%016I64X  %016I64X  ", sf.AddrPC.Offset, sf.AddrFrame.Offset );

			m_pSymbolInfo->MaxNameLen		= SYMBOL_NAME_LEN;
			m_pSymbolInfo->SizeOfStruct		= sizeof( SYMBOL_INFO );

			DWORD64 symDisplacement = 0;    // Displacement of the input address,
											// relative to the start of the symbol

			if ( pfSymFromAddr != NULL )
			{
				if ( pfSymFromAddr(m_hProcess, sf.AddrPC.Offset, &symDisplacement, m_pSymbolInfo))
				{
					EEHPrintCrash( "%hs+%08I64X", m_pSymbolInfo->Name, symDisplacement );
				}
				else    // No symbol found.  Print out the logical address instead.
				{
					ZeroMemory ( m_szModule, sizeof(m_szModule) );
					DWORD section = 0, offset = 0;

					EEHGetLogicalAddress(  (PVOID) sf.AddrPC.Offset,
										m_szModule, sizeof ( m_szModule ) - 1, section, offset );

					EEHPrintCrash( "%04X:%08X %s", section, offset, m_szModule );
				}
			}
			else
			{
				ZeroMemory ( m_szModule, sizeof(m_szModule) );

				DWORD section = 0, offset = 0;

				EEHGetLogicalAddress(  (PVOID)sf.AddrPC.Offset,
									m_szModule, sizeof ( m_szModule ) - 1, section, offset );

				EEHPrintCrash( "%04X:%08X %s", section, offset, m_szModule );
			}

			// Get the source line for this stack frame entry
			IMAGEHLP_LINE lineInfo = { sizeof (IMAGEHLP_LINE) };
			DWORD dwLineDisplacement;
			if ( pfSymGetLineFromAddr64 != NULL )
			{
				if ( pfSymGetLineFromAddr64 (	m_hProcess, sf.AddrPC.Offset,
												&dwLineDisplacement, &lineInfo ) )
				{
					EEHPrintCrash(" %s line %u",lineInfo.FileName,lineInfo.LineNumber); 
				}
			}
			else
			{
				EEHPrintCrash( "**** pfSymGetLineFromAddr is NULL\r\n" );
			}

			EEHPrintCrash( "\r\n" );

			// Write out the variables, if desired
			if ( bWriteVariables )
			{
				// Use SymSetContext to get just the locals/params for this frame
				IMAGEHLP_STACK_FRAME imagehlpStackFrame;
				imagehlpStackFrame.InstructionOffset = sf.AddrPC.Offset;
				if ( pfSymSetContext != NULL )
				{
					pfSymSetContext( m_hProcess, &imagehlpStackFrame, 0 );

					// Enumerate the locals/parameters
					if ( pfSymEnumSymbols != NULL )
					{
						pfSymEnumSymbols( m_hProcess, 0, 0, EEHEnumerateSymbolsCallback, &sf );
					}

				}
				else
				{
					EEHPrintCrash( "**** pfSymSetContext is NULL\r\n" );
				}
			//	EEHPrintCrash( "\r\n") );
			}
		}
    }

#endif


}

//
//====================================================================================
//
//		The function invoked by SymEnumSymbols
//
//====================================================================================
BOOL CALLBACK CExceptErrorHandler::EEHEnumerateSymbolsCallback ( PSYMBOL_INFO  pSymInfo, ULONG SymbolSize, PVOID UserContext )
{

#ifdef _M_IX86
	ZeroMemory ( m_szSymbolBuffer, sizeof(m_szSymbolBuffer) );

	if ( UserContext )
	{
		__try
		{
			if ( EEHFormatSymbolValue( pSymInfo, ( STACKFRAME * ) UserContext,
										m_szSymbolBuffer, sizeof ( m_szSymbolBuffer ) - 1 ) )
			{
				EEHPrintCrash ( "\t%s\r\n", m_szSymbolBuffer );
			}
		}
		__except( 1 )
		{
			DWORD	dwAddress = ( DWORD ) pSymInfo->Address;
			EEHPrintCrash(	"Symbol= %08lX [%4x] %s\r\n",
						dwAddress, SymbolSize, pSymInfo->Name );
		}
	}
	else
	{
		if ( pSymInfo != NULL )
		{
			DWORD	dwAddress = ( DWORD ) pSymInfo->Address;
			EEHPrintCrash(	"Symbol: %08lX [%4x] %s\r\n",
						dwAddress, SymbolSize, pSymInfo->Name );
		}
		else
		{
			EEHPrintCrash ( "pSymInfo is NULL\r\n" );
		}
	}

#endif


#ifdef _M_X64
	ZeroMemory ( m_szSymbolBuffer, sizeof(m_szSymbolBuffer) );

	if ( UserContext )
	{
		__try
		{
			if ( EEHFormatSymbolValue( pSymInfo, ( STACKFRAME * ) UserContext,
										m_szSymbolBuffer, sizeof ( m_szSymbolBuffer ) - 1 ) )
			{
				EEHPrintCrash ( "\t%s\r\n", m_szSymbolBuffer );
			}
		}
		__except( 1 )
		{
			DWORD64	dwAddress = ( DWORD64 ) pSymInfo->Address;
			EEHPrintCrash(	"Symbol= %016I64X [%4x] %s\r\n",
						dwAddress, SymbolSize, pSymInfo->Name );
		}
	}
	else
	{
		if ( pSymInfo != NULL )
		{
			DWORD64	dwAddress = ( DWORD64 ) pSymInfo->Address;
			EEHPrintCrash(	"Symbol: %016I64X [%4x] %s\r\n",
						dwAddress, SymbolSize, pSymInfo->Name );
		}
		else
		{
			EEHPrintCrash ( "pSymInfo is NULL\r\n" );
		}
	}

#endif

	return TRUE;
}

//
//====================================================================================
//
//		Given a SYMBOL_INFO representing a particular variable, displays its
//		contents.  If it's a user defined type, display the members and their
//		values.
//
//====================================================================================
bool CExceptErrorHandler::EEHFormatSymbolValue ( PSYMBOL_INFO pSym, STACKFRAME * sf, char * pszBuffer, unsigned cbBuffer )
{
	//
	//		Return If PSYM is NULL
	if ( pSym == NULL )
	{
		return false;
	}

    // Indicate if the variable is a local or parameter
    if ( pSym->Flags & IMAGEHLP_SYMBOL_INFO_PARAMETER )
	{
        sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "Parameter " );
	}
    else if ( pSym->Flags & IMAGEHLP_SYMBOL_INFO_LOCAL )
	{
        sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "Local     " );
	}

    // If it's a function, don't do anything.
    if ( pSym->Tag == 5 )   // SymTagFunction from CVCONST.H from the DIA SDK
	{
        return false;
	}

    // Emit the variable name
	if ( pSym != NULL )
	{
#ifdef _M_IX86
		sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "(%08X ", (long) pSym->Address );
#endif 

#ifdef _M_X64
		sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "(%016I64X ", pSym->Address );
#endif 

		sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), " [%4X] ", pSym->NameLen );
		if ( pSym->NameLen > 0 )
		{
			sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "SymName=%s)", pSym->Name );
		}
		else
		{
			sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "SymName=<EMPTY>)" );
		}
	}
	else
	{
		sprintf_s ( pszBuffer + strlen(pszBuffer), cbBuffer - strlen(pszBuffer), "(SymName=UnKnown)" );
	}

    DWORD_PTR pVariable = 0;    // Will point to the variable's data in memory

    if ( pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGRELATIVE )
    {
        // if ( pSym->Register == 8 )   // EBP is the value 8 (in DBGHELP 5.1)
        {                               //  This may change!!!
            pVariable = sf->AddrFrame.Offset;
            pVariable += (DWORD_PTR)pSym->Address;
        }
        // else
        //  return false;
    }
    else if ( pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGISTER )
    {
        return false;   // Don't try to report register variable
    }
    else
    {
        pVariable = (DWORD_PTR)pSym->Address;   // It must be a global variable
    }

    // Determine if the variable is a user defined type (UDT).  IF so, bHandled
    // will return true.
    bool bHandled;
	EEHDumpTypeIndex (		pszBuffer, cbBuffer,
							pSym->ModBase, pSym->TypeIndex,
							0, pVariable, bHandled );

    if ( ! bHandled )
    {
        // The symbol wasn't a UDT, so do basic, stupid formatting of the
        // variable.  Based on the size, we're assuming it's a char, WORD, or
        // DWORD.
        BasicType basicType = EEHGetBasicType( pSym->TypeIndex, pSym->ModBase );
        
        EEHFormatOutputValue ( pszBuffer, cbBuffer, basicType, pSym->Size, (PVOID)pVariable ); 
    }


    return true;
}

//
//====================================================================================
//
//		If it's a user defined type (UDT), recurse through its members until we're
//		at fundamental types.  When he hit fundamental types, return
//		bHandled = false, so that FormatSymbolValue() will format them.
//
//====================================================================================
char * CExceptErrorHandler::EEHDumpTypeIndex(
        char * pszCurrBuffer,
		size_t	cbBuffer,
        DWORD64 modBase,
        DWORD dwTypeIndex,
        unsigned nestingLevel,
        DWORD_PTR offset,
        bool & bHandled )
{
    bHandled = false;

	if ( pfSymGetTypeInfo == NULL )
	{
		return pszCurrBuffer;
	}

    // Get the name of the symbol.  This will either be a Type name (if a UDT),
    // or the structure member name.
    WCHAR * pwszTypeName;
    if ( pfSymGetTypeInfo( m_hProcess, modBase, dwTypeIndex, TI_GET_SYMNAME, &pwszTypeName ) )
    {
        sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbBuffer - strlen(pszCurrBuffer), " %ls", pwszTypeName );
        LocalFree( pwszTypeName );
    }

    // Determine how many children this type has.
    DWORD dwChildrenCount = 0;
    pfSymGetTypeInfo( m_hProcess, modBase, dwTypeIndex, TI_GET_CHILDRENCOUNT,
                    &dwChildrenCount );

    if ( ! dwChildrenCount )		// If no children, we're done
	{
        return pszCurrBuffer;
	}

    // Prepare to get an array of "TypeIds", representing each of the children.
    // SymGetTypeInfo(TI_FINDCHILDREN) expects more memory than just a
    // TI_FINDCHILDREN_PARAMS struct has.  Use derivation to accomplish this.
    struct FINDCHILDREN : TI_FINDCHILDREN_PARAMS
    {
        ULONG   MoreChildIds[1024];
        FINDCHILDREN()
		{
			Count = sizeof(MoreChildIds) / sizeof(MoreChildIds[0]);
		}
    } children;

    children.Count = dwChildrenCount;
    children.Start= 0;

    // Get the array of TypeIds, one for each child type
    if ( ! pfSymGetTypeInfo ( m_hProcess, modBase, dwTypeIndex, TI_FINDCHILDREN, &children ) )
    {
        return pszCurrBuffer;
    }

    // Append a line feed
    sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbBuffer - strlen(pszCurrBuffer), "\r\n" );

    // Iterate through each of the children
    for ( unsigned i = 0; i < dwChildrenCount; i++ )
    {
        // Add appropriate indentation level (since this routine is recursive)
        for ( unsigned j = 0; j <= nestingLevel+1; j++ )
		{
			sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbBuffer - strlen(pszCurrBuffer), "\t" );
		}

        // Recurse for each of the child types
        bool bHandled2;
		EEHDumpTypeIndex(	pszCurrBuffer, cbBuffer,
							modBase, children.ChildId[i], nestingLevel+1,
							offset, bHandled2 );

        // If the child wasn't a UDT, format it appropriately
        if ( ! bHandled2 )
        {
            // Get the offset of the child member, relative to its parent
            DWORD dwMemberOffset;
            pfSymGetTypeInfo( m_hProcess, modBase, children.ChildId[i],
                            TI_GET_OFFSET, &dwMemberOffset );

            // Get the real "TypeId" of the child.  We need this for the
            // SymGetTypeInfo( TI_GET_TYPEID ) call below.
            DWORD typeId;
            pfSymGetTypeInfo( m_hProcess, modBase, children.ChildId[i],
                            TI_GET_TYPEID, &typeId );

            // Get the size of the child member
            ULONG64 length;
            pfSymGetTypeInfo(m_hProcess, modBase, typeId, TI_GET_LENGTH,&length);

            // Calculate the address of the member
            DWORD_PTR dwFinalOffset = offset + dwMemberOffset;

            BasicType basicType = EEHGetBasicType(children.ChildId[i], modBase );

            EEHFormatOutputValue( pszCurrBuffer, cbBuffer, basicType,
                                                length, (PVOID)dwFinalOffset ); 

            sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbBuffer - strlen(pszCurrBuffer), "\r\n" );
        }
    }

    bHandled = true;
    return pszCurrBuffer;
}

//
//====================================================================================
//
//
//
//====================================================================================
char * CExceptErrorHandler::EEHFormatOutputValue(	char * pszCurrBuffer,
													size_t	cbCurrBuffer,
													BasicType basicType,
													DWORD64 length,
													PVOID pAddress )
{
    // Format appropriately (assuming it's a 1, 2, or 4 bytes (!!!)
    if ( length == 1 )
	{
        sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%X>", *(PBYTE)pAddress );
	}
    else if ( length == 2 )
	{
        sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%X>", *(PWORD)pAddress );
	}
    else if ( length == 4 )
    {
        if ( basicType == btFloat )
        {
            sprintf_s ( pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%f>", *(PFLOAT)pAddress);
        }
        else if ( basicType == btChar )
        {
            if ( ! IsBadStringPtrA( *(PSTR*)pAddress, 32) )
            {
                sprintf_s (	pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = \"%.31s\"", (LPSTR) ( *(PDWORD)pAddress )  );
            }
            else
			{
                sprintf_s (	pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%X>", *(PDWORD)pAddress );
			}
        }
        else
		{
            sprintf_s (	pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%X>", *(PDWORD)pAddress);
		}
    }
    else if ( length == 8 )
    {
        if ( basicType == btFloat )
        {
            sprintf_s (	pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%lf>", *(double *)pAddress );
        }
        else
		{
            sprintf_s (	pszCurrBuffer + strlen(pszCurrBuffer), cbCurrBuffer - strlen(pszCurrBuffer), " = <%I64X>", *(DWORD64*)pAddress );
		}
    }

    return pszCurrBuffer;
}

//
//====================================================================================
//
//
//
//====================================================================================
BasicType CExceptErrorHandler::EEHGetBasicType( DWORD typeIndex, DWORD64 modBase )
{
    BasicType basicType;

	if ( pfSymGetTypeInfo == NULL )
	{
	    return btNoType;
	}

    if ( (pfSymGetTypeInfo)( m_hProcess, modBase, typeIndex,
                        TI_GET_BASETYPE, &basicType ) )
    {
        return basicType;
    }

    // Get the real "TypeId" of the child.  We need this for the
    // SymGetTypeInfo( TI_GET_TYPEID ) call below.
    DWORD typeId;
    if ( (pfSymGetTypeInfo) (m_hProcess,modBase, typeIndex, TI_GET_TYPEID, &typeId))
    {
        if ( ( pfSymGetTypeInfo )( m_hProcess, modBase, typeId, TI_GET_BASETYPE,
                            &basicType ) )
        {
            return basicType;
        }
    }

    return btNoType;
}

//
//====================================================================================
//
//		Helper function that writes to the report file, and allows the user to use 
//		printf style formating                                                     
//
//====================================================================================
size_t __cdecl CExceptErrorHandler::EEHPrintCrash(const char * format, ...)
{
	if ( m_hCrashFile != NULL )
	{
		size_t	retValue;
		DWORD	dwValue;
		DWORD	cbWritten;

		ZeroMemory ( m_szBuffer, sizeof(m_szBuffer) );

		va_list	argptr;
         
		va_start( argptr, format );
		retValue = vsprintf_s ( m_szBuffer, sizeof ( m_szBuffer ), format, argptr );
		va_end( argptr );

		dwValue = ( DWORD ) retValue * sizeof(char);
		WriteFile ( m_hCrashFile, m_szBuffer, dwValue, &cbWritten, 0 );
		if ( EEHCrashToStderr )
		{
			fputs ( m_szBuffer, stderr );
		}
		return retValue;
	}

	return -1;
}

//
//====================================================================================
//
//		Helper function that writes to the report file, and allows the user to use 
//		printf style formating                                                     
//
//====================================================================================
size_t __cdecl CExceptErrorHandler::EEHPrintDebug(const char * format, ...)
{
	if ( m_hDebugFile != NULL || m_hCrashFile != NULL )
	{
		size_t	retValue;
		DWORD	dwValue;
		DWORD	cbWritten;
		ZeroMemory ( m_szBuffer, sizeof(m_szBuffer) );
		va_list	argptr;
         
		va_start( argptr, format );
		retValue = vsprintf_s ( m_szBuffer, sizeof ( m_szBuffer ), format, argptr );
		va_end( argptr );

		dwValue = ( DWORD ) retValue * sizeof(char);

		if ( m_hDebugFile != NULL )
		{
			WriteFile ( m_hDebugFile, m_szBuffer, dwValue, &cbWritten, 0 );

			return retValue;
		}
#if 0
		else if ( m_hCrashFile != NULL ) 
		{
			WriteFile ( m_hCrashFile, m_szBuffer, dwValue, &cbWritten, 0 );

			return retValue;
		}
#endif
	}

	return -1;
}

//
//====================================================================================
//
//		Get Filename
//
//====================================================================================
char * CExceptErrorHandler::EEHGetFilename(char * pName)
{
	char	*pReturn;

	pReturn = pName;

	if ( pName )
	{
		while ( *pName )
		{
			if ( *pName == ':' )
			{
				pReturn = pName + 1;
			}

			if ( *pName == '\\' )
			{
				pReturn = pName + 1;
			}
			pName++;
		}
	}

	return pReturn;
}

//
//====================================================================================
//
//====================================================================================
void CExceptErrorHandler::EEHGetTempDirectory(char * pName, size_t iSize)
{
	size_t iEnv = iSize;
	ZeroMemory(pName, iSize);
	getenv_s ( &iEnv, pName, iSize, "TEMP" );

}

//
//====================================================================================
//
//		Get Filename
//
//====================================================================================
void CExceptErrorHandler::EEHGetDirectory(char * pName, size_t iSize)
{
	char	*pReturn;

	pReturn = pName;

	if ( pName )
	{
		if ( strlen(pName) > 0 )
		{
			for ( int i = (int) strlen(pName) - 1; i >= 0; i-- )
			{
				if ( pName [i] == ':' )
				{
					pName [i+1] = 0;
					return ;
				}

				if ( pName [i] == '\\' )
				{
					pName [i] = 0;
					return ;
				}
			}
		}
	}

	//
	EEHGetTempDirectory ( pName, iSize );

	return ;
}

//
//====================================================================================
//	Accessible Functions
//====================================================================================

//
//====================================================================================
//
//====================================================================================
void InvokeExceptionErrorHandler( )
{
	if ( handler == NULL )
	{
		CExceptErrorHandler	*handler = new CExceptErrorHandler();
	}
}

//
//====================================================================================
//
//====================================================================================
void SetCrashToStderr( bool toStdErr )
{
	EEHCrashToStderr = toStdErr;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL TreatCrashOptionsW(int iArgCount, WCHAR* pArgValues[], const WCHAR* pOption, int &iX )
{
	if ( *pOption == L'-' || *pOption == L'/' )
	{
		pOption++;
	}

	if ( *pOption == L'-' )
	{
		pOption++;
	}

	if ( _wcsicmp ( pOption, L"handlecrash" ) == 0 )
	{
		InvokeExceptionErrorHandler();
		return TRUE;
	}
	else if ( _wcsicmp ( pOption, L"crashtostd" ) == 0 )
	{
		EEHCrashToStderr = true;
		return TRUE;
	}
	else if ( _wcsicmp ( pOption, L"verbose" ) == 0 )
	{
		VerboseMode = true;
		return TRUE;
	}
	else if ( _wcsicmp ( pOption, L"debug" ) == 0 )
	{
		DebugMode = true;
		return TRUE;
	}
	else if ( _wcsicmp ( pOption, L"trace" ) == 0 )
	{
		TraceMode = true;
		return TRUE;
	}

	return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL TreatCrashOptionsA(int iArgCount, char* pArgValues[], const char* pOption, int &iX, bool bTwo)
{
	if ( *pOption == '-' || *pOption == '/' )
	{
		pOption++;
	}

	if ( *pOption == '-' )
	{
		pOption++;
	}

	if ( _stricmp ( pOption, "handlecrash" ) == 0 )
	{
		InvokeExceptionErrorHandler();
		return TRUE;
	}
	else if ( _stricmp ( pOption, "crashtostd" ) == 0 )
	{
		EEHCrashToStderr = true;
		return TRUE;
	}
	else if ( _stricmp ( pOption, "verbose" ) == 0 )
	{
		VerboseMode = true;
		return TRUE;
	}
	else if ( _stricmp ( pOption, "debug" ) == 0 )
	{
		DebugMode = true;
		return TRUE;
	}
	else if ( __stricmpLA ( pOption, "trace" ) == 0 )
	{
		TraceMode = true;
		return TRUE;
	}
	return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void PrintCrashHelpW ( int iWidth, const WCHAR *programName, int iArgCount, WCHAR* pArgValues[], bool bLong, bool bTwo )
{
	PrintHelpLineW ( );
	if ( bTwo )
	{
		PrintHelpLineW ( iWidth, L"--handlecrash", L"Handle Program Crashes" );
		if ( bLong )
		{
			PrintHelpLineW ( iWidth, L"", L" use also --verbose, --debug, --trace, --crashtostd" );
		}
	}
	else
	{
		PrintHelpLineW ( iWidth, L"-handlecrash", L"Handle Program Crashes" );
		if ( bLong )
		{
			PrintHelpLineW ( iWidth, L"", L" use also -verbose, -debug, -trace, -crashtostd" );
		}
	}
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void PrintCrashHelpA ( int iWidth, const char *programName, int iArgCount, char* pArgValues[], bool bLong, bool bTwo )
{
	PrintHelpLineA ( );
	if ( bTwo )
	{
		PrintHelpLineA ( iWidth, "--handlecrash", "Handle Program Crashes" );
		if ( bLong )
		{
			PrintHelpLineA ( iWidth, "", " use also --verbose, --debug, -trace, --crashtostd" );
		}
	}
	else
	{
		PrintHelpLineA ( iWidth, "-handlecrash", "Handle Program Crashes" );
		if ( bLong )
		{
			PrintHelpLineA ( iWidth, "", " use also -verbose, -debug, -trace, -crashtostd" );
		}
	}
}

//
/////////////////////////////////////////////////////////////////////////////
//	Global Variables for InvalidParameterHandler
/////////////////////////////////////////////////////////////////////////////
bool InvalidParameterFound								= false;
_invalid_parameter_handler OldInvalidParameterHandler	= NULL;

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
void InvalidParameterHandler ( const WCHAR *expression, const WCHAR *function, const WCHAR *file, unsigned int line, uintptr_t pReserved )
{
	InvalidParameterFound = true;
#if 0
	PrintStderr ( L"Expression :%s - Function : %s - File : %s - Line : %d\n", expression, function, file, line );
#endif // 0
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
_invalid_parameter_handler SetInvalidParameterHandler ( _invalid_parameter_handler newInvalidParameterHandler )
{
	InvalidParameterFound = false;

	if ( newInvalidParameterHandler != NULL )
	{
		OldInvalidParameterHandler = _set_invalid_parameter_handler ( newInvalidParameterHandler );
	}
	else
	{
		OldInvalidParameterHandler = _set_invalid_parameter_handler( InvalidParameterHandler );
	}

	return OldInvalidParameterHandler;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void RestoreInvalidParameterHandler ( _invalid_parameter_handler oldInvalidParameterHandler )
{
	InvalidParameterFound = false;

	if ( oldInvalidParameterHandler != NULL )
	{
		_set_invalid_parameter_handler ( oldInvalidParameterHandler );
	}
	else if ( OldInvalidParameterHandler != NULL )
	{
		_set_invalid_parameter_handler ( OldInvalidParameterHandler );
	}
	OldInvalidParameterHandler = NULL;
}
