/*
 ************************************************************************************
 *
 *		File:			ExceptionErrorHandler.h
 *		Class:			CExceptErrorHandler
 *
 ************************************************************************************
 *		Description:
 *
 ************************************************************************************
 *		Notes:
 *
 *		20/12/2008
 *
 ************************************************************************************
 *
 *		E/Mail: ian.macallan@gmail.com
 *		(c) 2003 - 2023 / Macallan 
 *		
 ************************************************************************************
 *
 *
 *		Matt Pietrek
 *		MSDN Magazine, 2002
 *
 *		Litely adapted by Macallan.
 *		FILE: MacallanExceptionHandler.h
 *
 ************************************************************************************
 */
#pragma once

#ifdef EXCEPTION_ERROR_HANDLER

#include <dbghelp.h>

//=============================================================================
//
//
//
//=============================================================================
enum BasicType  // Stolen from CVCONST.H in the DIA 2.0 SDK
{
    btNoType = 0,
    btVoid = 1,
    btChar = 2,
    btWChar = 3,
    btInt = 6,
    btUInt = 7,
    btFloat = 8,
    btBCD = 9,
    btBool = 10,
    btLong = 13,
    btULong = 14,
    btCurrency = 25,
    btDate = 26,
    btVariant = 27,
    btComplex = 28,
    btBit = 29,
    btBSTR = 30,
    btHresult = 31
};

//
//====================================================================================
//
//		Global Function Pointers
//
//====================================================================================

typedef BOOL  ( __stdcall * typeSymGetTypeInfo )(
    IN  HANDLE          hProcess,
    IN  DWORD64         ModBase,
    IN  ULONG           TypeId,
    IN  IMAGEHLP_SYMBOL_TYPE_INFO GetType,
    OUT PVOID           pInfo
    );

typedef DWORD ( __stdcall * typeSymSetOptions ) (
    IN DWORD   SymOptions
    );

typedef DWORD ( __stdcall * typeSymGetOptions ) ( );

typedef BOOL ( __stdcall * typeSymInitialize ) (
    IN HANDLE   hProcess,
    IN PCSTR    UserSearchPath,
    IN BOOL     fInvadeProcess
    );

typedef BOOL ( __stdcall * typeSymCleanup ) (
    IN HANDLE hProcess
    );

typedef BOOL ( __stdcall * typeSymEnumSymbols ) (
    IN HANDLE                       hProcess,
    IN ULONG64                      BaseOfDll,
    IN PCSTR                        Mask,
    IN PSYM_ENUMERATESYMBOLS_CALLBACK    EnumSymbolsCallback,
    IN PVOID                        UserContext
    );

typedef BOOL ( __stdcall * typeSymSetContext ) (
    HANDLE hProcess,
    PIMAGEHLP_STACK_FRAME StackFrame,
    PIMAGEHLP_CONTEXT Context
    );

typedef BOOL ( __stdcall * typeSymFromAddr )(
    IN  HANDLE              hProcess,
    IN  DWORD64             Address,
    OUT PDWORD64            Displacement,
    IN OUT PSYMBOL_INFO     Symbol
    );

typedef BOOL ( __stdcall * typeSymGetLineFromAddr )(
    IN  HANDLE                hProcess,
    IN  DWORD                 dwAddr,
    OUT PDWORD                pdwDisplacement,
    OUT PIMAGEHLP_LINE        Line
    );

typedef BOOL ( __stdcall * typeSymGetLineFromAddr64 )(
    IN  HANDLE                hProcess,
    IN  DWORD64               dwAddr,
    OUT PDWORD                pdwDisplacement,
    OUT PIMAGEHLP_LINE64      Line
    );

typedef BOOL ( __stdcall * typeStackWalk )(
    DWORD                             MachineType,
    HANDLE                            hProcess,
    HANDLE                            hThread,
    LPSTACKFRAME                      StackFrame,
    PVOID                             ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE      ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE    FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE          GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE        TranslateAddress
    );

typedef BOOL ( __stdcall * typeStackWalk64 )(
    DWORD								MachineType,
    HANDLE								hProcess,
    HANDLE								hThread,
    LPSTACKFRAME64						StackFrame,
    PVOID								ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE		ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE		FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE			GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE			TranslateAddress
    );

typedef PVOID ( __stdcall * typeSymFunctionTableAccess )(
    HANDLE		hProcess,
    DWORD		AddrBase
    );

typedef PVOID ( __stdcall * typeSymFunctionTableAccess64 )(
    HANDLE		hProcess,
    DWORD64		AddrBase
    );

typedef DWORD ( __stdcall * typeSymGetModuleBase )(
    IN  HANDLE				hProcess,
    IN  DWORD				dwAddr
    );

typedef DWORD64  ( __stdcall * typeSymGetModuleBase64 )(
    IN  HANDLE			hProcess,
    IN  DWORD64			dwAddr
    );


typedef USHORT ( __stdcall * typeRtlCaptureStackBackTrace )(
	IN ULONG FramesToSkip,
	IN ULONG FramesToCapture,
	PVOID *BackTrace,
	PULONG BackTraceHash
	);

//
//=============================================================================
//
//	The Macallan Exception Handler.
//
//=============================================================================

//!	MacallanExceptionHandler
class CExceptErrorHandler
{
    public:
    
		CExceptErrorHandler ( );
		virtual ~CExceptErrorHandler( );
	    
		static void EEHSetLogFileName( char *pszLogFileName, size_t cbSize );
		static char * EEHGetFilename(char * pName);
		static void EEHGetDirectory(char * pName, size_t iSize);
		static void EEHGetTempDirectory(char * pName, size_t iSize);

		// entry point where control comes on an unhandled exception
		static LONG WINAPI EEHMacallanUnhandledExceptionFilter( PEXCEPTION_POINTERS pExceptionInfo );

		static void EEHInvalidParameterHandler (	const wchar_t * expression, const wchar_t * function, 
													const wchar_t * file, unsigned int line, uintptr_t pReserved );

    private:

		// where report info is extracted and generated 
		static void EEHGenerateExceptionReport( PEXCEPTION_POINTERS pExceptionInfo );

		// Helper functions
		static char * EEHGetExceptionString ( DWORD dwCode );
		static BOOL EEHGetLogicalAddress (	PVOID addr, char * szModule, DWORD len,
											DWORD& section, DWORD& offset );

		static void EEHWriteStackDetails( PCONTEXT pContext, bool bWriteVariables );

		static BOOL CALLBACK EEHEnumerateSymbolsCallback ( PSYMBOL_INFO, ULONG, PVOID );

		static bool EEHFormatSymbolValue( PSYMBOL_INFO, STACKFRAME *, char * pszBuffer, unsigned cbBuffer );

		static char * EEHDumpTypeIndex ( char *, size_t, DWORD64, DWORD, unsigned, DWORD_PTR, bool & );

		static char * EEHFormatOutputValue (	char * pszCurrBuffer, size_t cbCurrBuffer, 
												BasicType basicType, DWORD64 length,
												PVOID pAddress );
	    
		static BasicType EEHGetBasicType ( DWORD typeIndex, DWORD64 modBase );

		static BOOL EEHEnableCrashingOnCrashes ();

		static size_t __cdecl EEHPrintCrash ( const char * format, ... );
		static size_t __cdecl EEHPrintDebug ( const char * format, ... );

		static void EEHWriteStackTrace ();

		static const char *EEHGetLastErrorText ( );

		static void EEHInitializeSymbols();

		static void EEHSetInvalidParameterHandler ();

		static void EEHOpenDebugFile();
		static void EEHCloseDebugFile();

		static void EEHOpenCrashFile();
		static void EEHCloseCrashFile();

		//
		//		External functions
		static	typeSymInitialize				pfSymInitialize;
		static	typeSymGetTypeInfo				pfSymGetTypeInfo;
		static	typeSymSetOptions				pfSymSetOptions;
		static	typeSymGetOptions				pfSymGetOptions;
		static	typeSymCleanup					pfSymCleanup;
		static	typeSymEnumSymbols				pfSymEnumSymbols;
		static	typeSymSetContext				pfSymSetContext;
		static	typeSymFromAddr					pfSymFromAddr;
		static	typeSymGetLineFromAddr			pfSymGetLineFromAddr;
		static	typeSymGetLineFromAddr64		pfSymGetLineFromAddr64;
		static	typeStackWalk					pfStackWalk;
		static	typeStackWalk64					pfStackWalk64;
		static	typeSymFunctionTableAccess		pfSymFunctionTableAccess;
		static	typeSymFunctionTableAccess64	pfSymFunctionTableAccess64;
		static	typeSymGetModuleBase			pfSymGetModuleBase;
		static	typeSymGetModuleBase64			pfSymGetModuleBase64;

		static	typeRtlCaptureStackBackTrace	pfRtlCaptureStackBackTrace;

		// Variables used by the class
		static	char							m_szDebugFileName[MAX_PATH+1];
		static	char							m_szCrashFileName[MAX_PATH+1];
		static	char							m_szDirectory[MAX_PATH+1];
		static	char							m_szSearchPath[MAX_PATH+1];
		static	char							m_szFilename[MAX_PATH+1];
		static	char							m_szModule[MAX_PATH+1];
		static	char							m_szFaultingModule[MAX_PATH+1];
		static	char							m_szBuffer[MAX_PATH*4+1];
		static	WCHAR							m_szwBuffer[MAX_PATH*4+1];
		static	char							m_szSymbolBuffer[MAX_PATH*8+1];
		static	LPTOP_LEVEL_EXCEPTION_FILTER	m_previousFilter;
		static	HANDLE							m_hCrashFile;
		static	HANDLE							m_hDebugFile;
		static	HANDLE							m_hProcess;
		static	char							m_szErrorText[MAX_PATH*2+1];
#define	SYMBOL_NAME_LEN		256
		static  char							m_SymbolInfo [ sizeof(SYMBOL_INFO) + SYMBOL_NAME_LEN * sizeof( char )];
		static	SYMBOL_INFO						*m_pSymbolInfo;
		static _invalid_parameter_handler		m__invalid_parameter_handler;

};

#endif // EXCEPTION_ERROR_HANDLER

//
//	External Functions
extern void InvokeExceptionErrorHandler( );
extern void SetCrashToStderr( bool toStdErr );

extern BOOL TreatCrashOptionsW(int iArgCount, WCHAR* pArgValues[], const WCHAR* pOption, int &iX);
extern BOOL TreatCrashOptionsA(int iArgCount, char* pArgValues[], const char* pOption, int &iX);

extern void PrintCrashHelpW ( int iWidth, const WCHAR *programName, int iArgCount, WCHAR* pArgValues[], bool bLong, bool bTwo = false );
extern void PrintCrashHelpA ( int iWidth, const char *programName, int iArgCount, char* pArgValues[], bool bLong, bool bTwo = false );

//
extern bool InvalidParameterFound;
extern	_invalid_parameter_handler SetInvalidParameterHandler ( _invalid_parameter_handler newInvalidParameterHandler = NULL );
extern	void RestoreInvalidParameterHandler ( _invalid_parameter_handler oldInvalidParameterHandler = NULL );
extern	_invalid_parameter_handler OldInvalidParameterHandler;

//
#ifdef UNICODE
#define	TreatCrashOptions			TreatCrashOptionsW
#define	PrintCrashHelp				PrintCrashHelpW
#else
#define	TreatCrashOptions			TreatCrashOptionsA
#define	PrintCrashHelp				PrintCrashHelpA
#endif
