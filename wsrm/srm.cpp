//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <Windows.h>
#include <winioctl.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <gdiplus.h>

#include "AutomaticVersionHeader.h"

#include "srm.h"
#include "DiskFileInfos.h"

#include "PrintRoutine.h"
#include "strstr.h"
#include "ExceptionErrorHandler.h"

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
extern bool FillOneDisk ( const char *pDisk );

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void initBuffers ( int index );

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#define LEN_PATHNAME        4096
#define IMG_WIDTH           1920
#define IMG_HEIGHT          1080

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
static  UINT        CodePage                = CP_ACP;
static  WCHAR       LocaleString [ LEN_PATHNAME ];

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#define TIMES_NEW_ROMAN     "Times New Roman"
#define TIMES_NEW_ROMAN_W   L"Times New Roman"

static WCHAR    szCurrentDirectory [ LEN_PATHNAME ];
static WCHAR    szPrependFilename [ LEN_PATHNAME ];

static WCHAR    szFillWithFile [ LEN_PATHNAME ];
static WCHAR    szJpegext [ LEN_PATHNAME ];
static WCHAR    szCreateJpgFile [ LEN_PATHNAME ];

#define GDIPLUS_ENCODER     L"image/jpeg"

#define WSRM_FILL_FILENAME  L"WSRM_FILL_FILENAME"
#define WSRM_JPEG_TEXT      L"WSRM_JPEG_TEXT"
#define WSRM_INTERACTIVE    L"WSRM_INTERACTIVE"
#define WSRM_NO_BANNER      L"WSRM_NO_BANNER"

#define YES                 L"YES"
#define NO                  L"NO"
static WCHAR    szVariable [ LEN_PATHNAME ];

//
#define FILE_SUFFIX         L".bin"
#define MIN_FILENAME_LEN    48
static  WCHAR               randomization [] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz_";

//
#define NB_KILOS            512
#define ONE_KILO            1024

//
static int      RandomCount = 0;

//
//  First Buffer will be all zero
#define MAX_BUFFERS         10
#define MAX_BUFFER_LENGTH   (ONE_KILO*NB_KILOS)
static size_t   iBufferMaxLength;
static size_t   iBufferCurLength;
static BYTE     **pBufferRandom = NULL;
static int      iBufferIndex = 0;

//
#define LEN_PATHNAME_TABLE  256
static int      iPathnameCount = 0;
static int      iPathnameMax = LEN_PATHNAME_TABLE;
static WCHAR    *pPathnameTable [ LEN_PATHNAME_TABLE ];

//
static int      iExcludeCount = 0;
static int      iExcludeMax = LEN_PATHNAME_TABLE;
static WCHAR    *pExcludeTable [ LEN_PATHNAME_TABLE ];

static WCHAR    szDriveVolume [ LEN_PATHNAME ];
static WCHAR    szErrorText [ LEN_PATHNAME ];

//
enum Actions
{
    Action_None,
    Action_DeletePath,
    Action_Copy,
    Action_FileInfo,
    Action_LocateFile,
    Action_FillDisk,
    Action_FillName,
    Action_ShowAllFiles,
    Action_ShowDirectories,
    Action_ShowDeletedFiles,
    Action_ShowDeletedFilesXE,

};

//
static  Actions Action = Action_None;

//
static BOOL     bDeleteOnly = FALSE;

//
static BOOL     bRoundToBlock = FALSE;
static BOOL     bEncryptedWriteOver = FALSE;

static BOOL     bFillWithFile = FALSE;
static BOOL     bFillWithFileEnv = FALSE;

static int      iCountItems = 0;
static int      bCountItems = FALSE;

static BOOL     bFilter = FALSE;
static WCHAR    szFilter [ LEN_PATHNAME ];

static BOOL     bGenerateTableEachWrite = FALSE;

static BOOL     bInteractive = FALSE;
static BOOL     bInteractiveEnv = FALSE;

static BOOL     bJpegText = FALSE;
static BOOL     bJpegTextEnv = FALSE;

static BOOL     bCreateJpgFile = FALSE;

static BOOL     bNoBanner = FALSE;
static BOOL     bNoBannerEnv = FALSE;

static BOOL     bNoMatch;
static WCHAR    szNoMatch [ LEN_PATHNAME ];

static ULONG    iFileNumber = 0;

static BOOL     bCheckCluster = FALSE;
static BOOL     bGetDiskVolumeDone = FALSE;

static BOOL     bOverwrite = FALSE;

static BOOL     bOutput = FALSE;
static WCHAR    szOutputFile [ LEN_PATHNAME ];

static BOOL     bProbeMode = FALSE;
static BOOL     bRecursive = FALSE;
static BOOL     bSubDirectory = FALSE;

static BOOL     bYesToQuestion = FALSE;
static BOOL     bZeroBuffer = FALSE;
static int      iPasses = 1;

static bool     isAborted = false;

static BOOL     bDiskInfoDone = FALSE;

static DWORD    dwSectorPerCluster      = NULL;
static DWORD    dwBytePerSector         = NULL;
static DWORD    dwNumberOfFreeCluster   = NULL;
static DWORD    dwTotalNumberOfCluster  = NULL;

//
static PVOLUME_DISK_INFO        lpVolumeLcnInfo = NULL;
static int                      iFileLcnMax = 64;
static int                      iFileLcnSize = 0;
static PFILE_LCN                lpFileLcnBefore = NULL;
static PFILE_LCN                lpFileLcnAfter = NULL;

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
bool isAbortedByCtrlC()
{
    return isAborted;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *GetLastErrorText ()
{
    DWORD dwLastError = GetLastError();
    ZeroMemory ( szErrorText, sizeof (szErrorText) );

    switch ( dwLastError )
    {
    case ERROR_FILE_NOT_FOUND :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"File Not Found" );
        break;
    case ERROR_PATH_NOT_FOUND :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Path Not Found" );
        break;
    case ERROR_ACCESS_DENIED :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Access Denied" );
        break;
    case ERROR_INVALID_ACCESS :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Invalid Access" );
        break;
    case ERROR_WRITE_PROTECT :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Write Protect" );
        break;
    case ERROR_SHARING_VIOLATION :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Sharing Violation" );
        break;
    case  ERROR_INVALID_USER_BUFFER :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Invalid User Buffer" );
        break;
    case  ERROR_NOT_ENOUGH_MEMORY :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Not Enough Memory" );
        break;
    case  ERROR_OPERATION_ABORTED :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Operation Aborted" );
        break;
    case  ERROR_NOT_ENOUGH_QUOTA :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Not Enough Quota" );
        break;
    case  ERROR_IO_PENDING :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"IO Pending" );
        break;
    case ERROR_INVALID_PARAMETER :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Invalid Parameter" );
        break;
    case ERROR_INVALID_NAME :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Invalid Name" );
        break;
    case ERROR_FILE_EXISTS :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"File Exists" );
        break;
    case FVE_E_LOCKED_VOLUME :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Locked Volume" );
        break;
    default :
        wcscpy_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), L"Unknown Error" );
        break;
    }

    return szErrorText;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
BOOL endsWith ( WCHAR *pString, WCHAR c )
{
    if ( pString != NULL && wcslen ( pString ) > 0 )
    {
        if ( pString [ wcslen ( pString ) - 1 ] == c )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
BOOL startsWith ( WCHAR *pString, WCHAR *pStart )
{
    if ( pString != NULL && pStart != NULL && wcslen(pStart) )
    {
        if ( _wcsnicmp ( pString, pStart, wcslen(pStart) ) == 0 )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
BOOL isDirectory ( WCHAR *lpFileName )
{
    DWORD dwAttrivutes = GetFileAttributes ( lpFileName );
    if ( dwAttrivutes != INVALID_FILE_ATTRIBUTES )
    {
        if ( ( dwAttrivutes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
        {
            return TRUE;
        }
    }
    else
    {
        PrintStderrW ( L"Error - GetFileAttributes Fails - Cause : 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
    }

    return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if(size == 0)
    {
        return -1;  // Failure
    }

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL)
    {
        return -1;  // Failure
    }

    GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j)
    {
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);

    PrintStderrW ( L"Error - No Encoder Found For %s\n", format );

    return -1;  // Failure
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL CreateImage ( int width, int height, WCHAR *pText )
{
    //
    size_t iStringLength = wcslen (pText);
    if ( iStringLength == 0 )
    {
        PrintStderrW ( L"Error - JPEG Text Is Empty\n" );

        bJpegText = FALSE;
        bJpegTextEnv = FALSE;
        return FALSE;
    }


    //
    BOOL bResult = TRUE;
    Gdiplus::Status gdiStatus;

    BYTE* pImageData = new BYTE[width * 4 * height];

    Gdiplus::GdiplusStartupInput    gdiplusStartupInput;
    ULONG_PTR                       gdiplusToken;

    // Initialize GDI+.
    gdiStatus = Gdiplus::GdiplusStartup ( &gdiplusToken, &gdiplusStartupInput, NULL );
    if ( gdiStatus == Gdiplus::Ok )
    {
        // gdiplus
        Gdiplus::Bitmap bitmap ( width, height, 4 * width, PixelFormat32bppARGB, pImageData );
        Gdiplus::Graphics *graphic = Gdiplus::Graphics::FromImage(&bitmap);
        
        //  Alpha / RGB
        gdiStatus = graphic->Clear(Gdiplus::Color(255, 255, 255, 255));
        if ( gdiStatus != Gdiplus::Ok )
        {
            PrintStderrW ( L"Error - Gdiplus::Clear Fails : %d\n", gdiStatus );
        }

        //
        Gdiplus::SolidBrush             gdiBrush(Gdiplus::Color(255, 32, 32, 32));
        Gdiplus::FontFamily             gdiFontFamily(TIMES_NEW_ROMAN_W);
        Gdiplus::RectF                  gdiRectF ( 0.0f, 0.0f, 1.0f * width, 1.0f * height );
        Gdiplus::RectF                  gdiRectFResult;

        //
        Gdiplus::StringFormat stringFormat = new Gdiplus::StringFormat();
        gdiStatus = stringFormat.SetAlignment ( Gdiplus::StringAlignment::StringAlignmentCenter );
        gdiStatus = stringFormat.SetLineAlignment ( Gdiplus::StringAlignment::StringAlignmentCenter );

        //
        //  Compute Best Size
        BOOL bOutOfSpace = FALSE;
        int fontSize = 16;
        int fontSizeGood = 16;
        int fontSizeStep = 16;
        while ( ! bOutOfSpace )
        {
            Gdiplus::Font   gdiFontTest(&gdiFontFamily, (Gdiplus::REAL) fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            gdiStatus = graphic->MeasureString ( pText, -1, &gdiFontTest, gdiRectF, &gdiRectFResult );
            if ( gdiStatus != Gdiplus::Ok )
            {
                PrintStderrW ( L"Error - Gdiplus::MeasureString Fails : %d\n", gdiStatus );
                break;
            }

            if ( gdiRectFResult.GetRight() < IMG_WIDTH && gdiRectFResult.GetBottom() < IMG_HEIGHT )
            {
                fontSizeGood = fontSize;
                fontSize += fontSizeStep;
            }
            else
            {
                fontSize = fontSizeGood;
                bOutOfSpace = TRUE;
            }
        }

        //
        Gdiplus::Font   gdiFontFit(&gdiFontFamily, (Gdiplus::REAL) fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        gdiStatus = graphic->DrawString (pText, -1, &gdiFontFit, gdiRectF, &stringFormat, &gdiBrush );
        if ( gdiStatus != Gdiplus::Ok )
        {
            PrintStderrW ( L"Error - Gdiplus::DrawString Fails : %d\n", gdiStatus );
        }

        //
        CLSID jpgClsid;
        int iResult = GetEncoderClsid(GDIPLUS_ENCODER, &jpgClsid);
        if ( iResult >= 0 )
        {
            if ( bCreateJpgFile && wcslen ( szCreateJpgFile ) > 0 )
            {
                gdiStatus = bitmap.Save(szCreateJpgFile, &jpgClsid);
                if ( gdiStatus != Gdiplus::Ok )
                {
                    PrintStderrW ( L"Error - Bitmap::Save To File %s Fails : %d\n", szCreateJpgFile, gdiStatus );
                }
            }

            IStream *jpegStream;
            HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &jpegStream);
            gdiStatus = bitmap.Save ( jpegStream, &jpgClsid);
            if ( gdiStatus != Gdiplus::Ok )
            {
                PrintStderrW ( L"Error - Bitmap::Save To Stream Fails : %d\n", gdiStatus );
            }

            //get memory handle associated with jpegStream
            HGLOBAL hGlobal = NULL;
            GetHGlobalFromStream(jpegStream, &hGlobal);
            size_t streamBufferSize = GlobalSize(hGlobal);
            if ( streamBufferSize != 0 )
            {
                size_t bufferToCopy = min ( MAX_BUFFER_LENGTH, streamBufferSize );

                //  Lock / Copy / Unlock
                LPVOID streamBytesPointer = GlobalLock(hGlobal);
                if ( streamBytesPointer )
                {
                    memcpy ( pBufferRandom [ 1 ], streamBytesPointer, bufferToCopy );
                    GlobalUnlock(hGlobal);

                    //
                    //  Then Copy Again
                    DWORD dwUsed        = ( DWORD ) bufferToCopy;
                    while ( ( iBufferMaxLength - dwUsed ) > bufferToCopy )
                    {
                        BYTE *pBuffer = pBufferRandom [ 1 ];
                        memcpy ( pBuffer + dwUsed, pBuffer, bufferToCopy );
                        dwUsed += ( DWORD ) bufferToCopy;
                    }

                    //
                    iBufferCurLength = dwUsed;

                    //
                }
                else
                {
                    PrintStderrW ( L"Error - Stream Pointer Is Zero\n" );

                    bJpegText = FALSE;
                    bJpegTextEnv = FALSE;
                    bResult = FALSE;
                }
            }
            else
            {
                PrintStderrW ( L"Error - Stream Size Is Zero\n" );

                bJpegText = FALSE;
                bJpegTextEnv = FALSE;
                bResult = FALSE;
            }
        }
        else
        {
            PrintStderrW ( L"Error - No Encoder Found For %s\n", GDIPLUS_ENCODER );
            bJpegText = FALSE;
            bJpegTextEnv = FALSE;
            bResult = FALSE;
        }

        //
        if ( graphic != NULL )
        {
            delete graphic;
        }
    }
    else
    {
        PrintStderrW ( L"Error - Gdiplus::GdiplusStartup Fails : %d\n", gdiStatus );

        bJpegText = FALSE;
        bJpegTextEnv = FALSE;
        bResult = FALSE;
    }

    //
    if ( pImageData != NULL )
    {
        delete[] pImageData;
    }

    //
    return bResult;

}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR AskConfirmQuestion ( WCHAR *pQuestion, WCHAR *pExtra, WCHAR *pResponses, WCHAR *pHelp )
{
    WCHAR *pFound = NULL;
    WCHAR response = L'\0';
    do
    {
        fflush ( stdout );
        PrintNormalW ( L"%s %s [%s] ? ", pQuestion, pExtra, pHelp );
        response = L'\0';
        fflush ( stdin );
        response = _getwch();
        if ( response == 0x03 )
        {
            isAborted = TRUE;
            PrintNormalW ( L"CTRL+C\n" );
        }
        else
        {
            PrintNormalW ( L"%c\n", response );
        }
        fflush ( stdout );
        pFound = wcschr ( pResponses, toupper ( response ) );
    }
    while ( pFound == NULL && ! isAborted );

    return toupper( response );
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL readFillFile ( )
{
    LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;
    DWORD   dwDesiredAccess = FILE_GENERIC_READ;
    DWORD   dwShareMode = 0;
    DWORD   dwCreationDisposition = OPEN_EXISTING;
    DWORD   dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    HANDLE  hTemplateFile = NULL;

    //
    //  We will hav to prepend '\\?\'
    wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
    wcscat_s ( szPrependFilename, LEN_PATHNAME, szFillWithFile );

    HANDLE hFile =
        CreateFile ( szPrependFilename, dwDesiredAccess, dwShareMode,
            lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
    if ( hFile != INVALID_HANDLE_VALUE )
    {
        DWORD   dwNumberOfBytesToRead   = ( DWORD) iBufferMaxLength;
        DWORD   dwNumberOfBytesRead     = 0;

        //
        //  Read All In One
        BOOL bRead = ReadFile (
                hFile,                              // open file  handle
                pBufferRandom [ 1 ],                // start of data to write
                dwNumberOfBytesToRead,              // number of bytes to read
                &dwNumberOfBytesRead,               // number of bytes that were read
                NULL );                             // no overlapped structure

        //
        //  End Of File
        if ( dwNumberOfBytesRead == 0 )
        {
            //
            CloseHandle(hFile);

            //
            //  Nothing read : dont use
            bFillWithFile = FALSE;
            bFillWithFileEnv = FALSE;

            return FALSE;
        }
        else if ( dwNumberOfBytesRead > 0 )
        {
            //
            //  Fill Buffer with the file Read
            DWORD dwUsed        = dwNumberOfBytesRead;
            while ( ( iBufferMaxLength - dwUsed ) > dwNumberOfBytesRead )
            {
                BYTE *pBuffer = pBufferRandom [ 1 ];
                memcpy ( pBuffer + dwUsed, pBuffer, dwNumberOfBytesRead );
                dwUsed += dwNumberOfBytesRead;
            }

            //
            iBufferCurLength = dwUsed;

            //
            CloseHandle(hFile);

            return TRUE;
        }

        //
        //  Error
        if ( ! bRead )
        {
            PrintStderrW ( L"Error - CreateFile File '%s' - Cause : 0x%lx - %s\n", szFillWithFile, GetLastError(), GetLastErrorText() );

            //
            CloseHandle(hFile);

            bFillWithFile = FALSE;
            bFillWithFileEnv = FALSE;

            return FALSE;
        }

        CloseHandle ( hFile );
    }
    else
    {
        PrintStderrW ( L"Error - CreateFile File '%s' - Cause : 0x%lx - %s\n", szFillWithFile, GetLastError(), GetLastErrorText() );

        bFillWithFile = FALSE;
        bFillWithFileEnv = FALSE;

        return FALSE;
    }

    //
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL changeAttributes ( WCHAR *pFullPathname, DWORD dwCurrentAttributes )
{
    if ( isAborted )
    {
        return FALSE;
    }

    //
    DWORD   dwFileAttributes = dwCurrentAttributes;
    if ( dwCurrentAttributes & FILE_ATTRIBUTE_READONLY  )
    {
        dwFileAttributes ^= FILE_ATTRIBUTE_READONLY;
    }

    if ( dwCurrentAttributes &  FILE_ATTRIBUTE_SYSTEM )
    {
        dwFileAttributes ^= FILE_ATTRIBUTE_SYSTEM;
    }

    if ( dwFileAttributes != dwCurrentAttributes )
    {
        //
        if ( bProbeMode )
        {
            PrintNormalW ( L"%s - Attributes Left Intact\n", pFullPathname );
            return TRUE;
        }

        BOOL bChanged = SetFileAttributes ( pFullPathname, dwFileAttributes );
        if ( bChanged )
        {
            PrintVerboseW ( L"%s - Attributes Removed\n", pFullPathname );
        }
        else
        {
            PrintStderrW ( L"Error - Changing Attributes for File '%s' - Cause : 0x%lx - %s\n", pFullPathname, GetLastError(), GetLastErrorText() );
            return FALSE;
        }
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void randomSeed ( )
{
    //
    unsigned int seed = (unsigned int) time(NULL);
    srand ( seed );
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void randomizeName ( WCHAR *pName )
{
    //
    size_t modulo = wcslen ( randomization);

    //
    while ( *pName != L'\0' )
    {
        if ( *pName == L' ' )
        {
            int random = rand() % modulo;
            *pName = randomization [ random ];
        }
        pName++;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL AskConfirmQuestionForPathname ( WCHAR *pFullPathname )
{
    if ( ( bInteractive || bInteractiveEnv ) && ! bYesToQuestion )
    {
        WCHAR response = AskConfirmQuestion ( L"Do you want to delete", pFullPathname, L"YNAQ", L"Y(es), N(o), A(ll), Q(uit)" );
        if ( isAborted )
        {
            return FALSE;
        }
        else if ( response == L'A' )
        {
            bYesToQuestion = TRUE;
        }
        else if ( response == L'N' )
        {
            return FALSE;
        }
        else if ( response == L'Q' )
        {
            isAborted = TRUE;
            return FALSE;
        }
        else if ( response != L'Y' )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL renameAndDelete ( WCHAR *pFullPathname, BOOL bDirectory )
{
    //
    //  Check for exclusion
    for ( int i = 0; i < iExcludeCount; i++ )
    {
        PrintTraceW ( L"Checking %s versus %s\n", FindFileNameW ( pFullPathname ), pExcludeTable [ i ] );
        if ( PathMatchSpec ( FindFileNameW ( pFullPathname ), pExcludeTable [ i ] ) )
        {
            PrintVerboseW ( L"Info : File %s is ignored (Excluded)\n", pFullPathname );
            return FALSE;
        }
    }

    //
    if ( isAborted )
    {
        return FALSE;
    }

    //  Then Remove
    if ( bDeleteOnly && ! bProbeMode  )
    {
        BOOL bDeleted = FALSE;
        if ( ! bDirectory )
        {
            //  We will hav to prepend '\\?\'
            wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
            wcscat_s ( szPrependFilename, LEN_PATHNAME, pFullPathname );

            bDeleted = DeleteFile ( szPrependFilename );
        }
        else
        {
            //  We will hav to prepend '\\?\'
            wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
            wcscat_s ( szPrependFilename, LEN_PATHNAME, pFullPathname );

            bDeleted = RemoveDirectory ( szPrependFilename );
        }

        //
        if ( ! bDeleted )
        {
            PrintStderrW ( L"Error - Deleting File '%s' - Cause : 0x%lx - %s\n", pFullPathname, GetLastError(), GetLastErrorText() );
        }
        else if ( ! QuietMode )
        {
            if ( ! bDirectory )
            {
                PrintNormalW ( L"%s - File Deleted\n", pFullPathname );
            }
            else
            {
                PrintNormalW ( L"%s - Directory Deleted\n", pFullPathname );
            }
        }

        //
        return bDeleted;
    }

    //
    BOOL bSuccess = TRUE;

    //
    //  Prepare Split Pathname
    WCHAR *pInputDrive = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputDrive, LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputDirectory = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputDirectory, LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputFilename = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputFilename, LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputExtension = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputExtension, LEN_PATHNAME * sizeof(WCHAR) );

    //
    int iError =
        _wsplitpath_s (
            pFullPathname, pInputDrive, LEN_PATHNAME, pInputDirectory, LEN_PATHNAME,
            pInputFilename, LEN_PATHNAME, pInputExtension, LEN_PATHNAME );
    if ( iError != 0 )
    {
        int iError = errno;
        ZeroMemory ( szErrorText, sizeof(szErrorText) );
        _wcserror_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), iError );
        PrintStderrW ( L"Error - Unable to Split Path - Cause : %s\n", szErrorText );

        bSuccess = FALSE;
    }

    //
    //  Check if there is something in the diectory
    if ( bSuccess )
    {
        if ( bDirectory )
        {
            //
            BOOL bEmpty = PathIsDirectoryEmpty ( pFullPathname );
            if ( ! bEmpty )
            {
                //
                bDirectory = isDirectory ( pFullPathname );
                if ( bDirectory )
                {
                    //
                    //  Try Again
                    Sleep ( 500 );

                    //
                    bEmpty = PathIsDirectoryEmpty ( pFullPathname );
                    if ( ! bEmpty )
                    {
                        PrintStderrW ( L"Error - '%s' - Not Empty\n", pFullPathname );

                        bSuccess = FALSE;
                    }
                }
            }
        }
    }

    //  Then Prepare filenames
    size_t lengthFilename = wcslen ( pInputFilename ) +  wcslen ( pInputExtension );

    //  The position at the length will be replaced by a zero   

    //  Our suffix will be added so remove it from length
    if ( lengthFilename > wcslen ( FILE_SUFFIX ) )
    {
        lengthFilename -= wcslen ( FILE_SUFFIX );
    }

    //  Thera is a minimal length
    if ( lengthFilename < MIN_FILENAME_LEN )
    {
        lengthFilename = MIN_FILENAME_LEN;
    }

    //
    WCHAR *pRenameFilename = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pRenameFilename, LEN_PATHNAME * sizeof(WCHAR) );
    time_t currentTime = time(NULL);
    swprintf ( pRenameFilename, LEN_PATHNAME, L"wsrm %d %d %ld", RandomCount % 1000,  rand(), ( DWORD ) currentTime );
    size_t iStart = wcslen ( pRenameFilename );

    //  Add spaces at the end of the string
    for ( size_t i = iStart; i < lengthFilename; i++ )
    {
        pRenameFilename [ i ] = L' ';
    }

    //
    //  Randomize change space to random characters
    randomizeName ( pRenameFilename );
    pRenameFilename [ lengthFilename ] = L'\0';

    //  The add a file type
    wcscat_s ( pRenameFilename, LEN_PATHNAME, FILE_SUFFIX );

    //  Set Rename Path
    WCHAR *pRenamePathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pRenamePathname, LEN_PATHNAME * sizeof(WCHAR) );
    wcscpy_s ( pRenamePathname, LEN_PATHNAME, pInputDrive );
    wcscat_s ( pRenamePathname, LEN_PATHNAME, pInputDirectory );
    wcscat_s ( pRenamePathname, LEN_PATHNAME, pRenameFilename );

    //
    PrintTraceW ( L"%s - Will be Renamed to %s\n", pFullPathname, pRenamePathname );

    //
    //  Rename the file
    if ( ! bProbeMode && bSuccess )
    {
        int iRenamed = _wrename ( pFullPathname, pRenamePathname );
        if ( iRenamed != 0 )
        {
            int iError = errno;
            ZeroMemory ( szErrorText, sizeof(szErrorText) );
            _wcserror_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), iError );
            PrintStderrW ( L"Error - Unable to Rename '%s' - Cause : %s\n", pFullPathname, szErrorText );

            bSuccess = FALSE;
        }
    }

    //  Then Remove
    if ( ! bProbeMode && bSuccess )
    {
        BOOL bDeleted = FALSE;
        if ( ! bDirectory )
        {
            //  We will hav to prepend '\\?\'
            wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
            wcscat_s ( szPrependFilename, LEN_PATHNAME, pRenamePathname );

            bDeleted = DeleteFile ( szPrependFilename );
        }
        else
        {
            //  We will hav to prepend '\\?\'
            wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
            wcscat_s ( szPrependFilename, LEN_PATHNAME, pRenamePathname );

            bDeleted = RemoveDirectory ( szPrependFilename );
        }

        //
        if ( ! bDeleted )
        {
            PrintStderrW ( L"Error - Deleting File '%s' - Cause : 0x%lx - %s\n", pRenamePathname, GetLastError(), GetLastErrorText() );

            int iRenamed = _wrename ( pRenamePathname, pFullPathname );
            if ( iRenamed == 0 )
            {
                PrintNormalW ( L"%s - Restored\n", pFullPathname );
            }
            else
            {
                int iError = errno;
                ZeroMemory ( szErrorText, sizeof(szErrorText) );
                _wcserror_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), iError );
                PrintStderrW ( L"Error - Unable to Restore '%s' - Rename Fails - Cause : %s\n", pFullPathname, szErrorText );
            }

            bSuccess = FALSE;
        }
        else if ( ! QuietMode )
        {
            if ( ! bDirectory )
            {
                PrintNormalW ( L"%s - File Deleted\n", pFullPathname );
            }
            else
            {
                PrintNormalW ( L"%s - Directory Deleted\n", pFullPathname );
            }
        }
    }

    //
    if ( bProbeMode )
    {
        PrintNormalW ( L"%s - Not Renamed or Deleted\n", pFullPathname );
    }

    //
    free ( pInputDrive );
    free ( pInputDirectory );
    free ( pInputFilename );
    free ( pInputExtension );

    //
    free ( pRenameFilename );
    free ( pRenamePathname );

    return bSuccess;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void displayFileInfo ( WCHAR *pFullPathname, WIN32_FIND_DATA *pFindFileData )
{
    //
    WCHAR szAttributes [ 32 ];
    ZeroMemory ( szAttributes, sizeof ( szAttributes ) );

    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"A" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"C" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"D" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"E" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"H" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_NORMAL )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"N" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_READONLY )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"R" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"S" );
    }
    if ( pFindFileData->dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY )
    {
        wcscat_s ( szAttributes, sizeof(szAttributes) / sizeof(WCHAR), L"T" );
    }

    long long fileSize = ( long long ) pFindFileData->nFileSizeHigh;
    fileSize = fileSize << 32;
    fileSize += ( long long ) pFindFileData->nFileSizeLow;

    PrintVerboseW ( L"%s - %s - %8lld\n", pFullPathname, szAttributes, fileSize );

}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL writeOverFile(WCHAR *pFullPathname, WIN32_FIND_DATA *pFindFileData )
{
    //
    //  Check for exclusion
    for ( int i = 0; i < iExcludeCount; i++ )
    {
        PrintTraceW ( L"Checking %s versus %s\n", FindFileNameW ( pFullPathname ), pExcludeTable [ i ] );
        if ( PathMatchSpec ( FindFileNameW ( pFullPathname ), pExcludeTable [ i ] ) )
        {
            PrintVerboseW ( L"Info : File %s is ignored (Excluded)\n", pFullPathname );
            return FALSE;
        }
    }

    if ( isAborted )
    {
        return FALSE;
    }

    //
    if ( ! QuietMode )
    {
        PrintNormalW ( L"%s - File Filling\r", pFullPathname );
    }

    //
    if ( bProbeMode )
    {
        PrintNormalW ( L"%s - No Written Over\n", pFullPathname );
        return TRUE;
    }

    long long fileSize = ( long long ) pFindFileData->nFileSizeHigh;
    fileSize = fileSize << 32;
    fileSize += ( long long ) pFindFileData->nFileSizeLow;

    //
    for ( int i = 1; i <= iPasses && ! isAborted; i++ )
    {

        LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;
        DWORD   dwDesiredAccess = FILE_GENERIC_WRITE;
        DWORD   dwShareMode = 0;
        DWORD   dwCreationDisposition = OPEN_EXISTING;
        DWORD   dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        HANDLE  hTemplateFile = NULL;

        //
        //  We will hav to prepend '\\?\'
        wcscpy_s ( szPrependFilename, LEN_PATHNAME, L"\\\\?\\" );
        wcscat_s ( szPrependFilename, LEN_PATHNAME, pFullPathname );

        HANDLE hFile =
            CreateFile ( szPrependFilename, dwDesiredAccess, dwShareMode,
                lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
        if ( hFile != INVALID_HANDLE_VALUE )
        {
            PrintVerboseW ( L"%s - Writing Over", pFullPathname );

            long long toBeWritten = fileSize;
            long long totalWritten = 0;
            while ( toBeWritten > 0 && ! isAborted )
            {
                DWORD   dwBytesToWrite;
                if ( toBeWritten > ( long long ) iBufferCurLength )
                {
                    dwBytesToWrite = ( DWORD ) iBufferCurLength;
                }
                else
                {
                    dwBytesToWrite = ( DWORD ) toBeWritten;
                }

                //
                //  Round to Block
                if ( bRoundToBlock )
                {
                    if ( ( dwBytePerSector != 0 ) && ( ( dwBytesToWrite % dwBytePerSector ) != 0 ) )
                    {
                        dwBytesToWrite /= dwBytePerSector;
                        dwBytesToWrite++;
                        dwBytesToWrite *= dwBytePerSector;
                        PrintVerboseW ( L" - Adjusting to %ld", dwBytesToWrite );
                    }
                }

                //
                DWORD   dwBytesWritten = 0;

                //
                if ( bZeroBuffer )
                {
                    iBufferIndex = 0;
                }
                else
                {
                    //
                    //  Fill With File with one buffer
                    if ( bFillWithFile || bFillWithFileEnv || bJpegText || bJpegTextEnv )
                    {
                        iBufferIndex = 1;
                    }
                    //
                    //  Fill Normal
                    else
                    {
                        iBufferIndex++;
                        iBufferIndex = iBufferIndex % MAX_BUFFERS;
                        if ( iBufferIndex == 0 )
                        {
                            iBufferIndex++;
                        }
                    }
                }

                if ( bGenerateTableEachWrite )
                {
                    initBuffers ( iBufferIndex );
                }

                //
                BOOL bWritten = WriteFile (
                        hFile,                              // open file  handle
                        pBufferRandom [ iBufferIndex ],     // start of data to write
                        dwBytesToWrite,                     // number of bytes to write
                        &dwBytesWritten,                    // number of bytes that were written
                        NULL );                             // no overlapped structure
                if ( ! bWritten )
                {
                    PrintStderrW ( L"Error - WriteFile File '%s' - Cause : 0x%lx - %s\n", pFullPathname, GetLastError(), GetLastErrorText() );

                    PrintVerboseW ( L" - %lld\n", totalWritten );

                    //
                    CloseHandle(hFile);

                    return FALSE;
                }

                toBeWritten -= ( long long ) dwBytesToWrite;
                totalWritten += ( long long ) dwBytesWritten;
            }

            //
            CloseHandle(hFile);

            PrintVerboseW ( L" - %lld\n", totalWritten );
        }
        else
        {
            PrintStderrW ( L"Error - CreateFile File '%s' - Cause : 0x%lx - %s\n", pFullPathname, GetLastError(), GetLastErrorText() );

            return FALSE;
        }

    }

    //
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL isSpecialDirectory ( WCHAR *pName )
{
    if ( wcscmp ( pName, L"." ) == 0 || wcscmp ( pName, L".." ) == 0 )
    {
        return TRUE;
    }

    return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL getFreeCluster ( WCHAR *pPathname )
{
    WCHAR szDrive [ 32 ];
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    szDrive [ 0 ] = pPathname [ 0 ];
    szDrive [ 1 ] = pPathname [ 1 ];

    BOOL bResult = FALSE;

    //
    if ( ! bDiskInfoDone )
    {
        bResult = GetDiskFreeSpace (
            szDrive,
            &dwSectorPerCluster, &dwBytePerSector, &dwNumberOfFreeCluster, &dwTotalNumberOfCluster );
        if ( ! bResult )
        {
            wcscpy_s ( szDrive, _wsizeof(szDrive), L"C:" );
            bResult = GetDiskFreeSpace (
                szDrive,
                &dwSectorPerCluster, &dwBytePerSector, &dwNumberOfFreeCluster, &dwTotalNumberOfCluster );
            if ( ! bResult )
            {
                PrintStderrW ( L"Error - GetDiskFreeSpace fails for '%s' - Cause : 0x%lx - %s\n", pPathname, GetLastError(), GetLastErrorText() );
            }
        }

        bDiskInfoDone = TRUE;
    }

    if ( bResult )
    {
        PrintTraceW ( L"GetDiskFreeSpace - Sector Per Cluster      : %12ld\n", dwSectorPerCluster );
        PrintTraceW ( L"GetDiskFreeSpace - Byte Per Sector         : %12ld\n", dwBytePerSector );
        PrintTraceW ( L"GetDiskFreeSpace - Number Of Free Cluster  : %12ld\n", dwNumberOfFreeCluster );
        PrintTraceW ( L"GetDiskFreeSpace - Total Number Of Cluster : %12ld\n", dwTotalNumberOfCluster );
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL CompareLCN ( PFILE_LCN pBLCNefore, PFILE_LCN pLCNAfter )
{
    if ( pBLCNefore == NULL || pLCNAfter == NULL )
    {
        PrintStderrW ( L"Error - Compare LCN - One Pointer Is NULL Before : %ld - After : %ld\n", pBLCNefore, pLCNAfter );
        return FALSE;
    }

    if ( pBLCNefore->lLcnCount != pLCNAfter->lLcnCount )
    {
        PrintStderrW ( L"Error - Compare LCN - Count Differs %ld != %ld\n", pBLCNefore->lLcnCount, pLCNAfter->lLcnCount );
        return FALSE;
    }

    for ( int i = 0; i < pBLCNefore->lLcnCount; i++ )
    {
        if ( pBLCNefore->lcnOffsetAndLength [ i ].llLcnOffset != pLCNAfter->lcnOffsetAndLength [ i ].llLcnOffset )
        {
            PrintStderrW ( L"Error - Compare LCN - Offset Differs for %d\n", i );
            return FALSE;
        }
        if ( pBLCNefore->lcnOffsetAndLength [ i ].llLcnLength != pLCNAfter->lcnOffsetAndLength [ i ].llLcnLength )
        {
            PrintStderrW ( L"Error - Compare LCN - Length Differs for %d\n", i );
            return FALSE;
        }
    }

    //
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL deletePath ( const WCHAR *lpPathname, int iLevel )
{
    //
    //  Check for exclusion
    for ( int i = 0; i < iExcludeCount; i++ )
    {
        PrintTraceW ( L"Checking %s versus %s\n", FindFileNameW ( lpPathname ), pExcludeTable [ i ] );
        if ( PathMatchSpec ( FindFileNameW ( lpPathname ), pExcludeTable [ i ] ) )
        {
            PrintVerboseW ( L"Info : File %s is ignored (Excluded)\n", lpPathname );
            return FALSE;
        }
    }

    if ( isAborted )
    {
        return FALSE;
    }

    //
    RandomCount++;

    //
    //
    BOOL bFoundOne = FALSE;

    //
    PrintTraceW ( L"%s - Searching\n", lpPathname );

    //
    WCHAR *pInputDrive = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputDrive, LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputDirectory = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputDirectory,LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputFilename = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputFilename, LEN_PATHNAME * sizeof(WCHAR) );

    WCHAR *pInputExtension = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pInputExtension, LEN_PATHNAME * sizeof(WCHAR) );

    int iError =
        _wsplitpath_s (
            lpPathname, pInputDrive, LEN_PATHNAME, pInputDirectory, LEN_PATHNAME,
            pInputFilename, LEN_PATHNAME, pInputExtension, LEN_PATHNAME );
    if ( iError != 0 )
    {
        int iError = errno;
        ZeroMemory ( szErrorText, sizeof(szErrorText) );
        _wcserror_s ( szErrorText, sizeof(szErrorText) / sizeof(WCHAR), iError );
        PrintStderrW ( L"Error - Unable to Split Path - Cause : %s\n", szErrorText );
        return FALSE;
    }

    //
    PrintTraceW ( L"Drive : '%s' - Directory :'%s' - Filename : '%s' - Extension : '%s'\n",
            pInputDrive, pInputDirectory,  pInputFilename, pInputExtension );

    //  First Recurse Directories for files
    if ( bRecursive && ! isAborted )
    {
        //
        WCHAR *pDirectoryPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
        ZeroMemory ( pDirectoryPathname, LEN_PATHNAME * sizeof(WCHAR) );
        wcscpy_s ( pDirectoryPathname, LEN_PATHNAME, pInputDrive );
        wcscat_s ( pDirectoryPathname, LEN_PATHNAME, pInputDirectory );
        wcscat_s ( pDirectoryPathname, LEN_PATHNAME, L"*.*" );

        //
        PrintTraceW ( L"FindFirstFile will use '%s'\n", pDirectoryPathname );

        //  FILE_ATTRIBUTE_ENCRYPTED FILE_ATTRIBUTE_HIDDEN FILE_ATTRIBUTE_READONLY
        WIN32_FIND_DATA FindFileData;

        ZeroMemory ( &FindFileData, sizeof( FindFileData ) );

        HANDLE hFileFind = FindFirstFile ( pDirectoryPathname, &FindFileData );
        if ( hFileFind != INVALID_HANDLE_VALUE )
        {
            //
            BOOL bFound = TRUE;
            while (  bFound && ! isAborted )
            {
                //
                RandomCount++;

                //
                BOOL bSpecial = FALSE;
                BOOL bDirectory = FALSE;
                if ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
                {
                    bDirectory = TRUE;
                    if ( isSpecialDirectory ( FindFileData.cFileName ) )
                    {
                        bSpecial = TRUE;
                    }
                }

                //
                if ( ! bSpecial && bDirectory && ! isAborted )
                {
                    //
                    //  Set Full Pathname
                    WCHAR *pPartialPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
                    ZeroMemory ( pPartialPathname, LEN_PATHNAME * sizeof(WCHAR) );
                    wcscpy_s ( pPartialPathname, LEN_PATHNAME, pInputDrive );
                    wcscat_s ( pPartialPathname, LEN_PATHNAME, pInputDirectory );
                    wcscat_s ( pPartialPathname, LEN_PATHNAME, FindFileData.cFileName );
                    PrintTraceW ( L"Partial Pathname '%s'\n", pPartialPathname );

                    //
                    WCHAR *pFullPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
                    ZeroMemory ( pFullPathname, LEN_PATHNAME * sizeof(WCHAR) );

                    //
                    //  Get Full Pathname from a Paetial Pathname
                    LPWSTR lpFilepart = NULL;
                    DWORD dwResult = GetFullPathName ( pPartialPathname, LEN_PATHNAME, pFullPathname, &lpFilepart );
                    if ( dwResult > 0 )
                    {
                        //
                        BOOL bFreeCluster = getFreeCluster ( pFullPathname );

                        //
                        if ( bCheckCluster )
                        {
                            //
                            //  Done Once Only
                            if ( ! bGetDiskVolumeDone )
                            {
                                ZeroMemory ( lpVolumeLcnInfo, sizeof ( VOLUME_DISK_INFO )  );

                                BOOL bResult = GetVolumeDiskInfo ( pFullPathname, lpVolumeLcnInfo );
                                if ( ! bResult )
                                {
                                    lpVolumeLcnInfo->BytesPerSector         = dwBytePerSector;
                                    lpVolumeLcnInfo->SectorsPerCluster      = dwSectorPerCluster;
                                }
                                bGetDiskVolumeDone = TRUE;
                            }
                        }

                        //
                        PrintVerboseW ( L"%s - Entering\n", pFullPathname );

                        //
                        PrintTraceW ( L"Directory '%s'\n", FindFileData.cFileName );

                        //
                        //
                        WCHAR *pSubPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
                        ZeroMemory ( pSubPathname, LEN_PATHNAME * sizeof(WCHAR) );
                        wcscpy_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), pFullPathname );
                        wcscat_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), L"\\" );
                        wcscat_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), pInputFilename );
                        wcscat_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), pInputExtension );

                        //
                        bFoundOne |= deletePath ( pSubPathname, iLevel + 1 );

                        //
                        free ( pSubPathname );

                        PrintVerboseW ( L"%s - Leaving\n", pFullPathname );

                    }

                    //
                    free ( pFullPathname );
                    free ( pPartialPathname );

                }

                //
                bFound = FindNextFile ( hFileFind, &FindFileData );
            }

            //
            FindClose ( hFileFind );

        }   //  INVALID_HANDLE_VALUE

        free ( pDirectoryPathname );

    }   // Recursive

    //
    //  For Files And Directories
    WCHAR *pSearchFilePathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
    ZeroMemory ( pSearchFilePathname, LEN_PATHNAME * sizeof(WCHAR) );
    wcscpy_s ( pSearchFilePathname, LEN_PATHNAME, pInputDrive );
    wcscat_s ( pSearchFilePathname, LEN_PATHNAME, pInputDirectory );
    wcscat_s ( pSearchFilePathname, LEN_PATHNAME, pInputFilename );
    wcscat_s ( pSearchFilePathname, LEN_PATHNAME, pInputExtension );

    //  FILE_ATTRIBUTE_ENCRYPTED FILE_ATTRIBUTE_HIDDEN FILE_ATTRIBUTE_READONLY
    WIN32_FIND_DATA FindFileData;

    ZeroMemory ( &FindFileData, sizeof( FindFileData ) );

    HANDLE hFileFind = FindFirstFile ( pSearchFilePathname, &FindFileData );
    if ( hFileFind != INVALID_HANDLE_VALUE )
    {
        //
        BOOL bFound = TRUE;
        while (  bFound && ! isAborted )
        {
            //
            RandomCount++;

            //
            BOOL bDirectory = FALSE;
            BOOL bSpecial = FALSE;
            if ( ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0x00 )
            {
                bDirectory = TRUE;
                if ( isSpecialDirectory ( FindFileData.cFileName ) )
                {
                    bSpecial = TRUE;
                }
            }

            //
            //  Set Full Pathname
            WCHAR *pPartialPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
            ZeroMemory ( pPartialPathname, LEN_PATHNAME * sizeof(WCHAR) );
            wcscpy_s ( pPartialPathname, LEN_PATHNAME, pInputDrive );
            wcscat_s ( pPartialPathname, LEN_PATHNAME, pInputDirectory );
            wcscat_s ( pPartialPathname, LEN_PATHNAME, FindFileData.cFileName );

            PrintTraceW ( L"Found Partial Pathname '%s'\n", pPartialPathname );

            //
            WCHAR *pFullPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
            ZeroMemory ( pFullPathname, LEN_PATHNAME * sizeof(WCHAR) );

            //
            //  Get Full Pathname from Partial Pathname
            LPWSTR lpFilepart = NULL;
            DWORD dwResult = GetFullPathName ( pPartialPathname, LEN_PATHNAME, pFullPathname, &lpFilepart );
            if ( dwResult > 0 )
            {
                //
                BOOL bFreeCluster = getFreeCluster ( pFullPathname );

                //
                if ( bCheckCluster )
                {
                    //
                    //  Done Once Only
                    if ( ! bGetDiskVolumeDone )
                    {
                        ZeroMemory ( lpVolumeLcnInfo, sizeof ( VOLUME_DISK_INFO )  );

                        BOOL bResult = GetVolumeDiskInfo ( pFullPathname, lpVolumeLcnInfo );
                        if ( ! bResult )
                        {
                            lpVolumeLcnInfo->BytesPerSector         = dwBytePerSector;
                            lpVolumeLcnInfo->SectorsPerCluster      = dwSectorPerCluster;
                        }
                        bGetDiskVolumeDone = TRUE;
                    }
                }

                //
                if ( ! bDirectory )
                {
                    bFoundOne = TRUE;

                    PrintTraceW ( L"deletePath - Filename '%s'\n", FindFileData.cFileName );

                    //
                    displayFileInfo ( pFullPathname, &FindFileData );

                    //
                    //  Delete for A File
                    BOOL doDelete = AskConfirmQuestionForPathname ( pFullPathname );
                    if ( doDelete )
                    {
                        //
                        //  Change Flage
                        if ( bOverwrite )
                        {
                            changeAttributes ( pFullPathname, FindFileData.dwFileAttributes );
                        }

                        //
                        //
                        //  Write Over for non encrypted files
                        BOOL bWriteOver = TRUE;
                        if ( ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ) == 0 || bEncryptedWriteOver )
                        {
                            //
                            //  Get Before
                            if ( bCheckCluster && ! bDeleteOnly )
                            {
                                ZeroMemory ( lpFileLcnBefore, iFileLcnSize );

                                HANDLE hCheckFile = OpenBitmapAndCreateMapping ( pFullPathname );
                                if ( hCheckFile != INVALID_HANDLE_VALUE )
                                {
                                    BOOL bGetOffset = GetFileOffset ( hCheckFile, lpVolumeLcnInfo, lpFileLcnBefore, iFileLcnMax );
                                    BOOL bClosed = CloseHandle( hCheckFile );
                                }
                            }

                            //
                            if ( bDeleteOnly )
                            {
                                bWriteOver = TRUE;
                            }
                            else
                            {
                                bWriteOver = writeOverFile ( pFullPathname, &FindFileData );
                            }

                            //
                            //  Get After
                            if ( bCheckCluster && ! bDeleteOnly )
                            {
                                ZeroMemory ( lpFileLcnAfter, iFileLcnSize );

                                HANDLE hCheckFile = OpenBitmapAndCreateMapping ( pFullPathname );
                                if ( hCheckFile != INVALID_HANDLE_VALUE )
                                {
                                    BOOL bGetOffset = GetFileOffset ( hCheckFile, lpVolumeLcnInfo, lpFileLcnAfter, iFileLcnMax );
                                    BOOL bClosed = CloseHandle( hCheckFile );

                                    //
                                    BOOL bCompareLCN = CompareLCN ( lpFileLcnAfter, lpFileLcnBefore );
                                }
                            }
                        }   // FILE_ATTRIBUTE_ENCRYPTED

                        //
                        if ( bWriteOver )
                        {
                            renameAndDelete ( pFullPathname, FALSE );
                        }
                    }
                }
                //
                //  A Directory
                else if ( bDirectory && ! bSpecial && ! isAborted )
                {
                    bFoundOne = TRUE;

                    //
                    //  The Directory is correct
                    //  Is the flag subdirectory specified
                    if ( bSubDirectory )
                    {
                        PrintVerboseW ( L"%s - Entering\n", pFullPathname );

                        //
                        PrintTraceW ( L"Directory '%s'\n", FindFileData.cFileName );

                        //
                        //
                        WCHAR *pSubPathname = ( WCHAR * ) malloc ( LEN_PATHNAME * sizeof(WCHAR) );
                        ZeroMemory ( pSubPathname, LEN_PATHNAME * sizeof(WCHAR) );
                        wcscpy_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), pFullPathname );
                        wcscat_s ( pSubPathname, LEN_PATHNAME / sizeof(WCHAR), L"\\*.*" );

                        //
                        //      Recurse Directory
                        deletePath ( pSubPathname, iLevel + 1 );

                        //
                        free ( pSubPathname );

                        PrintVerboseW ( L"%s - Leaving\n", pFullPathname );

                    }

                    //
                    displayFileInfo ( pFullPathname, &FindFileData );

                    //
                    //  Delete for a Directory
                    BOOL doDelete = AskConfirmQuestionForPathname ( pFullPathname );
                    if ( doDelete )
                    {
                        //
                        //  Change Flage
                        if ( bOverwrite )
                        {
                            changeAttributes ( pFullPathname, FindFileData.dwFileAttributes );
                        }

                        renameAndDelete ( pFullPathname, TRUE );
                    }
                }


            }   // dwResult
            else
            {
                //  Split Fails
            }

            free ( pFullPathname );
            free ( pPartialPathname );

            //
            bFound = FindNextFile ( hFileFind, &FindFileData );
        }

        //
        FindClose ( hFileFind );
    }

    //
    free ( pInputDrive );
    free ( pInputDirectory );
    free ( pInputFilename );
    free ( pInputExtension );
    free ( pSearchFilePathname );

    if ( iLevel == 0 && ! bFoundOne )
    {
        PrintStderrW ( L"Error - '%s' Not Found\n", lpPathname );
    }

    //
    return bFoundOne;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void printBanner()
{
    int iWidth = 24;

    PrintHelpLine ( iWidth, PROGRAM_NAME_P, PROGRAM_DATE_F, PROGRAM_VERSION );
    PrintHelpLine ( );
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void printHelp ( int iArgCount, WCHAR *pArgValues [], bool bLong, bool bExit )
{
    printBanner();

    int iWidth = 24;

    PrintHelpLine ( iWidth, L"Syntax is", PROGRAM_NAME, L"[options] {files or directories...}" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"Optionally Followed By [filter] For -sd or -sf", L"" );
        PrintHelpLine ( iWidth, L"", L"Followed By [output] For -copy", L"" );
    }

    if ( bLong )
    {
        // PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -ntfs entry {drive}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -sf {drive or volume} {filter}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -sd {drive or volume} {filter}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -sdir {drive or volume} {filter}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -xsd {drive}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -filldisk {drive/directory}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -fillname {drive/directory} -count num" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -count {count} -fillname {drive or volume}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -copy {filenumber} {drive or volume} -output file" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -copy {filenumber} {drive or volume} {outputfile}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -locate {filenumber} {drive or volume}" );
        PrintHelpLine ( iWidth, L"", PROGRAM_NAME, L"[options] -info {filenumber} {drive or volume}" );
        PrintHelpLine ( );
        PrintHelpLine ( iWidth, L"", L"files and directories can use wildcards '*' (C:\\TEMP\\*, C:\\TEMP\\*.log)" );
        PrintHelpLine ( iWidth, L"", L"Pathname are drive:\\directories\\files (C:\\TEMP\\test.tmp)", L"" );
        PrintHelpLine ( iWidth, L"", L"Drives are disk: (C:, D:, ...)" );
        PrintHelpLine ( iWidth, L"", L"Drives are also \\\\?\\disk: without trailing \\ " );
        PrintHelpLine ( iWidth, L"", L"Volumes are     \\\\?\\{id}  without trailing \\ " );
        PrintHelpLine ( iWidth, L"", L"See cmd mountvol for Volumes" );
    }

    PrintHelpLine ( );
    PrintHelpLine ( iWidth, L"", L"Clear File Content, Then Rename File And Finally Remove File");
    PrintHelpLine ( );

    PrintHelpLine ( iWidth, L"-h, -help, -? ", L"Show Help" );
    PrintHelpLine ( iWidth, L"-hl, -l, -longhelp", L"Show Long Help" );

    PrintHelpLine ( iWidth, L"-1 to -9", L"Number Of Passes For Writing File" );
    PrintHelpLine ( iWidth, L"-b, -block", L"Last Writing Will Be Rounded to Block Size" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-banner", L"Display Banner" );
    }
    PrintHelpLine ( iWidth, L"-c, -clusters", L"Check Cluster Allocation" );
    PrintHelpLine ( iWidth, L"-count number", L"Count for -fillname" );

    PrintHelpLine ( iWidth, L"-copy filenumber", L"Copy Filenumber To A File" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"Make Sure To Specify The Output File On Another Disk" );
    }

    PrintHelpLine ( iWidth, L"-d, -delete", L"Delete Only (no write over)" );

    PrintHelpLine ( iWidth, L"-e, -encrypted", L"Write Over Encrypted Files (Normally Not Done)" );
    PrintHelpLine ( iWidth, L"-f file, -fillfile file", L"Fill Buffer With This File" );
    PrintHelpLine ( iWidth, L"", L"Only the first 512k bytes will be used" );

    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"(If File Is Not Found Or '-' It Will Be Not Used)" );
    }

    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-filter substring", L"Filter for -sd and -sf" );
        PrintHelpLine ( iWidth, L"", L"A Filter For Filenames (*.doc, foo*.log)" );
    }
    PrintHelpLine ( iWidth, L"-filldisk {drive/dir}", L"Fill One Disk" );
    PrintHelpLine ( iWidth, L"-fillname {drive/dir}", L"Create Files on The Disk" );
    PrintHelpLine ( iWidth, L"-g", L"Generate Table For Each Write" );
    PrintHelpLine ( iWidth, L"", L"(Otherwise Tables are Generated Once At Start)" );
    PrintHelpLine ( iWidth, L"-i", L"Interactive Mode (Ask Confirmation Before Deletion)" );
    PrintHelpLine ( iWidth, L"-info filenumber", L"Display Infos For A Fiie Number" );
    PrintHelpLine ( iWidth, L"-j text, -jpgtext text", L"Generate a jpeg file From Text That Will Be Used For Buffers" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"(If Text Is Empty Or '-' It Will Be Not Used)" );
        PrintHelpLine ( iWidth, L"-k filename", L"Create The JPEG file To See What It Looks Like" );
    }

    PrintHelpLine ( iWidth, L"-locale string", L"Display using locale information (eg FR-fr, .1252)" );

    PrintHelpLine ( iWidth, L"-locate filenumber", L"Display Hierarchy Info" );
    PrintHelpLine ( iWidth, L"-nobanner", L"Do Not Display Banner" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-nomatch substring", L"Filter for -sd and -sf" );
    }
    PrintHelpLine ( iWidth, L"-o, -over", L"Overwrite Read Only / System Flag" );
    PrintHelpLine ( iWidth, L"-output filename", L"For -copy Specify The Output Filename" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"Can be A Directory Or A Filename" );
    }
    PrintHelpLine ( iWidth, L"-p, -probe", L"Probe Mode : Just Test And Do Nothing" );
    PrintHelpLine ( iWidth, L"-q, -quiet", L"Quiet Mode (Only Display Errors)" );
    PrintHelpLine ( iWidth, L"-r, -recursive", L"Recursive Mode For File Deletion (Same as 'del /s')" );
    PrintHelpLine ( iWidth, L"-s, -subdir", L"Remove Subdirectory (Same As 'rd /s')" );
    PrintHelpLine ( iWidth, L"-sd drive/volume", L"Show Deleted Files" );
    PrintHelpLine ( iWidth, L"-sf drive/volume", L"Show All Files (including deleted)" );
    PrintHelpLine ( iWidth, L"-sdir drive/volume", L"Show Directories Only (including deleted)" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-t, -trace", L"Trace Mode (More Verbose)" );
    }
    PrintHelpLine ( iWidth, L"-v, -verbose", L"Verbose Mode" );
    PrintHelpLine ( iWidth, L"-w, -write n", L"Write n Times (The Default is One Pass)" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-xsd drive", L"Show Deleted Files" );
    }
    PrintHelpLine ( iWidth, L"-x, -exclude files", L"Exclude files on deletion" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"-xsd drive", L"Show Deleted Files" );
    }
    PrintHelpLine ( iWidth, L"-y, -yes", L"Yes Response To Questions (No Confirmation On Delete)" );
    if ( bLong )
    {
        PrintHelpLine ( iWidth, L"", L"It Disables Interactive Mode" );
    }
    PrintHelpLine ( iWidth, L"-z, -zero", L"Use Zeroes Buffer" );


    //
    if ( bLong )
    {
        PrintHelpLine ( );
        PrintHelpLine ( iWidth, L"Environment", L"Variables" );
        PrintHelpLine ( iWidth, WSRM_FILL_FILENAME, L"If Set This Will Be The File Used To Fill Buffers" );
        PrintHelpLine ( iWidth, WSRM_JPEG_TEXT, L"If Set This Will Be Used to Create a Jpeg To Fill Buffers" );
        PrintHelpLine ( iWidth, WSRM_INTERACTIVE, L"If Set to YES The Mode Will Be Interactive" );
        PrintHelpLine ( iWidth, WSRM_NO_BANNER, L"If Set to YES No Banner Will Be Displayed" );
    }

    PrintCrashHelpW ( iWidth, PROGRAM_NAME, iArgCount, pArgValues, bLong );

    if ( bExit )
    {
        exit ( 0 );
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL treatOptions ( int iArgCount, WCHAR *pArgValues [] )
{
    //
    Action = Action_DeletePath;

    //
    ZeroMemory ( szDriveVolume, sizeof ( szDriveVolume ) );
    ZeroMemory ( LocaleString, sizeof ( LocaleString ) );

    //
    int i = 1;
    while ( i < iArgCount )
    {
        //
        //  Options
        WCHAR *pOption = pArgValues [ i ];
        if ( *pOption == L'-' || *pOption == L'/' )
        {
            pOption++;

            //
            //  Do Not Display Banner
            if ( __wcsicmpL ( pOption, L"h", L"help", L"?", NULL ) == 0 )
            {
                Action = Action_None;
                printHelp ( iArgCount, pArgValues, FALSE, TRUE );
            }

            //
            else if ( TreatCrashOptions ( iArgCount, pArgValues, pOption, i ) )
            {
                //
            }
            //
            //  Do Not Display Banner
            else if ( *pOption >= L'1' && *pOption <= L'9' )
            {
                iPasses = _wtol ( pOption );
                if ( iPasses < 0 )
                {
                    iPasses = 1;
                }
                if ( iPasses > 9 )
                {
                    iPasses = 9;
                }
            }

            //
            //  Round to block the last writing
            else if ( __wcsicmpL ( pOption, L"b", L"block", NULL ) == 0 )
            {
                bRoundToBlock = TRUE;
            }

            //
            //  Display Banner
            else if ( _wcsicmp ( pOption, L"banner" ) == 0 )
            {
                bNoBanner = FALSE;
                bNoBannerEnv = FALSE;
            }

            //
            //  Check Cluster Allocation
            else if ( __wcsicmpL ( pOption, L"c", L"clusters", NULL ) == 0 )
            {
                bCheckCluster = TRUE;
            }

            //
            //  Count
            else if ( __wcsicmpL ( pOption, L"count", NULL ) == 0 ||  __wcsnicmpL ( pOption, L"count=", NULL ) == 0 )
            {
                Action = Action_FillName;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bCountItems = TRUE;
                iCountItems = _wtol (pCode );
            }

            //
            //  Copy Information on a File
            else if ( __wcsicmpL ( pOption, L"copy", NULL ) == 0 ||  __wcsnicmpL ( pOption, L"copy=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );
                Action = Action_Copy;
                iFileNumber = _wtol ( pCode );

            }

            //
            //  Delete Only
            else if ( __wcsicmpL ( pOption, L"d", L"delete", NULL ) == 0 )
            {
                bDeleteOnly = TRUE;
            }

            //
            //  Write Over Encrypted
            else if ( __wcsicmpL ( pOption, L"e", L"encrypted", NULL ) == 0 )
            {
                bEncryptedWriteOver = TRUE;
            }

            //
            //  Fill With File
            else if ( __wcsicmpL ( pOption, L"f", L"fillfile", NULL ) == 0 || __wcsnicmpL ( pOption, L"f=", L"fillfile=", NULL ) == 0)
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bFillWithFile = TRUE;
                ZeroMemory ( szFillWithFile, sizeof(szFillWithFile) );
                wcscpy_s ( szFillWithFile, sizeof(szFillWithFile) / sizeof ( WCHAR) , pCode );

                if ( wcscmp ( szFillWithFile, L"-" ) == 0  || wcslen (szFillWithFile) == 0 )
                {
                    bFillWithFile       = TRUE;
                    bFillWithFileEnv    = TRUE;
                    ZeroMemory ( szFillWithFile, sizeof(szFillWithFile) );
                }
            }

            //
            //  Fill One Disk
            else if ( __wcsicmpL ( pOption, L"filldisk", NULL ) == 0 || __wcsnicmpL ( pOption, L"filldisk=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_FillDisk;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );
            }

            //
            //  Fill Names
            else if ( __wcsicmpL ( pOption, L"fillname", NULL ) == 0 || __wcsnicmpL ( pOption, L"fillname=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_FillName;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );
            }

            //
            //  Filter
            else if ( __wcsicmpL ( pOption, L"filter", NULL ) == 0 || __wcsnicmpL ( pOption, L"filter=", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bFilter = TRUE;
                ZeroMemory ( szFilter, sizeof(szFilter) );
                wcscpy_s ( szFilter, sizeof(szFilter) / sizeof ( WCHAR) , pCode );

                if ( wcslen(szFilter) == 0 )
                {
                    bFilter = FALSE;
                    ZeroMemory ( szFilter, sizeof(szFilter) );
                }
            }

            //
            //  Write Over Encrypted
            else if ( __wcsicmpL ( pOption, L"g", NULL ) == 0 )
            {
                bGenerateTableEachWrite = TRUE;
            }

            //
            //  Interractive
            else if ( __wcsicmpL ( pOption, L"i", NULL ) == 0 )
            {
                bInteractive = TRUE;
            }

            //
            //  Display Information on a File
            else if ( __wcsicmpL ( pOption, L"info", NULL ) == 0 || __wcsnicmpL ( pOption, L"info=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_FileInfo;
                iFileNumber = _wtol ( pCode );
            }

            //
            //  Fill With a Jpeg File
            else if ( __wcsicmpL ( pOption, L"j", L"jpgtext", NULL ) == 0 || __wcsnicmpL ( pOption, L"j=", L"jpgtext=", NULL ) == 0 )
            {

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bJpegText = TRUE;
                ZeroMemory ( szJpegext, sizeof(szJpegext) );
                wcscpy_s ( szJpegext, sizeof(szJpegext) / sizeof ( WCHAR), pCode );

                if ( wcscmp ( szJpegext, L"-" ) == 0 || wcslen(szJpegext) == 0 )
                {
                    bJpegText = FALSE;
                    bJpegTextEnv = FALSE;
                    ZeroMemory ( szJpegext, sizeof(szJpegext) );
                }
            }

            //
            //  Fill With File
            else if ( __wcsicmpL ( pOption, L"k", NULL ) == 0 || __wcsnicmpL ( pOption, L"k=", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bCreateJpgFile = TRUE;
                ZeroMemory ( szCreateJpgFile, sizeof(szCreateJpgFile) );
                wcscpy_s ( szCreateJpgFile, sizeof(szCreateJpgFile) / sizeof ( WCHAR), pCode );
            }

            //
            //  Long Help
            else if ( __wcsicmpL ( pOption, L"l", L"hl", L"longhelp", NULL ) == 0 )
            {
                Action = Action_None;
                printHelp ( iArgCount, pArgValues, TRUE, TRUE );
            }

            //
            //  Locale Information
            else if ( __wcsicmpL ( pOption, L"locale", NULL ) == 0 || __wcsnicmpL ( pOption, L"locale=", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                ZeroMemory ( LocaleString, sizeof(LocaleString) );
                wcscpy_s ( LocaleString, sizeof(LocaleString) / sizeof ( WCHAR), pCode );
            }

            //
            //  Display Information on a File
            else if ( __wcsicmpL ( pOption, L"locate", NULL ) == 0 || __wcsnicmpL ( pOption, L"locate=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                if ( *pCode >= L'0' && *pCode <= L'9' )
                {
                    Action = Action_LocateFile;
                    iFileNumber = _wtol ( pCode );
                }
                else
                {
                    PrintStderrW ( L"Error - No Filenumber Defined\n" );
                    exit ( 1 );
                }
            }

            //
            //  Do Not Display Banner
            else if ( __wcsicmpL ( pOption, L"nobanner", NULL ) == 0 )
            {
                bNoBanner = TRUE;
            }

            //
            //  Filter
            else if ( __wcsicmpL ( pOption, L"nomatch", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bNoMatch = TRUE;
                ZeroMemory ( szNoMatch, sizeof(szNoMatch) );
                wcscpy_s ( szNoMatch, sizeof(szNoMatch) / sizeof ( WCHAR) , pCode );

                if ( wcslen(szNoMatch) == 0 )
                {
                    bNoMatch = FALSE;
                    ZeroMemory ( szNoMatch, sizeof(szNoMatch) );
                }
            }

            //
            //  Overwrite READ ONLY Flags
            else if ( __wcsicmpL ( pOption, L"o", L"over", NULL ) == 0 )
            {
                bOverwrite = TRUE;
            }

            //
            //  The Output Filename
            else if ( __wcsicmpL ( pOption, L"output", NULL ) == 0 || __wcsnicmpL ( pOption, L"output=", NULL ) == 0 )
            {
                Action = Action_Copy;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                bOutput = TRUE;
                ZeroMemory ( szOutputFile, sizeof(szOutputFile) );
                wcscpy_s ( szOutputFile, sizeof(szOutputFile) / sizeof ( WCHAR) , pCode );

                if ( wcscmp ( szOutputFile, L"-" ) == 0 || wcslen(szOutputFile) == 0 )
                {
                    bOutput = FALSE;
                    ZeroMemory ( szOutputFile, sizeof(szOutputFile) );
                }
            }

            //
            //  Probe Mode
            else if ( __wcsicmpL ( pOption, L"p", L"probe", NULL ) == 0 )
            {
                bProbeMode = TRUE;
            }

            //
            //  Quiet Mode Implies not Verbose nor Trace
            else if ( __wcsicmpL ( pOption, L"q", L"quiet", NULL ) == 0 )
            {
                QuietMode = TRUE;
                VerboseMode = FALSE;
                TraceMode = FALSE;
            }

            //
            //  Recursive
            else if ( __wcsicmpL ( pOption, L"r", L"recursive", NULL ) == 0 )
            {
                bRecursive = TRUE;
            }

            //
            //  Sub Directory
            else if ( __wcsicmpL ( pOption, L"s", L"subdir", NULL ) == 0 )
            {
                bSubDirectory = TRUE;
            }

            //
            //  Show Delete Files
            else if ( __wcsicmpL ( pOption, L"sd", NULL ) == 0 || __wcsnicmpL ( pOption, L"sd=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_ShowDeletedFiles;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );

            }

            //
            //  Show All Files
            else if ( __wcsicmpL ( pOption, L"sf", NULL ) == 0 ||  __wcsnicmpL ( pOption, L"sf=", NULL ) == 0 )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_ShowAllFiles;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );
            }

            //
            //  Show Directories
            else if ( __wcsicmpL ( pOption, L"sdir", NULL ) == 0 || __wcsnicmpL ( pOption, L"sdir=", NULL ) == 0  )
            {
                Action = Action_None;

                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_ShowDirectories;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );
            }

            //
            //  Trace Implies Verbose and not Quiet
            else if ( __wcsicmpL ( pOption, L"t", L"trace", NULL ) == 0 )
            {
                TraceMode = TRUE;
                VerboseMode = TRUE;
                QuietMode = FALSE;
            }

            //
            //  Verbose Implies not Quiet
            else if ( __wcsicmpL ( pOption, L"v", L"verbose", NULL ) == 0 )
            {
                VerboseMode = TRUE;
                QuietMode = FALSE;
            }

            //
            //  Write
            else if ( __wcsicmpL ( pOption, L"w", L"write", NULL ) == 0 || __wcsnicmpL ( pOption, L"w=", L"write=", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                iPasses = _wtol ( pCode );
                if ( iPasses < 0 )
                {
                    iPasses = 1;
                }

                if ( iPasses > 9 )
                {
                    iPasses = 9;
                }
            }

            //
            //  Exclude
            else if ( __wcsicmpL ( pOption, L"x", L"exclude", NULL ) == 0 || __wcsnicmpL ( pOption, L"x=", L"exclude=", NULL ) == 0  )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                if ( iExcludeCount < iExcludeMax - 1 )
                {
                    size_t length =  ( wcslen ( pCode ) + sizeof(WCHAR) ) * sizeof(WCHAR);
                    pExcludeTable [ iExcludeCount ] = ( WCHAR * ) malloc ( length  );
                    wcscpy_s ( pExcludeTable [ iExcludeCount ], length / sizeof(WCHAR), pCode );
                    iExcludeCount++;
                }
                else
                {
                    PrintStderrW ( L"Error - Exclude Files maximum reached\n" );
                    exit ( 1 );
                }
            }
            //
            //  Show Delete Files
            else if ( __wcsicmpL ( pOption, L"xsd", NULL ) == 0 || __wcsnicmpL ( pOption, L"xsd=", NULL ) == 0 )
            {
                WCHAR *pCode = GetArgument ( iArgCount, pArgValues, &i );

                Action = Action_None;

                Action = Action_ShowDeletedFilesXE;
                ZeroMemory ( szDriveVolume, sizeof(szDriveVolume) );
                wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof ( WCHAR) , pCode );
            }

            //
            //  Yes To Questions
            else if ( __wcsicmpL ( pOption, L"y", L"yes", NULL ) == 0 )
            {
                bYesToQuestion = TRUE;
                bInteractive = FALSE;
                bInteractiveEnv = FALSE;
            }

            //
            //  Zero
            else if ( __wcsicmpL ( pOption, L"z", L"zero", NULL ) == 0 )
            {
                bZeroBuffer = TRUE;
            }

            //
            else
            {
                printHelp ( iArgCount, pArgValues, FALSE, FALSE );

                PrintStderrW ( L"Error - Invalid Option '%s'\n", pArgValues [ i ] );
                exit ( 1 );
            }
        }
        else
        {
            //  Argument not empty
            if ( pArgValues [ i ] != NULL && wcslen ( pArgValues [ i ] ) > 0 )
            {

                //  For Show All FIles and Show Deleted Files The parameter can be the filter
                if ( Action == Action_ShowAllFiles || Action == Action_ShowDeletedFiles || Action == Action_ShowDirectories )
                {
                    if ( wcslen ( szFilter ) == 0 )
                    {
                        bFilter = TRUE;
                        wcscpy_s ( szFilter, sizeof(szFilter) / sizeof(WCHAR), pArgValues [ i ] );
                    }
                    else
                    {
                        PrintStderrW ( L"Error - A Filter is Already Defined\n" );
                        exit ( 1 );
                    }
                }

                //  For Copy the parameter is the drive/volume or can be the output filename
                else if ( Action == Action_Copy )
                {
                    if ( wcslen ( szDriveVolume ) == 0 )
                    {
                        wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof(WCHAR), pArgValues [ i ] );
                    }
                    else if ( wcslen ( szOutputFile ) == 0 )
                    {
                        bOutput = TRUE;
                        wcscpy_s ( szOutputFile, sizeof(szOutputFile) / sizeof(WCHAR), pArgValues [ i ] );
                    }
                    else
                    {
                        PrintStderrW ( L"Error - An Output File is Already Defined\n" );
                        exit ( 1 );
                    }
                }

                //
                else if ( Action == Action_LocateFile || Action == Action_FileInfo )
                {
                    if ( wcslen ( szDriveVolume ) == 0 )
                    {
                        wcscpy_s ( szDriveVolume, sizeof(szDriveVolume) / sizeof(WCHAR), pArgValues [ i ] );
                    }
                    else
                    {
                        PrintStderrW ( L"Error - Drive/Volume is Already Defined\n" );
                        exit ( 1 );
                    }
                }

                //
                //  Otherwise : The Path Name for Delete
                else if ( iPathnameCount < iPathnameMax - 1 )
                {
                    size_t length =  ( wcslen ( pArgValues [ i ] ) + sizeof(WCHAR) ) * sizeof(WCHAR);
                    pPathnameTable [ iPathnameCount ] = ( WCHAR * ) malloc ( length  );
                    wcscpy_s ( pPathnameTable [ iPathnameCount ], length / sizeof(WCHAR), pArgValues [ i ] );
                    iPathnameCount++;
                }

                //
                else
                {
                    printHelp ( iArgCount, pArgValues, FALSE, FALSE );

                    PrintStderrW ( L"Error - Only one pathname must be specified\n" );
                    exit ( 1 );
                }
            }
            else
            {
                PrintStderrW ( L"Error - Invalid Empty #%d argument\n", i );
                exit ( 1 );
            }
        }

        i++;
    }

    //
    if ( wcslen ( szDriveVolume ) == 0 && iPathnameCount == 0 )
    {
        printHelp ( iArgCount, pArgValues, FALSE, FALSE );

        PrintStderrW ( L"Error - At least a filename/drive/volume must be provided\n" );

        exit ( 1 );
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void initBuffers ( int index )
{
    //  Maximum Length and Current Length
    iBufferMaxLength = MAX_BUFFER_LENGTH;
    iBufferCurLength = iBufferMaxLength;

    //  Allocate the vector
    if ( pBufferRandom == NULL )
    {
        unsigned int lenToAllocate = MAX_BUFFERS * sizeof ( BYTE * );
        pBufferRandom = ( BYTE ** ) malloc ( lenToAllocate );
        ZeroMemory ( pBufferRandom, lenToAllocate );
    }

    //
    //  For All Buffers
    for ( int i = 0; i < MAX_BUFFERS; i++ )
    {
        BOOL toInit = FALSE;
        if ( pBufferRandom [ i ] == NULL )
        {
            pBufferRandom [ i ] = ( BYTE * ) malloc ( iBufferMaxLength );
            memset ( pBufferRandom [ i ], 0, iBufferMaxLength );
            toInit = TRUE;
        }

        //
        BYTE *pBuffer = pBufferRandom [ i ];

        //
        //  Fill Buffer
        if ( toInit || index == -1 || index == i )
        {
            //  First Buffer Will be always all zeroes
            //  Other will be random
            if ( i > 0 )
            {
                short * pShortBuffer = (short *) pBuffer;
                for ( size_t iX = 0; iX < iBufferMaxLength; iX++ )
                {
                    pBuffer [ iX ] = rand() & 0xff;
                }

                for ( size_t iX = 0; iX < iBufferMaxLength / sizeof(short); iX++ )
                {
                    pShortBuffer [ iX ] = rand();
                }
            }
        }
    }

}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
void freeBuffers ()
{
    if ( pBufferRandom != NULL )
    {
        //
        //  For All Buffers
        for ( int i = 0; i < MAX_BUFFERS; i++ )
        {
            if ( pBufferRandom [ i ] != NULL )
            {
                free ( pBufferRandom [ i ] );
                pBufferRandom [ i ] = NULL;
            }
        }

        //
        free ( pBufferRandom );
        pBufferRandom = NULL;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
int _tmain ( int iArgCount, WCHAR *pArgValues [] )
{
    //
    SetConsoleEventHandler ( &isAborted );

    //
    InitStdHandlers();

    //
    //  Init Randomisation
    randomSeed();

    //
    //  Reset Table Search Pathname
    for ( int i = 0; i < iPathnameMax; i++ )
    {
        pPathnameTable [ i ] = NULL;
    }
    iPathnameCount  = 0;

    //
    //  Allocate Buffer for Check Clusters
    lpVolumeLcnInfo = ( PVOLUME_DISK_INFO ) malloc ( sizeof(VOLUME_DISK_INFO) );
    ZeroMemory ( lpVolumeLcnInfo, sizeof ( VOLUME_DISK_INFO ) );

    iFileLcnSize = sizeof ( FILE_LCN ) + ( iFileLcnMax - 1 )  * sizeof(LCN);
    lpFileLcnBefore = ( PFILE_LCN ) malloc ( iFileLcnSize );
    ZeroMemory ( lpFileLcnBefore, iFileLcnSize );

    lpFileLcnAfter = ( PFILE_LCN ) malloc ( iFileLcnSize );
    ZeroMemory ( lpFileLcnAfter, iFileLcnSize );

    //
    //  Strings
    ZeroMemory ( szFillWithFile, sizeof(szFillWithFile) );
    ZeroMemory ( szJpegext, sizeof(szJpegext) );
    ZeroMemory ( szCreateJpgFile, sizeof(szCreateJpgFile) );
    ZeroMemory ( szFilter, sizeof(szFilter) );
    ZeroMemory ( szNoMatch, sizeof(szNoMatch) );
    ZeroMemory ( szOutputFile, sizeof(szOutputFile) );

    //
    ZeroMemory ( szCurrentDirectory, sizeof ( szCurrentDirectory ) );
    GetCurrentDirectory ( sizeof ( szCurrentDirectory ) / sizeof(WCHAR), szCurrentDirectory );

    //  Fill Filename
    ZeroMemory ( szVariable, sizeof ( szVariable ) );
    DWORD dwVariable = GetEnvironmentVariable ( WSRM_FILL_FILENAME, szVariable, sizeof ( szVariable ) / sizeof(WCHAR) );
    if ( dwVariable )
    {
        ZeroMemory ( szFillWithFile, sizeof(szFillWithFile) );
        wcscpy_s ( szFillWithFile, sizeof(szFillWithFile), szVariable );
        bFillWithFileEnv = TRUE;
    }

    //  Jpeg Text
    ZeroMemory ( szVariable, sizeof ( szVariable ) );
    dwVariable = GetEnvironmentVariable ( WSRM_JPEG_TEXT, szVariable, sizeof ( szVariable ) / sizeof(WCHAR) );
    if ( dwVariable )
    {
        ZeroMemory ( szJpegext, sizeof(szJpegext) );
        wcscpy_s ( szJpegext, sizeof(szJpegext), szVariable );
        bJpegTextEnv = TRUE;
    }

    //
    //  Get Interactive
    ZeroMemory ( szVariable, sizeof ( szVariable ) );
    dwVariable = GetEnvironmentVariable ( WSRM_INTERACTIVE, szVariable, sizeof ( szVariable ) / sizeof(WCHAR) );
    if ( dwVariable )
    {
        if ( _wcsicmp ( szVariable, YES ) == 0 )
        {
            bInteractiveEnv = TRUE;
        }
    }

    //
    //  Get No Banner
    ZeroMemory ( szVariable, sizeof ( szVariable ) );
    dwVariable = GetEnvironmentVariable ( WSRM_NO_BANNER, szVariable, sizeof ( szVariable ) / sizeof(WCHAR) );
    if ( dwVariable )
    {
        if ( _wcsicmp ( szVariable, YES ) == 0 )
        {
            bNoBannerEnv = TRUE;
        }
    }

    //
    treatOptions ( iArgCount, pArgValues );

    //
    if ( wcslen(LocaleString) > 0 )
    {
        CreateLocalePointer ( LocaleString );
    }


    //
    if ( ! ( bNoBanner || bNoBannerEnv ) )
    {
        printBanner();
    }

    //
    initBuffers ( -1 );

    //
    //  If fill with file : read the file
    if ( bFillWithFile || bFillWithFileEnv )
    {
        readFillFile();
    }

    //
    //  If fill with file : read the file
    if ( bJpegText || bJpegTextEnv )
    {
        CreateImage ( IMG_WIDTH, IMG_HEIGHT, szJpegext);
    }

    //
    if ( Action == Action_ShowDeletedFilesXE )
    {
        WCHAR szDrive = L'C';
        WCHAR drive = szDriveVolume [ 0 ];
        if ( drive >= L'A' && drive <= L'Z' )
        {
            szDrive = drive;
        }
        XEMain ( *szDriveVolume, TRUE, FALSE, 0, NULL );
    }

    //
    else if ( Action == Action_ShowDeletedFiles )
    {
        //
        ListFiles ( szDriveVolume, lpVolumeLcnInfo, FALSE, TRUE, GET_VOLUME_DATA_DO, szFilter, szNoMatch );
    }

    //
    else if ( Action == Action_ShowAllFiles )
    {
        //
        ListFiles ( szDriveVolume, lpVolumeLcnInfo, FALSE, FALSE, GET_VOLUME_DATA_DO, szFilter, szNoMatch );
    }

    //
    else if ( Action == Action_ShowDirectories )
    {
        //
        ListFiles ( szDriveVolume, lpVolumeLcnInfo, TRUE, FALSE, GET_VOLUME_DATA_DO, szFilter, szNoMatch );
    }

    //
    else if ( Action == Action_FileInfo )
    {
        TraceMode = TRUE;
        VerboseMode = TRUE;
        BOOL bResult = GetFileInformation ( szDriveVolume, lpVolumeLcnInfo, GET_VOLUME_DATA_DO, iFileNumber );
    }

    //
    else if ( Action == Action_LocateFile )
    {
        BOOL bResult = LocateFile ( szDriveVolume, lpVolumeLcnInfo, GET_VOLUME_DATA_DO, iFileNumber );
    }

    //
    else if ( ( Action == Action_Copy ) && bOutput )
    {
        BOOL bResult = CopyFileInformation ( szDriveVolume, lpVolumeLcnInfo, GET_VOLUME_DATA_DO, iFileNumber, szOutputFile );
    }

    //
    else if ( Action == Action_FillDisk )
    {
        int iSize = (int) wcslen ( szDriveVolume )  + sizeof(WCHAR);
        LPSTR multiBytes = ( CHAR * ) malloc ( iSize  );
        ZeroMemory ( multiBytes, iSize );

        BOOL    bUsedDefaultChar;
        int iRes = WideCharToMultiByte(
            CP_ACP,             //  _In_       UINT CodePage,
            NULL,               //  _In_       DWORD dwFlags,
            szDriveVolume,  //  _In_       LPCWSTR lpWideCharStr,
            -1,                 //  _In_       int cchWideChar,
            multiBytes,         //  _Out_opt_  LPSTR lpMultiByteStr,
            iSize,              //  _In_       int cbMultiByte,
            NULL,               //  _In_opt_   LPCSTR lpDefaultChar,
            &bUsedDefaultChar   // _Out_opt_  LPBOOL lpUsedDefaultChar
        );

        BOOL bResult = FillOneDisk ( multiBytes );
    }

    //
    else if ( ( Action == Action_FillName ) && bCountItems )
    {
        FillNames ( szDriveVolume, iCountItems );
    }

    //
    else if ( Action == Action_DeletePath )
    {
        for ( int i = 0; i < iPathnameCount; i++ )
        {
            if (  pPathnameTable [ i ] != NULL )
            {
                deletePath ( pPathnameTable [ i ], 0 );
            }
            else
            {
                PrintStderrW ( L"Error - Table Item Is NULL\n" );
            }
        }
    }

    //
    else
    {
        PrintStderrW ( L"Error - Invalid Options\n" );
    }

    //
    freeBuffers ();

    //
    if ( lpVolumeLcnInfo != NULL )
    {
        if ( lpVolumeLcnInfo->pMFTRecordOututBuffer != NULL )
        {
            free ( lpVolumeLcnInfo->pMFTRecordOututBuffer );
        }
        free ( lpVolumeLcnInfo );
    }

    if ( lpFileLcnBefore != NULL )
    {
        free ( lpFileLcnBefore );
    }

    if ( lpFileLcnAfter != NULL )
    {
        free ( lpFileLcnAfter );
    }

    //
    for ( int i = 0; i < iPathnameMax; i++ )
    {
        if ( pPathnameTable [ i ] != NULL )
        {
            free ( pPathnameTable [ i ] );
            pPathnameTable [ i ] = NULL;
        }
    }

    //
    exit (0);
}

