//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <Windows.h>
#include <winioctl.h>

#include <math.h>
#include <stdio.h>

#include "srm.h"
#include "DiskFileInfos.h"
#include "PrintRoutine.h"
#include "strstr.h"

#define PREFIX_DSK          L"\\\\.\\"
#define PREFIX_VOL          L"\\\\?\\"

#define LEN_DRIVE_STRING    MAX_PATH
#define LEN_NUMBER_STRING   64
#define LEN_NUMBER          32

#define ONE_KILO            1024
#define ONE_KILO_DOUBLE     ( double ) ( ONE_KILO )
#define ONE_MEGA            (ONE_KILO*ONE_KILO)

//
#define LEN_FORMATTED       256
static WCHAR                szFormatted [ LEN_FORMATTED ];
#define LEN_LINE            4096
static WCHAR                szLine [ LEN_LINE ];

//
#define                     LEN_PATHNAME    4096
static WCHAR                szFilename [ LEN_PATHNAME ];
static WCHAR                szLongFilename [ LEN_PATHNAME ];
static WCHAR                szShortFilename [ LEN_PATHNAME ];
static WCHAR                szOutputFilename [ LEN_PATHNAME ];

//
static WCHAR                szAttributeName [ LEN_PATHNAME ];
static WCHAR                szFileAttribute [ LEN_PATHNAME ];

#define LEN_DUMP_LINE       128
static WCHAR                szDumpLineW [ LEN_DUMP_LINE ];
static CHAR                 szDumpLineA [ LEN_DUMP_LINE ];

//
#define MAX_SHORT_DUMP      64

//
typedef enum {
        DUMP_DATA_NONE  = 0,
        DUMP_DATA_SHORT = 1,
        DUMP_DATA_ONE   = 2,
        DUMP_DATA_ALL   = 3
} DUMP_DATA;

//
typedef enum {
        SHOW_DATA_NO    = 0,
        SHOW_DATA_DO    = 1
} SHOW_DATA;

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
double Round ( double dfValue )
{
    dfValue += 0.5;
    return dfValue;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *SeparatedNumberType ( LARGE_INTEGER li )
{
    static WCHAR szTemp1 [ LEN_NUMBER_STRING ];
    SeparatedNumber ( szTemp1, _wsizeof(szTemp1), li, L' ' );
    return szTemp1;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *SeparatedNumberType ( long long li )
{
    static WCHAR szTemp1 [ LEN_NUMBER_STRING ];
    SeparatedNumber ( szTemp1, _wsizeof(szTemp1), li, L' ' );
    return szTemp1;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL isTypeValid ( PFILE_RECORD_HEADER pFileRecordHeader )
{
    char szType [ 5 ];
    ZeroMemory ( szType, sizeof ( szType ) );
    memcpy ( szType, pFileRecordHeader->RecHdr.Type.szType, 4 );

    //  ‘FILE’ ‘INDX’ ‘BAAD’ ‘HOLE’ ‘CHKD’
    if ( memcmp (szType, "FILE", sizeof( szType ) - 1 ) == 0 )
    {
        return TRUE;
    }
    else if ( memcmp (szType, "INDX", sizeof( szType ) - 1 ) == 0 )
    {
        return TRUE;
    }
    else if ( memcmp (szType, "BAAD", sizeof( szType ) - 1 ) == 0 )
    {
        return TRUE;
    }
    else if ( memcmp (szType, "HOLE", sizeof( szType ) - 1 ) == 0 )
    {
        return TRUE;
    }
    else if ( memcmp (szType, "CHKD", sizeof( szType ) - 1 ) == 0 )
    {
        return TRUE;
    }

    PrintTraceA ( "Error - isTypeValid - Not A Valid Type '%s' %02x%02x%02x%02x\n", szType,
        (unsigned char) szType[0], (unsigned char) szType[1], (unsigned char) szType[2], (unsigned char) szType[3] );

    return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL isTypeValid ( PNTFS_FILE_RECORD_OUTPUT_BUFFER ntfsFileRecordOutput )
{
    //
    PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &ntfsFileRecordOutput->FileRecordBuffer[0] );

    return isTypeValid ( pFileRecordHeader );

}


//
//////////////////////////////////////////////////////////////////////////////
//  Move iOffset Byte from pBuffer
//////////////////////////////////////////////////////////////////////////////
PVOID MoveToOffset ( PVOID pBuffer, ULONG iOffset )
{
    return ( PBYTE ) pBuffer + iOffset;
}

PVOID MoveToOffset ( PVOID pBuffer, USHORT iOffset )
{
    return ( PBYTE ) pBuffer + iOffset;
}

PVOID MoveToOffset ( PVOID pBuffer, int iOffset )
{
    return ( PBYTE ) pBuffer + iOffset;
}

//
//////////////////////////////////////////////////////////////////////////////
//  RUN Length
//////////////////////////////////////////////////////////////////////////////
static ULONG RunLength(PUCHAR run)
{
    return ( *run & 0xf ) + ( ( *run >> 4 ) & 0xf ) + 1;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get LCN from RUN
//////////////////////////////////////////////////////////////////////////////
static LONGLONG RunLCN(PUCHAR run)
{
    UCHAR n1 = *run & 0xf;
    UCHAR n2 = ( *run >> 4 ) & 0xf;
    LONGLONG lcn = n2 == 0 ? 0 : CHAR( run [ n1 + n2 ] );
    for ( LONG i = n1 + n2 - 1; i > n1; i-- )
    {
        lcn = ( lcn << 8 ) + run [ i ];
    }
    return lcn;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Return RUN Count
//////////////////////////////////////////////////////////////////////////////
ULONGLONG RunCount(PUCHAR run)
{
    UCHAR n = *run & 0xf;
    ULONGLONG count = 0;
    for (ULONG i = n; i > 0; i-- )
    {
        count = ( count << 8 ) + run [ i ];
    }
    return count;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL FindRun(PNONRESIDENT_ATTRIBUTE pNonResident, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count)
{
    if ( vcn < pNonResident->LowVcn || vcn > pNonResident->HighVcn )
    {
        return FALSE;
    }

    *lcn = 0;
    ULONGLONG base = pNonResident->LowVcn;
    PUCHAR puStart = (PUCHAR) MoveToOffset(pNonResident, pNonResident->RunArrayOffset);
    for (   PUCHAR run = puStart; *run != 0; run += RunLength(run) )
    {
        *lcn    += RunLCN(run);
        *count  = RunCount(run);
        if ( base <= vcn && vcn < base + *count ) {
            *lcn    = RunLCN(run) == 0 ? 0 : *lcn + vcn - base;
            *count  -= ULONG(vcn - base);
            return TRUE;
        }
        else
        {
            base += *count;
        }
    }
    return FALSE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
PATTRIBUTE FindAttribute ( PFILE_RECORD_HEADER fileRecordHeader, ATTRIBUTE_TYPE type, PWSTR name, int index )
{
    //
    int count = 0;
    PATTRIBUTE pStart = (PATTRIBUTE) MoveToOffset ( fileRecordHeader, fileRecordHeader->AttributeOffset);
    for (   PATTRIBUTE pAttribute = pStart;
            pAttribute->AttributeType != -1;
            pAttribute = (PATTRIBUTE) MoveToOffset(pAttribute, pAttribute->Length)) {

        if ( pAttribute->AttributeType == type ) {

            //  No Name
            if (name == 0 && pAttribute->NameLength == 0)
            {
                if ( count >= index )
                {
                    return pAttribute;
                }
                count++;
            }

            //  With a Name
            if (    name != 0 && wcslen(name) == pAttribute->NameLength &&
                    _wcsicmp(name, PWSTR(MoveToOffset(pAttribute, pAttribute->NameOffset))) == 0 )
            {
                if ( count >= index )
                {
                    return pAttribute;
                }
                count++;
            }
        }
    }

    return 0;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
ULONGLONG GetVCNFromIndex (ULONG index, PVOLUME_DISK_INFO lpVolumeLcnInfo )
{
    ULONGLONG vcn =
        ULONGLONG(index) * lpVolumeLcnInfo->lBytesPerFileRecord / lpVolumeLcnInfo->BytesPerSector / lpVolumeLcnInfo->SectorsPerCluster;
    return vcn;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
ULONGLONG GetAttributeLength ( PATTRIBUTE attribute )
{
    if ( attribute->Nonresident == FALSE )
    {
        PRESIDENT_ATTRIBUTE pResident = (PRESIDENT_ATTRIBUTE) attribute;
        return pResident->ValueLength;
    }
    else
    {
        PNONRESIDENT_ATTRIBUTE pNonResident = (PNONRESIDENT_ATTRIBUTE) attribute;
        return pNonResident->DataSize;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
ULONGLONG GetAttributeAllocated ( PATTRIBUTE attribute )
{
    if ( attribute->Nonresident == FALSE )
    {
        PRESIDENT_ATTRIBUTE pResident = (PRESIDENT_ATTRIBUTE) attribute;
        return pResident->ValueLength;
    }
    else
    {
        PNONRESIDENT_ATTRIBUTE pNonResident = (PNONRESIDENT_ATTRIBUTE) attribute;
        return pNonResident->AllocatedSize;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID FixupUpdateSequenceArray ( PFILE_RECORD_HEADER fileRecordHeader)
{
    PUSHORT usa = PUSHORT(MoveToOffset(fileRecordHeader, fileRecordHeader->RecHdr.UsaOffset));
    PUSHORT sector = PUSHORT(fileRecordHeader);
    for ( ULONG i = 1; i < fileRecordHeader->RecHdr.UsaCount; i++ ) {
        sector[255] = usa[i];
        sector += 256;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadSector (   HANDLE hVolume,
                    PVOLUME_DISK_INFO lpVolumeLcnInfo,
                    ULONGLONG sector, ULONG count, PVOID buffer )
{
    BOOL bResult = TRUE;

    ULARGE_INTEGER offset;
    OVERLAPPED overlap = {0};
    offset.QuadPart = sector * lpVolumeLcnInfo->BytesPerSector;
    overlap.Offset = offset.LowPart;
    overlap.OffsetHigh = offset.HighPart;

    ULONG dwReturned;
    bResult = ReadFile ( hVolume, buffer, count * lpVolumeLcnInfo->BytesPerSector, &dwReturned, &overlap );
    if ( ! bResult )
    {
        PrintStderrW ( L"Error - ReadFile Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText () );
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadLCN (  HANDLE hVolume,
                PVOLUME_DISK_INFO lpVolumeLcnInfo,
                ULONGLONG lcn, ULONG count, PVOID buffer )
{
    BOOL bResult =
        ReadSector ( hVolume, lpVolumeLcnInfo, lcn * lpVolumeLcnInfo->SectorsPerCluster, count * lpVolumeLcnInfo->SectorsPerCluster, buffer );
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadExternalAttribute (    HANDLE hVolume,
                                PVOLUME_DISK_INFO lpVolumeLcnInfo,
                                PNONRESIDENT_ATTRIBUTE pNonResident,
                                ULONGLONG vcn, ULONG count, PVOID pBuffer )
{
    BOOL bResult = TRUE;

    ULONGLONG lcn, RunCount;
    ULONG readcount, left;
    PUCHAR bytes = PUCHAR(pBuffer);
    for ( left = count; left > 0; left -= readcount )
    {
        FindRun ( pNonResident, vcn, &lcn, &RunCount );
        readcount = ULONG(min(RunCount, left));
        ULONG numberOfBytes = readcount * lpVolumeLcnInfo->BytesPerSector * lpVolumeLcnInfo->SectorsPerCluster;
        if (lcn == 0)
        {
            memset ( bytes, 0, numberOfBytes );
        }
        else
        {
            bResult = ReadLCN ( hVolume, lpVolumeLcnInfo, lcn, readcount, bytes );
        }
        vcn += readcount;
        bytes += numberOfBytes;
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadAttribute (    HANDLE hVolume,
                        PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        PATTRIBUTE attribute, PVOID pBuffer )
{
    BOOL bResult = TRUE;

    if ( attribute->Nonresident == FALSE )
    {
        PRESIDENT_ATTRIBUTE pResident = ( PRESIDENT_ATTRIBUTE ) attribute;
        PVOID pVoid = MoveToOffset ( pResident, pResident->ValueOffset );
        memcpy ( pBuffer, pVoid, pResident->ValueLength );
    }
    else
    {
        PNONRESIDENT_ATTRIBUTE pNonResident = (PNONRESIDENT_ATTRIBUTE) attribute;

        //  Here We Will have to read t from disk
        bResult = ReadExternalAttribute ( hVolume, lpVolumeLcnInfo, pNonResident, 0, ULONG(pNonResident->HighVcn) + 1, pBuffer );
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadVCN (  HANDLE hVolume,
                PVOLUME_DISK_INFO lpVolumeLcnInfo,
                PFILE_RECORD_HEADER fileRecordHeader,
                ATTRIBUTE_TYPE type,
                ULONGLONG vcn, ULONG count, PVOID pBuffer )
{
    BOOL bResult = TRUE;

    PNONRESIDENT_ATTRIBUTE pNonResident = PNONRESIDENT_ATTRIBUTE(FindAttribute ( fileRecordHeader, type, 0, 0 ) );

    //
    //  Not Non Resident or vcn out of range
    if ( pNonResident == 0 || (vcn < pNonResident->LowVcn || vcn > pNonResident->HighVcn ) )
    {
        //  Support for huge fileRecordHeaders
        PATTRIBUTE pNonResidentlist = FindAttribute(fileRecordHeader, AttributeAttributeList, 0, 0);

        //  Mainly an Error
        return FALSE;
//      DebugBreak();
    }

    //
    bResult = ReadExternalAttribute( hVolume, lpVolumeLcnInfo, pNonResident, vcn, count, pBuffer);

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ReadFileRecord (   HANDLE hVolume,
                        PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        ULONG index, PFILE_RECORD_HEADER fileRecordHeader )
{
    BOOL bResult = TRUE;

    ULONG clusters = lpVolumeLcnInfo->ClustersPerFileRecord;
    if (clusters > 0x80)
    {
        clusters = 1;
    }

    //
    //  Allocate a buffer
    LONG lBufferSize = lpVolumeLcnInfo->BytesPerSector * lpVolumeLcnInfo->SectorsPerCluster * clusters;
    PUCHAR pBuffer = ( PUCHAR ) malloc ( lBufferSize );
    ZeroMemory ( pBuffer, lBufferSize );

    //  Compute VCN
    ULONGLONG vcn = ULONGLONG(index) * lpVolumeLcnInfo->lBytesPerFileRecord / lpVolumeLcnInfo->BytesPerSector /  lpVolumeLcnInfo->SectorsPerCluster;

    //
    //  Read VCN
    bResult = ReadVCN ( hVolume, lpVolumeLcnInfo, lpVolumeLcnInfo->pMFTRecordHeader, AttributeData, vcn, clusters, pBuffer );
    if ( bResult )
    {
        LONG m = (lpVolumeLcnInfo->SectorsPerCluster * lpVolumeLcnInfo->BytesPerSector / lpVolumeLcnInfo->lBytesPerFileRecord) - 1;
        ULONG n = m > 0 ? (index & m) : 0;

        //  Copy Buffer
        memcpy(fileRecordHeader, pBuffer + n * lpVolumeLcnInfo->lBytesPerFileRecord, lpVolumeLcnInfo->lBytesPerFileRecord);

        //
        FixupUpdateSequenceArray ( fileRecordHeader );

    }

    free ( pBuffer );

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
PATTRIBUTE GetAttributeByIndex ( PFILE_RECORD_HEADER pFileRecordHeader , int attributeIndex )
{
    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - 1;
    BOOL bContinue = TRUE;
    int currentIndex = 0;
    while ( offset < maxOffset  && bContinue  )
    {
        char *pStartAttribute = ( char * ) pFileRecordHeader + offset;
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;
        if ( attributeIndex == currentIndex )
        {
            return pAttribute;
        }
        currentIndex++;

        if ( pAttribute->Length == 0 )
        {
            break;
        }

        offset += pAttribute->Length;
    }

    return NULL;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
PATTRIBUTE GetAttributeByIndex ( PNTFS_FILE_RECORD_OUTPUT_BUFFER ntfsFileRecordOutput , int attributeIndex )
{
    //
    PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &ntfsFileRecordOutput->FileRecordBuffer[0] );

    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) - 1;
    BOOL bContinue = TRUE;
    int currentIndex = 0;
    while ( offset < maxOffset  && bContinue  )
    {
        char *pStartAttribute = ( char * ) & ntfsFileRecordOutput->FileRecordBuffer[offset];
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;
        if ( attributeIndex == currentIndex )
        {
            return pAttribute;
        }

        currentIndex++;

        if ( pAttribute->Length == 0 )
        {
            break;
        }

        offset += pAttribute->Length;
    }

    return NULL;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
PATTRIBUTE GetAttributeByType ( PFILE_RECORD_HEADER pFileRecordHeader,
                                ATTRIBUTE_TYPE attributeType, int attributeIndex )
{
    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - 1;
    BOOL bContinue = TRUE;
    int currentIndex = 0;
    while ( offset < maxOffset  && bContinue  )
    {
        char *pStartAttribute = ( char * ) pFileRecordHeader + offset;
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;
        if ( pAttribute->AttributeType == attributeType )
        {
            if ( attributeIndex == currentIndex )
            {
                return pAttribute;
            }
            currentIndex++;
        }

        if ( pAttribute->Length == 0 )
        {
            break;
        }

        offset += pAttribute->Length;
    }

    return NULL;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
PATTRIBUTE GetAttributeByType ( PNTFS_FILE_RECORD_OUTPUT_BUFFER ntfsFileRecordOutput,
                                ATTRIBUTE_TYPE attributeType, int attributeIndex )
{
    //
    PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &ntfsFileRecordOutput->FileRecordBuffer[0] );

    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) - 1;
    BOOL bContinue = TRUE;
    int currentIndex = 0;
    while ( offset < maxOffset  && bContinue  )
    {
        char *pStartAttribute = ( char * ) & ntfsFileRecordOutput->FileRecordBuffer[offset];
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;
        if ( pAttribute->AttributeType == attributeType )
        {
            if ( attributeIndex == currentIndex )
            {
                return pAttribute;
            }
            currentIndex++;
        }

        if ( pAttribute->Length == 0 )
        {
            break;
        }

        offset += pAttribute->Length;
    }

    return NULL;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ShowDetailLineW ( WCHAR pSeparator, WCHAR *pLabel, WCHAR *format, ... )
{
    PrintTraceW ( L"DiskFileInfos %c ", pSeparator );
    PrintTraceW ( L"%-32s : ", pLabel );

    //
    if ( TraceMode )
    {
        va_list vaList;
        va_start(vaList, format);
        int result = PrintVW ( format, vaList );
        va_end(vaList);
    }

    //
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ShowDetailLineA ( CHAR pSeparator, CHAR *pLabel, CHAR *format, ... )
{
    PrintTraceA ( "DiskFileInfos %c ", pSeparator );
    PrintTraceA ( "%-32s : ", pLabel );

    //
    if ( TraceMode )
    {
        va_list vaList;
        va_start(vaList, format);
        int result = PrintVA ( format, vaList );
        va_end(vaList);
    }

    //
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetFileOffset ( HANDLE hFile, PVOLUME_DISK_INFO lpVolumeLcnInfo, PFILE_LCN lpFileLcn, int iLcnMax )
{
    if ( NULL == hFile )
    {
        PrintStderrW ( L"Error - hFile is null\n" );
        return FALSE;
    }

    //
    STARTING_VCN_INPUT_BUFFER  startingVcnInputBuffer;
    DWORD   iRetrievalPointersBuffer = sizeof(RETRIEVAL_POINTERS_BUFFER) + ( iLcnMax - 1 ) * 2 * sizeof(LARGE_INTEGER);
    // iRetrievalPointersBuffer = sizeof(RETRIEVAL_POINTERS_BUFFER) + ( ( 1 + iLcnMax / 4 ) - 1 ) * 2 * sizeof(LARGE_INTEGER);
    PRETRIEVAL_POINTERS_BUFFER  pRetrievalPointersBuffer = ( PRETRIEVAL_POINTERS_BUFFER ) malloc ( iRetrievalPointersBuffer ) ;

    //
    ZeroMemory ( &startingVcnInputBuffer, sizeof (startingVcnInputBuffer) );
    ZeroMemory ( pRetrievalPointersBuffer, iRetrievalPointersBuffer );

    //
    DWORD   dwError = 0;

    //
    lpFileLcn->lLcnCount    = 0;

    //
    BOOL    bContinue = TRUE;

    //
    LONGLONG    llTotalLength = 0;

    //
    startingVcnInputBuffer.StartingVcn.QuadPart = 0;

    //
    // every time its return only one Extent, if there are many ,you need get many times
    do
    {
        //
        //  Reset Buffer
        ZeroMemory ( pRetrievalPointersBuffer, iRetrievalPointersBuffer );

        //
        DWORD                      dwReturnedBytes = 0;

        //
        BOOL bResult = DeviceIoControl ( hFile,
            FSCTL_GET_RETRIEVAL_POINTERS,
            &startingVcnInputBuffer,
            sizeof(STARTING_VCN_INPUT_BUFFER),
            pRetrievalPointersBuffer,
            iRetrievalPointersBuffer,
            &dwReturnedBytes,
            NULL );

        //
        dwError = GetLastError();
        if ( bResult || ( ! bResult && ( dwError == NO_ERROR ||  dwError == ERROR_MORE_DATA || dwError == ERROR_HANDLE_EOF  ) ) )
        {
            switch ( dwError )
            {
                //
                //  https://social.technet.microsoft.com/Forums/windowsserver/en-US/1a16311b-c625-46cf-830b-6a26af488435/how-to-solve-error-38-0x26-errorhandleeof-using-fsctlgetretrievalpointers?forum=winserverfiles
                //  Very Small File can be store in the MFT
                case ERROR_HANDLE_EOF:
                {
                    ShowDetailLineW ( L':', L"File Record Is End Of", L"\n", L"" );
                    bContinue = FALSE;
                    break;
                }

                //
                //  The Buffer Can Hold only One Entry : So Treat It and Loop
                case ERROR_MORE_DATA:
                {
                    //  Continue to Next case
                }

                //
                case NO_ERROR:
                {
                    ShowDetailLineW ( L':', L"Retrieval Extent Count", L"%12ld\n", pRetrievalPointersBuffer->ExtentCount );
                    ShowDetailLineW ( L':', L"Starting VCN is", L"%12lld\n", pRetrievalPointersBuffer->StartingVcn.QuadPart  );

                    //
                    if ( lpFileLcn->lLcnCount >= iLcnMax )
                    {
                        //
                        //  We Dont Read Any More
                        ShowDetailLineW ( L':', L"We Have Reach Our Maximum", L"%d\n", iLcnMax );
                        bContinue = FALSE;
                        break;
                    }

                    //
                    LONGLONG startingVCN =  pRetrievalPointersBuffer->StartingVcn.QuadPart;
                    for ( DWORD i = 0; i < pRetrievalPointersBuffer->ExtentCount && lpFileLcn->lLcnCount < iLcnMax; i++ )
                    {

                        LONGLONG llOffset =
                            pRetrievalPointersBuffer->Extents [ i ].Lcn.QuadPart * (LONGLONG) lpVolumeLcnInfo->SectorsPerCluster * (LONGLONG)lpVolumeLcnInfo->BytesPerSector
                            + (LONGLONG)lpVolumeLcnInfo->llVolumeOffset;
                        LONGLONG llLength =
                            ( pRetrievalPointersBuffer->Extents[ i ].NextVcn.QuadPart - startingVCN ) * lpVolumeLcnInfo->SectorsPerCluster * lpVolumeLcnInfo->BytesPerSector;

                        lpFileLcn->lcnOffsetAndLength [ lpFileLcn->lLcnCount ].llLcnOffset  = llOffset;
                        lpFileLcn->lcnOffsetAndLength [ lpFileLcn->lLcnCount ].llLcnLength  = llLength;

                        WCHAR szOffset [ LEN_NUMBER_STRING ];
                        HumanReadable ( szOffset, sizeof ( szOffset ) / sizeof(WCHAR), llOffset );

                        WCHAR szLength [ LEN_NUMBER_STRING ];
                        HumanReadable ( szLength, sizeof ( szLength ) / sizeof(WCHAR), llLength );

                        ShowDetailLineW ( L':', L"VCN - Offset - Length ", L"#%02ld  %12lld (%6s) %12lld (%6s) - (%12lld - %12lld)\n",
                            lpFileLcn->lLcnCount, llOffset, szOffset, llLength, szLength,
                            pRetrievalPointersBuffer->Extents [ i ].Lcn.QuadPart,
                            ( pRetrievalPointersBuffer->Extents[ i ].NextVcn.QuadPart - startingVCN ) );

                        //
                        //  THe Length is relative from previous VCN
                        startingVCN = pRetrievalPointersBuffer->Extents[ i ].NextVcn.QuadPart;

                        //
                        lpFileLcn->lLcnCount++;

                        //
                        llTotalLength += llLength;

                        //
                        //  For The Next Seacrh
                        startingVcnInputBuffer.StartingVcn = pRetrievalPointersBuffer->Extents[ i ].NextVcn;
                    }

                    //
                    ShowDetailLineW ( L':', L"VCN Count Is", L"%15s\n", SeparatedNumberType ( lpFileLcn->lLcnCount )  );

                    break;
                }
                default:
                {
                    PrintStderrW ( L"Error - Error Code %ld - %s\n", dwError, GetLastErrorText () );
                    bContinue = FALSE;
                    break;
                }
            };

        }

    } while ( ERROR_MORE_DATA == dwError && bContinue );

    WCHAR szLength [ LEN_NUMBER_STRING ];
    HumanReadable ( szLength, sizeof ( szLength ) / sizeof(WCHAR), llTotalLength );
    ShowDetailLineW ( L':', L"Total Length Is", L"%12lld (%6s)\n", llTotalLength, szLength  );

    free ( pRetrievalPointersBuffer );

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
HANDLE OpenExistingFile ( WCHAR *pPathname )
{
    if ( pPathname == NULL )
    {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hFile = CreateFile (
        pPathname,                              //  LPCTSTR lpFileName
        GENERIC_READ,                           //  DWORD dwDesiredAccess
        FILE_SHARE_READ,                        //  DWORD dwShareMode
        NULL,                                   //  LPSECURITY_ATTRIBUTES lpSecurityAttributes
        OPEN_EXISTING,                          //  DWORD dwCreationDisposition
        FILE_ATTRIBUTE_NORMAL,                  //  DWORD dwFlagsAndAttributes
        NULL );                                 //  HANDLE hTemplateFile
    if ( INVALID_HANDLE_VALUE == hFile )
    {
        PrintStderrW ( L"Error - CreateFile OpenExistingFile %s Fails With 0x%lx - %s\n", pPathname, GetLastError(), GetLastErrorText() );
    }

    return hFile;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
HANDLE OpenLogicalDisk ( WCHAR * pDrive )
{
    if ( pDrive == NULL )
    {
        return INVALID_HANDLE_VALUE;
    }

    //
    HANDLE hLogicalDisk = CreateFile (
        pDrive,                                 //  LPCTSTR lpFileName
        FILE_READ_DATA,                         //  DWORD dwDesiredAccess GENERIC_READ
        FILE_SHARE_READ | FILE_SHARE_WRITE |
                        FILE_SHARE_DELETE,      //  DWORD dwShareMode FILE_SHARE_VALID_FLAGS
        NULL,                                   //  LPSECURITY_ATTRIBUTES lpSecurityAttributes
        OPEN_EXISTING,                          //  DWORD dwCreationDisposition
        0,                                      //  DWORD dwFlagsAndAttributes
        NULL );                                 //  HANDLE hTemplateFile

    if ( INVALID_HANDLE_VALUE == hLogicalDisk )
    {
        PrintStderrW ( L"Error - CreateFile OpenLogicalDisk %s Fails With  0x%lx - %s\n", pDrive, GetLastError(), GetLastErrorText() );
    }

    return hLogicalDisk;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Must be runned in adminstrator mode
//////////////////////////////////////////////////////////////////////////////
HANDLE OpenPhysicalDisk ( WCHAR * pDiskName )
{
    if ( pDiskName == NULL )
    {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hPhysicalDisk = CreateFile (
            pDiskName,                          //  LPCTSTR lpFileName
            GENERIC_READ,                       //  DWORD dwDesiredAccess
            0,                                  //  DWORD dwShareMode
            NULL,                               //  LPSECURITY_ATTRIBUTES lpSecurityAttributes
            OPEN_EXISTING,                      //  DWORD dwCreationDisposition
            0,                                  //  DWORD dwFlagsAndAttributes
            NULL );                             //  HANDLE hTemplateFile
    if ( INVALID_HANDLE_VALUE == hPhysicalDisk )
    {
        PrintStderrW ( L"Error - CreateFile OpenPhysicalDisk %s Fails With  0x%lx - %s\n", pDiskName, GetLastError(), GetLastErrorText() );
    }

    return hPhysicalDisk;

}
//
//////////////////////////////////////////////////////////////////////////////
//  open file ,and set file size.
//  if file size < 1KB, MFT can store the data engouth,
//  so FSCTL_GET_RETRIEVAL_POINTERS ioctl will failed by error code  ERROR_HANDLE_EOF.
//////////////////////////////////////////////////////////////////////////////
HANDLE OpenBitmapAndCreateMapping ( WCHAR *lpFullPathname )
{
    if ( NULL == lpFullPathname )
    {
        return NULL;
    }

    HANDLE hFile = NULL;
    HANDLE hMapFile = NULL;

    hFile = OpenExistingFile ( lpFullPathname );
    if ( INVALID_HANDLE_VALUE == hFile )
    {
        return FALSE;
    }

    //
    LARGE_INTEGER fileSize;
    fileSize.QuadPart = 0LL;
    BOOL bGetFileSize = GetFileSizeEx ( hFile, &fileSize );

    // High Part and Low Part of the Memory
    DWORD dwMaximumSizeHigh = 0;
    DWORD dwMaximumSizeLow  = 2 * ONE_MEGA;

    //
    if ( bGetFileSize && ( fileSize.QuadPart < dwMaximumSizeLow ) )
    {
        dwMaximumSizeLow = 0LL;
    }
    hMapFile = CreateFileMapping ( hFile, NULL, PAGE_READONLY | SEC_RESERVE, dwMaximumSizeHigh, dwMaximumSizeLow, NULL );
    if ( NULL == hMapFile )
    {
        PrintStderrW ( L"Error - CreateFileMapping %s Fails With 0x%lx - %s\n", lpFullPathname, GetLastError(), GetLastErrorText() );
    }

    if ( hMapFile )
    {
        CloseHandle(hMapFile);
    }

    return hFile;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *GetFileAttribute ( ULONG FileAttributes, WCHAR *pFileAttributes, size_t iSizeInBytes)
{
    ZeroMemory ( pFileAttributes, iSizeInBytes );
    if ( ( FileAttributes & FILE_ATTRIBUTE_READONLY ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"R" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_HIDDEN ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"H" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_SYSTEM ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"S" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_DIRECTORY ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"D" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_ARCHIVE ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"A" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_NORMAL ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"N" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_TEMPORARY ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"T" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"s" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"r" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_COMPRESSED ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"C" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_OFFLINE ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"O" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) !=  0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"I" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    if ( ( FileAttributes & FILE_ATTRIBUTE_ENCRYPTED ) != 0 )
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L"E" );
    }
    else
    {
        wcscat_s ( pFileAttributes, iSizeInBytes / sizeof(WCHAR), L" " );
    }

    return pFileAttributes;

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *GetAttributeName ( ATTRIBUTE_TYPE type )
{
    switch ( type )
    {
        case AttributeStandardInformation : { return L"AttributeStandardInformation"; }
        case AttributeAttributeList : { return L"AttributeAttributeList"; }
        case AttributeFileName : { return L"AttributeFileName"; }
        case AttributeObjectId : { return L"AttributeObjectId"; }
        case AttributeSecurityDescriptor : { return L"AttributeSecurityDescriptor"; }
        case AttributeVolumeName : { return L"AttributeVolumeName"; }
        case AttributeVolumeInformation : { return L"AttributeVolumeInformation"; }
        case AttributeData : { return L"AttributeData"; }
        case AttributeIndexRoot : { return L"AttributeIndexRoot"; }
        case AttributeIndexAllocation : { return L"AttributeIndexAllocation"; }
        case AttributeBitmap : { return L"AttributeBitmap"; }
        case AttributeReparsePoint : { return L"AttributeReparsePoint"; }
        case AttributeEAInformation : { return L"AttributeEAInformation"; }
        case AttributeEA : { return L"AttributeEA"; }
        case AttributePropertySet : { return L"AttributePropertySet"; }
        case AttributeLoggedUtilityStream : { return L"AttributeLoggedUtilityStream"; }
    }
    return L"AttributeUnknown";
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *ConvertDateTime ( ULONGLONG llFileTime, WCHAR *pSystemTime, BOOL bShortTime)
{
    SYSTEMTIME stUTC;
    FileTimeToSystemTime( ( FILETIME * ) &llFileTime, &stUTC );
    FILETIME    localTime;
    FileTimeToLocalFileTime ( ( FILETIME * ) &llFileTime, &localTime);
    FileTimeToSystemTime( &localTime, &stUTC );
    if ( bShortTime )
    {
        wsprintf ( pSystemTime, L"%02d/%02d/%04d %02d:%02d", stUTC.wDay, stUTC.wMonth, stUTC.wYear, stUTC.wHour, stUTC.wMinute );
    }
    else
    {
        wsprintf ( pSystemTime, L"%02d/%02d/%04d %02d:%02d:%02d", stUTC.wDay, stUTC.wMonth, stUTC.wYear, stUTC.wHour, stUTC.wMinute, stUTC.wSecond );
    }
    return pSystemTime;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetAttributeFilename ( PFILENAME_ATTRIBUTE pFilenameAttribute, WCHAR *pFilename, size_t sizeInBytes )
{
    ZeroMemory ( pFilename, sizeInBytes );
    memcpy ( pFilename, pFilenameAttribute->Name, min ( sizeInBytes / sizeof(WCHAR), pFilenameAttribute->NameLength * sizeof ( WCHAR ) ) );
    pFilename [ sizeInBytes / sizeof(WCHAR) - 1 ] = L'\0';
    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagCompressedToString ( USHORT flag )
{
    switch ( flag )
    {
        case 0x00 : return L"Not Compressed";
        case 0x01 : return L"Compressed";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagIndexedToString ( USHORT flag )
{
    switch ( flag )
    {
        case 0x00 : return L"Not Indexed";
        case 0x01 : return L"Indexed";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagInUseDirToString ( USHORT flag )
{
    switch ( flag )
    {
        case 0x00 : return L"<FIL> Not In Use";
        case 0x01 : return L"<FIL> In Use";
        case 0x02 : return L"<DIR> Not In Use";
        case 0x03 : return L"<DIR> In Use";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagFilenameToString ( UCHAR flag )
{
    switch ( flag )
    {
        case 0x01 : return L"Long";
        case 0x02 : return L"Short";
        case 0x03 : return L"Both";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagDirectoryToString ( ULONG flag )
{
    switch ( flag )
    {
        case 0x00 : return L"Small";
        case 0x01 : return L"Large";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
WCHAR *FlagNonResidentToString ( BOOLEAN flag )
{
    switch ( flag )
    {
        case 0x00 : return L"RESIDENT";
        case 0x01 : return L"NON RESIDENT";
        default : return L"<UNK>";
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL AddToOutputFile ( ULONG iFileNumber )
{
    if ( wcslen ( szLongFilename ) > 0 )
    {
        wcscat_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), szLongFilename );
    }
    else if ( wcslen ( szShortFilename ) > 0 )
    {
        wcscat_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), szShortFilename );
    }
    else
    {
        WCHAR szFileNumber [ LEN_NUMBER ];
        wnsprintf ( szFileNumber, sizeof(szFileNumber) / sizeof(WCHAR), L"%lu.bin", iFileNumber );
        wcscat_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), szFileNumber );
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
HANDLE OpenOutputFile ( WCHAR *pOutputFile, ULONG iFileNumber )
{
    HANDLE hFile = NULL;

    //
    if ( endsWith (  pOutputFile, L'\\' ) )
    {
        wcscpy_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), pOutputFile );
        AddToOutputFile ( iFileNumber );
    }
    else if ( isDirectory ( pOutputFile ) )
    {
        wcscpy_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), pOutputFile );
        wcscat_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), L"\\" );
        AddToOutputFile ( iFileNumber );
    }
    else
    {
        wcscpy_s ( szOutputFilename, sizeof(szOutputFilename) / sizeof(WCHAR), pOutputFile );
    }

    //
    hFile = CreateFile (
        szOutputFilename,                       //  LPCTSTR lpFileName
        GENERIC_WRITE,                          //  DWORD dwDesiredAccess
        0,                                      //  DWORD dwShareMode
        NULL,                                   //  LPSECURITY_ATTRIBUTES lpSecurityAttributes
        CREATE_NEW,                             //  DWORD dwCreationDisposition
        FILE_ATTRIBUTE_NORMAL,                  //  DWORD dwFlagsAndAttributes
        NULL );                                 //  HANDLE hTemplateFile
    if ( hFile == INVALID_HANDLE_VALUE )
    {
        PrintStderrW ( L"Error - CreateFile Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
    }

    return hFile;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Dump
//////////////////////////////////////////////////////////////////////////////
void Dump ( CHAR pSeparator, CHAR *pLabel, BYTE *pBuffer, int iSize, BOOL bOffset, BOOL bHexa, BOOL bAscii )
{
    int iStep = 16;

    //
    if ( pBuffer != NULL && iSize > 0 )
    {
        for ( int i = 0; i < iSize; i = i + iStep )
        {
            //
            ZeroMemory ( szDumpLineA, sizeof ( szDumpLineA ) );

            //
            if ( isAbortedByCtrlC() )
            {
                return;
            }

            //
            if ( bOffset )
            {
                sprintf_s ( szDumpLineA + strlen(szDumpLineA), sizeof(szDumpLineA) - strlen(szDumpLineA), "%04x : ", i );
            }
            for ( int j = i; j < i + iStep; j++ )
            {
                if ( j < iSize )
                {
                    sprintf_s ( szDumpLineA + strlen(szDumpLineA), sizeof(szDumpLineA) - strlen(szDumpLineA), "%02x ", pBuffer [ j ] );
                }
                else
                {
                    strcat_s ( szDumpLineA, sizeof(szDumpLineA), "__ " );
                }
            }

            if ( bAscii )
            {
                strcat_s ( szDumpLineA, sizeof(szDumpLineA), "- " );
                for ( int j = i; j < i + iStep; j++ )
                {
                    //
                    if ( isAbortedByCtrlC() )
                    {
                        return;
                    }

                    //
                    if ( j < iSize && pBuffer [ j ] >= ' ' && pBuffer [ j ] < 0x7f )
                    {
                        sprintf_s ( szDumpLineA + strlen(szDumpLineA), sizeof(szDumpLineA) - strlen(szDumpLineA), "%c", pBuffer [ j ] );
                    }
                    else
                    {
                        strcat_s ( szDumpLineA, sizeof(szDumpLineA), "." );
                    }
                }
            }

            ShowDetailLineA ( pSeparator, pLabel, "%s\n" , szDumpLineA );
        }
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL DumpNonResi (  CHAR pSeparator, CHAR *pLabel,
                    HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                    PNONRESIDENT_ATTRIBUTE pNonResident, DUMP_DATA iDumpData )
{
    if ( iDumpData > DUMP_DATA_NONE )
    {
        BOOL bContinueFor = TRUE;
        for ( ULONGLONG vcn = pNonResident->LowVcn; vcn < pNonResident->LowVcn + 1 && bContinueFor; vcn++ )
        {
            int sizeToRead = lpVolumeLcnInfo->SectorsPerCluster * lpVolumeLcnInfo->BytesPerSector;
            PBYTE pBuffer = ( PBYTE ) malloc ( sizeToRead );
            BOOL bResult = ReadExternalAttribute (  hVolume, lpVolumeLcnInfo, pNonResident, vcn, 1, pBuffer );
            if ( bResult )
            {
                if ( iDumpData == DUMP_DATA_SHORT ) // Short Dump
                {
                    sizeToRead = min ( MAX_SHORT_DUMP, sizeToRead );
                }
                Dump ( pSeparator, pLabel, pBuffer, sizeToRead, FALSE, TRUE, TRUE );
            }
            else
            {
                bContinueFor = FALSE;
            }
            free ( pBuffer );
        }
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//  uDimpData = 0 : no Dump
//              1 : One Record
//              2 : All Records
//////////////////////////////////////////////////////////////////////////////
BOOL ShowDetailRecord ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        PFILE_RECORD_HEADER pFileRecordHeader,
                        ULONG iFileNumber,
                        DUMP_DATA iDumpData  )
{
    //
    //
    if ( ! isTypeValid ( pFileRecordHeader ) )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szLongFilename, sizeof ( szLongFilename ) );
    ZeroMemory ( szShortFilename, sizeof ( szShortFilename ) );

    //
    ShowDetailLineW ( L':', L"RecHdr RecHdr Type", L"0x%lx\n", pFileRecordHeader->RecHdr.Type.uType );

    char szType [ 5 ];
    ZeroMemory ( szType, sizeof ( szType ) );
    memcpy ( szType, pFileRecordHeader->RecHdr.Type.szType, 4 );
    ShowDetailLineA ( ':', "RecHdr RecHdr Type", "%s\n", szType );
    
    ShowDetailLineW ( L':', L"RecHdr RecHdr UsaCount ", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->RecHdr.UsaCount ) );
    ShowDetailLineW ( L':', L"RecHdr RecHdr UsaOffset", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->RecHdr.UsaOffset ) );
    ShowDetailLineW ( L':', L"RecHdr RecHdr Usn", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->RecHdr.Usn ) );

    ShowDetailLineW ( L':', L"Sequence Number", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->SequenceNumber ) );
    ShowDetailLineW ( L':', L"Link Count", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->LinkCount) );
    ShowDetailLineW ( L':', L"Attribute Offset", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->AttributeOffset ) );
    ShowDetailLineW ( L':', L"Flags", L"0x%02x (%s) \n", pFileRecordHeader->Flags, FlagInUseDirToString(pFileRecordHeader->Flags) );
    ShowDetailLineW ( L':', L"Bytes In Use", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->BytesInUse ) );
    ShowDetailLineW ( L':', L"Bytes Allocated", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->BytesAllocated) );
    ShowDetailLineW ( L':', L"Base File Record", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->BaseFileRecord ) );
    ShowDetailLineW ( L':', L"Next Attribute Number", L"%15s\n", SeparatedNumberType ( pFileRecordHeader->NextAttributeNumber) );

    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) - 1;
    BOOL bContinue = TRUE;
    while ( offset < maxOffset  && bContinue  )
    {
        PrintTraceW ( L"\n" );

        char *pStartAttribute = ( char * ) MoveToOffset ( pFileRecordHeader, offset );
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;
        {
                ShowDetailLineW ( L':', L"Attribute", L"0x%0x\n", pAttribute->AttributeType );
                ShowDetailLineW ( L':', L"Attribute", L"%s\n", GetAttributeName ( pAttribute->AttributeType ) );
                ShowDetailLineW ( L':', L"Attribute Length", L"%15s\n", SeparatedNumberType ( pAttribute->Length ) );
                ShowDetailLineW ( L':', L"Attribute Nonresident", L"%d (%s)\n", pAttribute->Nonresident, FlagNonResidentToString(pAttribute->Nonresident) );
                ShowDetailLineW ( L':', L"Attribute NameOffset", L"%15s\n", SeparatedNumberType ( pAttribute->NameOffset ) );
                ShowDetailLineW ( L':', L"Attribute NameLength", L"%15s\n", SeparatedNumberType ( pAttribute->NameLength ) );
                if ( pAttribute->NameLength > 0 )
                {
                    WCHAR *pName = ( WCHAR * ) MoveToOffset ( pAttribute, pAttribute->NameOffset );
                    ZeroMemory ( szAttributeName, sizeof ( szAttributeName ) );
                    memcpy ( szAttributeName, pName, pAttribute->NameLength * sizeof(WCHAR) );
                    ShowDetailLineW ( L':', L"Attribute Name", L"%s\n", szAttributeName );
                }
                ShowDetailLineW ( L':', L"Attribute Flags", L"0x%02x (%s)\n", pAttribute->Flags, FlagCompressedToString (pAttribute->Flags) );
                ShowDetailLineW ( L':', L"Attribute Number", L"%15s\n", SeparatedNumberType ( pAttribute->AttributeNumber ) );
        }

        //
        PNONRESIDENT_ATTRIBUTE pNonResident = ( PNONRESIDENT_ATTRIBUTE )  ( ( char * ) pAttribute );
        PRESIDENT_ATTRIBUTE pResident       = ( PRESIDENT_ATTRIBUTE )  ( ( char * ) pAttribute );

        //
        if ( pAttribute->Nonresident )
        {
                ShowDetailLineW ( L'+', L"Non Resident Low Vcn", L"%15s\n", SeparatedNumberType ( pNonResident->LowVcn ) );
                ShowDetailLineW ( L'+', L"Non Resident High Vcn", L"%15s\n", SeparatedNumberType ( pNonResident->HighVcn ) );
                ShowDetailLineW ( L'+', L"Non Resident Run Array Offset", L"%15s\n", SeparatedNumberType ( pNonResident->RunArrayOffset ) );

                //  Analyze RunArray
                PUCHAR runnArray = (PUCHAR) MoveToOffset ( pNonResident, pNonResident->RunArrayOffset );

                //
                ShowDetailLineW ( L'+', L"Non Resident Compression Unit", L"%15s\n", SeparatedNumberType ( pNonResident->CompressionUnit ) );
                ShowDetailLineW ( L'+', L"Non Resident Allocated Size", L"%15s\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );
                ShowDetailLineW ( L'+', L"Non Resident Data Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                ShowDetailLineW ( L'+', L"Non Resident Initialized Size", L"%15s\n", SeparatedNumberType ( pNonResident->InitializedSize ) );

            if ( pAttribute->Flags & 0x01 )
            {
                ShowDetailLineW ( L'+', L"Non Resident Compressed Size", L"%15s\n", SeparatedNumberType ( pNonResident->CompressedSize ) );
            }
        }
        else
        {
                ShowDetailLineW ( L'-', L"Resident Value Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                ShowDetailLineW ( L'-', L"Resident Value Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );
                ShowDetailLineW ( L'-', L"Resident Flags", L"0x%02x (%s)\n", pResident->Flags, FlagIndexedToString(pResident->Flags) );
        }

        //
        switch ( pAttribute->AttributeType )
        {
            //
            case AttributeStandardInformation :
            {
                PSTANDARD_INFORMATION pValue = ( PSTANDARD_INFORMATION ) MoveToOffset ( pResident, pResident->ValueOffset );
                WCHAR szTime [ 32 ];

                ShowDetailLineW ( L'-', L"SI Creation Time", L"%llu %s\n", pValue->CreationTime, ConvertDateTime (  pValue->CreationTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"SI Change Time", L"%llu %s\n", pValue->ChangeTime, ConvertDateTime (  pValue->ChangeTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"SI Last Write Time", L"%llu %s\n", pValue->LastWriteTime, ConvertDateTime (  pValue->LastWriteTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"SI Last Access Time", L"%llu %s\n", pValue->LastAccessTime, ConvertDateTime (  pValue->LastAccessTime, szTime, FALSE ) );

                ShowDetailLineW ( L'-', L"SI File Attributes", L"%lu (%s)\n", pValue->FileAttributes,
                    GetFileAttribute ( pValue->FileAttributes, szFileAttribute, sizeof(szFileAttribute) ) );
                ShowDetailLineW ( L'-', L"SI QuotaId", L"%15s\n", SeparatedNumberType ( pValue->QuotaId ) );
                ShowDetailLineW ( L'-', L"SI SecurityId", L"%15s\n", SeparatedNumberType ( pValue->SecurityId ) );
                ShowDetailLineW ( L'-', L"SI Quota Charge", L"%15s\n", SeparatedNumberType ( pValue->QuotaCharge ) );
                break;
            }

            //
            case AttributeAttributeList :
            {
                PATTRIBUTE_LIST pValue = ( PATTRIBUTE_LIST ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"A List Type", L"%u\n", pValue->AttributeType );
                ShowDetailLineW ( L'-', L"A List Type", L"%s\n", GetAttributeName ( pValue->AttributeType ) );
                ShowDetailLineW ( L'-', L"A List Length", L"%15s\n", SeparatedNumberType ( pValue->Length ) );
                ShowDetailLineW ( L'-', L"A List Name Offset", L"%15s\n", SeparatedNumberType ( pValue->NameOffset ) );
                ShowDetailLineW ( L'-', L"A List Name Length", L"%15s\n", SeparatedNumberType ( pValue->NameLength ) );
                if ( pValue->NameLength > 0 )
                {
                    WCHAR *pName = ( WCHAR * ) MoveToOffset ( pValue, pValue->NameOffset );
                    ZeroMemory ( szAttributeName, sizeof ( szAttributeName ) );
                    memcpy ( szAttributeName, pName, pValue->NameLength * sizeof(WCHAR) );
                    ShowDetailLineW ( L':', L"A List Name", L"%s\n", szAttributeName );
                }
                ShowDetailLineW ( L'-', L"A List Low VCN", L"%15s\n", SeparatedNumberType ( pValue->LowVcn ) );
                ShowDetailLineW ( L'-', L"A List File Reference Number", L"%15s\n", SeparatedNumberType ( pValue->FileReferenceNumber ) );
                ShowDetailLineW ( L'-', L"A List Attribute Number", L"%15s\n", SeparatedNumberType ( pValue->AttributeNumber ) );
                break;
            }

            //
            case AttributeFileName :
            {
                PFILENAME_ATTRIBUTE pValue = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                WCHAR szTime [ 32 ];

                //
                LARGE_INTEGER dftn;
                dftn.QuadPart           = pValue->DirectoryFileReferenceNumber;

                //
                ShowDetailLineW ( L'-', L"FN Directory Number", L"%15s\n", SeparatedNumberType ( pValue->DirectoryFileReferenceNumber ) );
                ShowDetailLineW ( L'-', L"FN Directory High Part", L"%15s\n", SeparatedNumberType ( dftn.HighPart ) );
                ShowDetailLineW ( L'-', L"FN Directory Low Part", L"%15s\n", SeparatedNumberType ( dftn.LowPart ) );
                ShowDetailLineW ( L'-', L"FN Creation Time", L"%llu %s\n", pValue->CreationTime, ConvertDateTime (  pValue->CreationTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"FN Change Time", L"%llu %s\n", pValue->ChangeTime, ConvertDateTime (  pValue->ChangeTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"FN Last Write Time", L"%llu %s\n", pValue->LastWriteTime, ConvertDateTime (  pValue->LastWriteTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"FN Last Access Time", L"%llu %s\n", pValue->LastAccessTime, ConvertDateTime (  pValue->LastAccessTime, szTime, FALSE ) );
                ShowDetailLineW ( L'-', L"FN Allocated Size", L"%15s\n", SeparatedNumberType ( pValue->AllocatedSize ) );
                ShowDetailLineW ( L'-', L"FN Data Size", L"%15s\n", SeparatedNumberType ( pValue->DataSize ) );
                ShowDetailLineW ( L'-', L"FN File Attributes", L"0x%lx (%s)\n", pValue->FileAttributes,
                    GetFileAttribute ( pValue->FileAttributes, szFileAttribute, sizeof(szFileAttribute) ) );
                ShowDetailLineW ( L'-', L"FN Name Length", L"%15s\n", SeparatedNumberType ( pValue->NameLength ) );
                ShowDetailLineW ( L'-', L"FN Name Type", L"%u (%s)\n", pValue->NameType, FlagFilenameToString(pValue->NameType) );
#if 0
                ShowDetailLineW ( L'-', L"FN Name                          : %c\n", pValue->Name [ 0 ] );
#endif
                if ( ( pValue->NameType & 0x01 ) != 0 )
                {
                    GetAttributeFilename ( pValue, szLongFilename, sizeof ( szLongFilename ) );
                    ShowDetailLineW ( L'-', L"FN Name", L"%s\n", szLongFilename );
                }
                else
                {
                    GetAttributeFilename ( pValue, szShortFilename, sizeof ( szShortFilename ) );
                    ShowDetailLineW ( L'-', L"FN Name", L"%s\n", szShortFilename );
                }
                break;
            }

            //
            case AttributeObjectId :
            {
                POBJECTID_ATTRIBUTE pValue = ( POBJECTID_ATTRIBUTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                WCHAR szGUID [ MAX_PATH ];
                ZeroMemory ( szGUID, sizeof ( szGUID ) );
                StringFromGUID2 ( pValue->ObjectId, szGUID, sizeof ( szGUID ) / sizeof(WCHAR) );
                ShowDetailLineW ( L'-', L"OI ObjectId", L"%s\n", szGUID );
                StringFromGUID2 ( pValue->BirthObjectId, szGUID, sizeof ( szGUID ) / sizeof(WCHAR) );
                ShowDetailLineW ( L'-', L"OI BirthObjectId", L"%s\n", szGUID );

                break;
            }

            //
            case AttributeSecurityDescriptor :
            {
                ShowDetailLineW ( L'-', L"AttributeSecurityDescriptor", L"\n" );
                //  The data attribute contains whatever data the creator of the attribute chooses.
                if ( pAttribute->Nonresident )
                {
                    ShowDetailLineW ( L'+', L"AttributeSecurity Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                    ShowDetailLineW ( L'+', L"AttributeSecurity Allocated", L"%15\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );

                    DumpNonResi ( '-', "AttributeSecurity Dump", hVolume, lpVolumeLcnInfo, pNonResident, iDumpData );

                }
                else
                {
                    ShowDetailLineW ( L'-', L"AttributeSecurity Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                    ShowDetailLineW ( L'-', L"AttributeSecurity Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );

                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                    if ( iDumpData > 0 )
                    {
                           Dump ( '-', "AttributeSecurity Dump", pValue, pResident->ValueLength, FALSE, TRUE, TRUE );
                    }
                }
                break;
            }

            //
            case AttributeVolumeName :
            {
                //  The volume name attribute just contains the volume label as a Unicode string.
                WCHAR *pValue = ( WCHAR * ) MoveToOffset ( pResident, pResident->ValueOffset );
                WCHAR szVolumeName [ MAX_PATH ];
                ZeroMemory ( szVolumeName, sizeof ( szVolumeName ) );
                wcscpy_s ( szVolumeName, min ( sizeof ( szVolumeName ) / sizeof(WCHAR), pResident->ValueLength ), pValue );
                ShowDetailLineW ( L'-', L"AttributeVolumeName", L"%s\n", szVolumeName );
                break;
            }

            //
            case AttributeVolumeInformation :
            {
                PVOLUME_INFORMATION pValue = ( PVOLUME_INFORMATION ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"VI Major Version", L"%u\n", pValue->MajorVersion );
                ShowDetailLineW ( L'-', L"VI Minor Version", L"%u\n", pValue->MinorVersion );
                ShowDetailLineW ( L'-', L"VI Flags", L"%u\n", pValue->Flags );
                break;
            }

            //
            case AttributeData :
            {
                //
                DWORD dwTotalWritten = 0;

                //  The data attribute contains whatever data the creator of the attribute chooses.
                if ( pAttribute->Nonresident )
                {
                    ShowDetailLineW ( L'+', L"AttributeData Data Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                    ShowDetailLineW ( L'+', L"AttributeData Data Allocated", L"%15s\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );

                    DumpNonResi ( '+', "AttributeData Dump", hVolume, lpVolumeLcnInfo, pNonResident, iDumpData );
                }
                else
                {
                    ShowDetailLineW ( L'-', L"AttributeData Value Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                    ShowDetailLineW ( L'-', L"AttributeData Value Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );
                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                    if ( iDumpData > 0 )
                    {
                           Dump ( '-', "AttributeData Dump", pValue, pResident->ValueLength, FALSE, TRUE, TRUE );
                    }
                }

                break;
            }

            //
            case AttributeIndexRoot :
            {
                PINDEX_ROOT pValue = ( PINDEX_ROOT ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"Root Type", L"0x%x\n", pValue->Type );
                ShowDetailLineW ( L'-', L"Root Type", L"%s\n", GetAttributeName ( pValue->Type ) );
                ShowDetailLineW ( L'-', L"Root Collation Rule", L"%lu\n", pValue->CollationRule );
                ShowDetailLineW ( L'-', L"Root Bytes Per Index Block", L"%15s\n", SeparatedNumberType ( pValue->BytesPerIndexBlock ) );
                ShowDetailLineW ( L'-', L"Root Clusters Per Index Block", L"%15s\n", SeparatedNumberType ( pValue->ClustersPerIndexBlock ) );
                ShowDetailLineW ( L'-', L"Root Entries Offset", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.EntriesOffset ) );
                ShowDetailLineW ( L'-', L"Root Index Block Length", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.IndexBlockLength ) );
                ShowDetailLineW ( L'-', L"Root Allocated Size", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.AllocatedSize ) );
                ShowDetailLineW ( L'-', L"Root Flags Directory", L"0x%01lx (%s)\n", pValue->DirectoryIndex.Flags, FlagDirectoryToString(pValue->DirectoryIndex.Flags) );
                break;
            }

            //
            case AttributeIndexAllocation :
            {
                PINDEX_BLOCK_HEADER pValue = ( PINDEX_BLOCK_HEADER ) MoveToOffset ( pResident, pResident->ValueOffset );

                ShowDetailLineW ( L'-', L"IndexAllocation Type", L"0x%lx\n", pValue->Ntfs.Type.uType );
                char szType [ 5 ];
                ZeroMemory ( szType, sizeof ( szType ) );
                memcpy ( szType, pFileRecordHeader->RecHdr.Type.szType, 4 );
                ShowDetailLineA ( '-', "IndexAllocation Type", "%s\n", szType );
                ShowDetailLineW ( L'-', L"IndexAllocation Usa Count", L"%15s\n", SeparatedNumberType ( pValue->Ntfs.UsaCount ) );
                ShowDetailLineW ( L'-', L"IndexAllocation Usa Offset", L"%15s\n", SeparatedNumberType ( pValue->Ntfs.UsaOffset ) );
                ShowDetailLineW ( L'-', L"IndexAllocation USN", L"%15s\n", SeparatedNumberType ( pValue->Ntfs.Usn ) );
                ShowDetailLineW ( L'-', L"IndexAllocation Value Length", L"%15s\n", SeparatedNumberType ( pValue->IndexBlockVcn ) );
                ShowDetailLineW ( L'-', L"IndexAllocation Flag Directory", L"0x%02lx (%s)\n", pValue->DirectoryIndex.Flags, FlagDirectoryToString(pValue->DirectoryIndex.Flags) );
                ShowDetailLineW ( L'-', L"IndexAllocation Allocated Size", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.AllocatedSize ) );
                ShowDetailLineW ( L'-', L"IndexAllocation Entry Offse", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.EntriesOffset ) );
                ShowDetailLineW ( L'-', L"IndexAllocation Block Length", L"%15s\n", SeparatedNumberType ( pValue->DirectoryIndex.IndexBlockLength ) );

                break;
            }

            //
            case AttributeBitmap :
            {
                if ( pAttribute->Nonresident )
                {
                    ShowDetailLineW ( L'+', L"AttributeBitmap Data Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                    ShowDetailLineW ( L'+', L"AttributeBitmap AllocatedSize", L"%15s\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );

                    DumpNonResi ( '+', "AttributeBitmap", hVolume, lpVolumeLcnInfo, pNonResident, iDumpData );

                }
                else
                {
                    ShowDetailLineW ( L'-', L"AttributeBitmap Value Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                    ShowDetailLineW ( L'-', L"AttributeBitmap Value Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );

                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                    if ( iDumpData > 0 )
                    {
                           Dump ( '-', "AttributeBitmap Dump", pValue, pResident->ValueLength, FALSE, TRUE, TRUE );
                    }
                }
                break;
            }

            //
            case AttributeReparsePoint :
            {
                PREPARSE_POINT pValue = ( PREPARSE_POINT ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"AttributeReparsePoint Tag", L"%lu\n", pValue->ReparseTag );
                ShowDetailLineW ( L'-', L"AttributeReparsePoint DataLength", L"%15s\n", SeparatedNumberType ( pValue->ReparseDataLength ) );

                break;
            }

            //
            case AttributeEAInformation :
            {
                PEA_INFORMATION pValue = ( PEA_INFORMATION ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"AttributeEAInformation EaLength", L"%15s\n", SeparatedNumberType ( pValue->EaLength ) );
                ShowDetailLineW ( L'-', L"AttributeEAInformation Q Length", L"%15s\n", SeparatedNumberType ( pValue->EaQueryLength ) );
                break;
            }

            //
            case AttributeEA :
            {
                PEA_ATTRIBUTE pValue = ( PEA_ATTRIBUTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                ShowDetailLineW ( L'-', L"AttributeEA Flags ", L"0x%x\n", pValue->Flags );
                ShowDetailLineW ( L'-', L"AttributeEA Ea Name Length", L"%15s\n", SeparatedNumberType ( pValue->EaNameLength ) );
                ShowDetailLineW ( L'-', L"AttributeEA Ea Value Length", L"%15s\n", SeparatedNumberType ( pValue->EaValueLength ) );
                break;
            }

            //
            case AttributePropertySet :
            {
                if ( pAttribute->Nonresident )
                {
                    ShowDetailLineW ( L'+', L"AttributePropertySet Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                    ShowDetailLineW ( L'+', L"AttributePropertySet Allocated", L"%15s\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );

                    DumpNonResi ( '+', "AttributePropertySet Dump", hVolume, lpVolumeLcnInfo, pNonResident, iDumpData );
                }
                else
                {
                    ShowDetailLineW ( L'-', L"AttributePropertySet Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                    ShowDetailLineW ( L'-', L"AttributePropertySet Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );

                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                    if ( iDumpData > 0 )
                    {
                           Dump ( '-', "AttributePropertySet Dump", pValue, pResident->ValueLength, FALSE, TRUE, TRUE );
                    }
                }
                break;
            }

            //
            case AttributeLoggedUtilityStream :
            {
                if ( pAttribute->Nonresident )
                {
                    ShowDetailLineW ( L'+', L"AttributeLogged Size", L"%15s\n", SeparatedNumberType ( pNonResident->DataSize ) );
                    ShowDetailLineW ( L'+', L"AttributeLogged Allocated", L"%15s\n", SeparatedNumberType ( pNonResident->AllocatedSize ) );

                    DumpNonResi ( '+', "AttributeLogged Dump", hVolume, lpVolumeLcnInfo, pNonResident, iDumpData );
                }
                else
                {
                    ShowDetailLineW ( L'-', L"AttributeLogged Offset", L"%15s\n", SeparatedNumberType ( pResident->ValueOffset ) );
                    ShowDetailLineW ( L'-', L"AttributeLogged Length", L"%15s\n", SeparatedNumberType ( pResident->ValueLength ) );

                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                    if ( iDumpData > 0 )
                    {
                           Dump ( '-', "AttributeLogged Dump", pValue, pResident->ValueLength, FALSE, TRUE, TRUE );
                    }
                }
                break;
            }

            //
            default :
            {
                bContinue = FALSE;
                break;
            }
        }

        offset += pAttribute->Length;

        if ( pAttribute->Length == 0 )
        {
            bContinue = FALSE;
        }

    }

    PrintTraceW ( L"\n" );

    return TRUE;

}

//
//////////////////////////////////////////////////////////////////////////////
//  uDimpData = 0 : no Dump
//              1 : One Record
//              2 : All Records
//////////////////////////////////////////////////////////////////////////////
BOOL WriteFileData (    HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        PFILE_RECORD_HEADER pFileRecordHeader,
                        WCHAR *pOutputFile, ULONG iFileNumber )
{
    //
    //
    if ( ! isTypeValid ( pFileRecordHeader ) )
    {
        return FALSE;
    }

    if ( ( pFileRecordHeader->Flags & 0x02 ) != 0 )
    {
        PrintStderrW ( L"Error - Directories Are Not Copied\n" );
        return FALSE;
    }

    //
    ZeroMemory ( szLongFilename, sizeof ( szLongFilename ) );
    ZeroMemory ( szShortFilename, sizeof ( szShortFilename ) );

    //
    int offset = pFileRecordHeader->AttributeOffset;
    int maxOffset = pFileRecordHeader->BytesInUse - sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) - 1;
    BOOL bContinue = TRUE;
    while ( offset < maxOffset  && bContinue  )
    {
        char *pStartAttribute = ( char * ) MoveToOffset ( pFileRecordHeader, offset );
        PATTRIBUTE pAttribute = ( PATTRIBUTE ) pStartAttribute;

        //
        PNONRESIDENT_ATTRIBUTE pNonResident = ( PNONRESIDENT_ATTRIBUTE )  ( ( char * ) pAttribute );
        PRESIDENT_ATTRIBUTE pResident       = ( PRESIDENT_ATTRIBUTE )  ( ( char * ) pAttribute );

        //
        switch ( pAttribute->AttributeType )
        {
            //
            case AttributeFileName :
            {
                PFILENAME_ATTRIBUTE pValue = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResident, pResident->ValueOffset );
                if ( ( pValue->NameType & 0x01 ) != 0 )
                {
                    GetAttributeFilename ( pValue, szLongFilename, sizeof ( szLongFilename ) );
                }
                else
                {
                    GetAttributeFilename ( pValue, szShortFilename, sizeof ( szShortFilename ) );
                }
                break;
            }

            //
            case AttributeData :
            {
                //
                DWORD dwTotalWritten = 0;

                //
                HANDLE hFile = INVALID_HANDLE_VALUE;
                if ( pOutputFile != NULL )
                {
                    hFile = OpenOutputFile ( pOutputFile, iFileNumber );
                }

                //  The data attribute contains whatever data the creator of the attribute chooses.
                if ( pAttribute->Nonresident )
                {
                    //
                    if ( hFile != INVALID_HANDLE_VALUE )
                    {
                        ULONGLONG ullRemaining = pNonResident->DataSize;

                        //
                        BOOL bContinueFor = TRUE;
                        for ( ULONGLONG vcn = pNonResident->LowVcn; vcn < pNonResident->HighVcn + 1 && bContinueFor; vcn++ )
                        {
                            int sizeToRead = lpVolumeLcnInfo->SectorsPerCluster * lpVolumeLcnInfo->BytesPerSector;
                            PBYTE pBuffer = ( PBYTE ) malloc ( sizeToRead );
                            BOOL bResult = ReadExternalAttribute (  hVolume, lpVolumeLcnInfo, pNonResident, vcn, 1, pBuffer );
                            if ( bResult )
                            {
                                if ( sizeToRead > ullRemaining )
                                {
                                    sizeToRead = ( int ) ullRemaining;
                                }
                                DWORD dwWritten;
                                BOOL bWrite = WriteFile ( hFile, pBuffer, sizeToRead, &dwWritten, NULL );
                                if ( ! bWrite )
                                {
                                    PrintStderrW ( L"Error - WriteFile Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
                                    bContinueFor = FALSE;
                                }
                                
                                //
                                dwTotalWritten += dwWritten;

                                if ( ullRemaining > ( ULONGLONG) sizeToRead )
                                {
                                    ullRemaining = ullRemaining - sizeToRead;
                                }
                                else
                                {
                                    ullRemaining = 0;
                                    bContinueFor = FALSE;
                                }
                            }
                            else
                            {
                                bContinueFor = FALSE;
                            }

                            free ( pBuffer );
                        }
                    }
                }
                else
                {
                    PBYTE pValue = ( PBYTE ) MoveToOffset ( pResident, pResident->ValueOffset );

                    if ( hFile != INVALID_HANDLE_VALUE )
                    {
                        DWORD dwWritten;
                        BOOL bWrite = WriteFile ( hFile, pValue, pResident->ValueLength, &dwWritten, NULL );
                        if ( ! bWrite )
                        {
                            PrintStderrW ( L"Error - WriteFile Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
                        }
                        //
                        dwTotalWritten += dwWritten;

                    }
                }

                //
                if ( hFile != INVALID_HANDLE_VALUE )
                {
                    PrintNormalW ( L"%s - Created (%ld Bytes)\n", szOutputFilename, dwTotalWritten );

                    CloseHandle ( hFile );
                }

                break;
            }
        }

        offset += pAttribute->Length;

        if ( pAttribute->Length == 0 )
        {
            bContinue = FALSE;
        }

    }

    return TRUE;

}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ShowDetailRecord ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        PNTFS_FILE_RECORD_OUTPUT_BUFFER ntfsFileRecordOutput,
                        ULONG iFileNumber, DUMP_DATA iDumpData  )
{
    //
    PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &ntfsFileRecordOutput->FileRecordBuffer[0] );

    ShowDetailLineW ( L':', L"File Reference Number", L"%15s\n", SeparatedNumberType ( ntfsFileRecordOutput->FileReferenceNumber.QuadPart ) );
    ShowDetailLineW ( L':', L"File Record Length", L"%15s\n", SeparatedNumberType ( ntfsFileRecordOutput->FileRecordLength ) );

    //
    ShowDetailRecord ( hVolume, lpVolumeLcnInfo, pFileRecordHeader, iFileNumber, iDumpData );

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL WriteFileData (    HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                        PNTFS_FILE_RECORD_OUTPUT_BUFFER ntfsFileRecordOutput,
                        WCHAR *pOutputFile, ULONG iFileNumber )
{
    //
    PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &ntfsFileRecordOutput->FileRecordBuffer[0] );

    //
    WriteFileData ( hVolume, lpVolumeLcnInfo, pFileRecordHeader, pOutputFile, iFileNumber );

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ListAllAttributes ( PNTFS_FILE_RECORD_OUTPUT_BUFFER pNtfsFileRecordOutput )
{
    //
    BOOL    bResult = TRUE;

    //
    PATTRIBUTE pAttribute = NULL;
    int index = 0;
    ShowDetailLineW ( L':', L"List Attributes", L"\n" );
    do
    {
        pAttribute = GetAttributeByIndex ( pNtfsFileRecordOutput, index );
        if ( pAttribute == NULL )
        {
            break;
        }

        ShowDetailLineW ( L':', L"Attribute", L"%s - %s\n",
            GetAttributeName ( pAttribute->AttributeType ), FlagNonResidentToString ( pAttribute->Nonresident ) );

        index++;
    }
    while ( pAttribute != NULL );

    PrintTraceW ( L"\n" );

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetNTFSFileRecord (    HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                            ULONG iFileNumber, SHOW_DATA bShow,
                            PNTFS_FILE_RECORD_OUTPUT_BUFFER pNtfsFileRecordOutput,
                            WCHAR *pOutputFile,
                            DUMP_DATA iDumpData )
{
    // Variables for MFT-reading
    NTFS_FILE_RECORD_INPUT_BUFFER   ntfsFileRecordInput;

    //
    PNTFS_FILE_RECORD_OUTPUT_BUFFER pLocalNtfsFileRecordOutput = NULL;
    if ( pNtfsFileRecordOutput == NULL )
    {
        pLocalNtfsFileRecordOutput = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)
            malloc ( sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );
        pNtfsFileRecordOutput = pLocalNtfsFileRecordOutput;
    }

    //  0 = $MFT
    //  1 = $MFTMirr
    //  2 = $LogFile
    //  3 = $Volume
    //  4 = $AttrDef
    //  5 = . Index Root
    //  6 = $Bitmap
    //  7 = $Boot
    //  8 = $BadClus
    //  9 = $Secure
    //  10 = $UpCase
    //  11 = $Extend
    //  12 = No Name
    ntfsFileRecordInput.FileReferenceNumber.QuadPart = iFileNumber;

    //
    DWORD lpBytesReturned = 0;

    //
    //  A voir aussi \\.\E:\\$MFT

    //
    BOOL bContinue = true;

    //
    BOOL bResult = DeviceIoControl (
        hVolume,
        FSCTL_GET_NTFS_FILE_RECORD,
        &ntfsFileRecordInput,
        sizeof ( NTFS_FILE_RECORD_INPUT_BUFFER ),
        pNtfsFileRecordOutput,
        sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1,
        &lpBytesReturned, NULL );
    if ( bResult )
    {
        if ( pNtfsFileRecordOutput->FileReferenceNumber.QuadPart == iFileNumber )
        {

            //
            if ( bShow )
            {
                ShowDetailRecord ( hVolume, lpVolumeLcnInfo, pNtfsFileRecordOutput, iFileNumber, iDumpData );

                ListAllAttributes ( pNtfsFileRecordOutput );
            }

            if ( pOutputFile != NULL )
            {
                bResult = WriteFileData ( hVolume, lpVolumeLcnInfo, pNtfsFileRecordOutput, pOutputFile, iFileNumber );
            }

        }
        else
        {
            bResult = FALSE;
        }

    }
    //  Error After last file generally
    else
    {
        PrintStderrW ( L"Error - DeviceIoControl Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
    }

    //
    if ( pLocalNtfsFileRecordOutput != NULL )
    {
        free(pLocalNtfsFileRecordOutput);
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  https://www.codeproject.com/Articles/9293/Undelete-a-file-in-NTFS
//  https://stackoverflow.com/questions/54606760/how-to-read-the-master-file-table-mft-using-c
//////////////////////////////////////////////////////////////////////////////
BOOL GetNTFSVolumeData ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo )
{
    //
    NTFS_VOLUME_DATA_BUFFER ntfsVolData;
    ZeroMemory ( &ntfsVolData, sizeof(NTFS_VOLUME_DATA_BUFFER) );

    //

    DWORD dwWritten = 0;
    BOOL bResult = DeviceIoControl (
        hVolume,
        FSCTL_GET_NTFS_VOLUME_DATA,
        NULL,
        0,
        &ntfsVolData,
        sizeof(ntfsVolData),
        &dwWritten,
        NULL );
    if ( ! bResult )
    {
        PrintStderrW ( L"Error - DeviceIoControl Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
        return FALSE;
    }

    PrintTraceW ( L"\n" );

    ShowDetailLineW ( L':', L"Volume Serial Number", L"0x%08llx\n", ntfsVolData.VolumeSerialNumber.QuadPart );
    ShowDetailLineW ( L':', L"Number Sectors", L"%15s\n", SeparatedNumberType ( ntfsVolData.NumberSectors.QuadPart ) );
    ShowDetailLineW ( L':', L"Total Clusters", L"%15s\n", SeparatedNumberType ( ntfsVolData.TotalClusters.QuadPart ) );
    ShowDetailLineW ( L':', L"Free Clusters", L"%15s\n", SeparatedNumberType ( ntfsVolData.FreeClusters.QuadPart ) );
    ShowDetailLineW ( L':', L"Total Reserved", L"%15s\n", SeparatedNumberType ( ntfsVolData.TotalReserved.QuadPart ) );
    ShowDetailLineW ( L':', L"Bytes Per Sector", L"%15s\n", SeparatedNumberType ( ntfsVolData.BytesPerSector ) );
    ShowDetailLineW ( L':', L"Bytes Per Cluster", L"%15s\n", SeparatedNumberType ( ntfsVolData.BytesPerCluster ) );
    ShowDetailLineW ( L':', L"Bytes Per File Record Segment", L"%15s\n", SeparatedNumberType ( ntfsVolData.BytesPerFileRecordSegment ) );
    ShowDetailLineW ( L':', L"Clusters Per File Record Segment", L"%15s\n", SeparatedNumberType ( ntfsVolData.ClustersPerFileRecordSegment) );
    ShowDetailLineW ( L':', L"Mft Valid Data Length", L"%15s\n", SeparatedNumberType ( ntfsVolData.MftValidDataLength.QuadPart ) );
    ShowDetailLineW ( L':', L"Mft Start Lcn", L"%15s\n", SeparatedNumberType ( ntfsVolData.MftStartLcn.QuadPart ) );
    ShowDetailLineW ( L':', L"Mft2 Start Lcn", L"%15s\n", SeparatedNumberType ( ntfsVolData.Mft2StartLcn.QuadPart ) );
    ShowDetailLineW ( L':', L"Mft Zone Start", L"%15s\n", SeparatedNumberType ( ntfsVolData.MftZoneStart.QuadPart ) );
    ShowDetailLineW ( L':', L"Mft Zone End", L"%15s\n", SeparatedNumberType ( ntfsVolData.MftZoneEnd.QuadPart ) );

    //
    //  Set Byte Per File Record Segment
    lpVolumeLcnInfo->lBytesPerFileRecord = ntfsVolData.BytesPerFileRecordSegment;

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetVolumeDiskExtents ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo )
{
    VOLUME_DISK_EXTENTS   volumeExtents;
    DWORD                 retbytes = 0;

    //
    ZeroMemory ( &volumeExtents, sizeof(VOLUME_DISK_EXTENTS) );

    BOOL bResult = DeviceIoControl ( hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        NULL,
        NULL,
        &volumeExtents,
        sizeof ( VOLUME_DISK_EXTENTS ),
        &retbytes,
        NULL);
    if ( ! bResult )
    {
        PrintStderrW ( L"Error - DeviceIoControl %ld Fails With 0x%lx - %s\n", lpVolumeLcnInfo->DiskNumber, GetLastError(), GetLastErrorText() );
        return FALSE;
    }

    lpVolumeLcnInfo->DiskNumber     = volumeExtents.Extents[0].DiskNumber;
    lpVolumeLcnInfo->llVolumeOffset = volumeExtents.Extents[0].StartingOffset.QuadPart;

    WCHAR szOffset [ LEN_NUMBER_STRING ];
    HumanReadable ( szOffset, sizeof ( szOffset ) / sizeof(WCHAR), lpVolumeLcnInfo->llVolumeOffset );

    PrintTraceW ( L"\n" );
    ShowDetailLineW ( L':', L"Volume Disk Number - Offset", L"%ld  %12lld (%s)\n",
        lpVolumeLcnInfo->DiskNumber, lpVolumeLcnInfo->llVolumeOffset, szOffset );

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetBootBlock ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo, PBOOT_BLOCK pBootBlock )
{
    //
    //  Read Boot Block
    //
    DWORD               retbytes = 0;

    //
    //  Position At Start
    SetFilePointer ( hVolume, 0, 0, FILE_BEGIN );
    ZeroMemory ( pBootBlock, sizeof(BOOT_BLOCK) );

    //
    BOOL bResult = ReadFile ( hVolume,
        pBootBlock,
        sizeof(BOOT_BLOCK),
        &retbytes,
        NULL);
    if ( ! bResult )
    {
        PrintStderrW ( L"Error - ReadFile Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText() );
        PrintStderrW ( L"Info  - Sizeof BOOT_BLOCK %ld\n", sizeof(BOOT_BLOCK) );
        return FALSE;
    }
    else
    {
        PrintTraceW ( L"\n" );
        ShowDetailLineW ( L':', L"Boot : Sectors Per Cluster", L"%15s\n", SeparatedNumberType ( pBootBlock->SectorsPerCluster ) );
        ShowDetailLineW ( L':', L"Boot : Bytes Per Sector", L"%15s\n", SeparatedNumberType ( pBootBlock->BytesPerSector ) );
        ShowDetailLineW ( L':', L"Boot : Clusters Per File Record", L"%15s\n", SeparatedNumberType ( pBootBlock->ClustersPerFileRecord ) );
        if ( pBootBlock->ClustersPerFileRecord > 0x80 )
        {
            ShowDetailLineW ( L':', L"Boot : Bytes    Per File Record", L"2 ** %lu\n", 0x100 - pBootBlock->ClustersPerFileRecord );
        }
        ShowDetailLineW ( L':', L"Boot : Clusters Per Index Block", L"%15s\n", SeparatedNumberType ( pBootBlock->ClustersPerIndexBlock ) );

        //
        //  This value is retrieved by FSCTL_GET_NTFS_VOLUME_DATA
        LONG BytesPerFileRecord = 0;
        if ( pBootBlock->ClustersPerFileRecord < 0x80 )
        {
            BytesPerFileRecord = pBootBlock->ClustersPerFileRecord * pBootBlock->SectorsPerCluster * pBootBlock->BytesPerSector;
        }
        else
        {
            BytesPerFileRecord = 1 << (0x100 - pBootBlock->ClustersPerFileRecord );
        }
        ShowDetailLineW ( L':', L"Boot : Bytes Per File Record", L"%15s\n", SeparatedNumberType ( BytesPerFileRecord ) );

        //
        lpVolumeLcnInfo->SectorsPerCluster      = pBootBlock->SectorsPerCluster;
        lpVolumeLcnInfo->BytesPerSector         = pBootBlock->BytesPerSector;
        lpVolumeLcnInfo->ClustersPerFileRecord      = pBootBlock->ClustersPerFileRecord;
        lpVolumeLcnInfo->ClustersPerIndexBlock      = pBootBlock->ClustersPerIndexBlock;
        lpVolumeLcnInfo->lBytesPerFileRecord        = BytesPerFileRecord;
    }

    return TRUE;
}

//
//////////////////////////////////////////////////////////////////////////////
//  get volume sectors per clusters, bytes per sectors
//  get volume disk index ,volume offset on disk;
//  char *drv = "\\\\.\\E:";
//////////////////////////////////////////////////////////////////////////////
BOOL GetVolumeDiskInfoMFT ( HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                            GET_VOLUME_DATA bGetVolumeData, SHOW_DATA bShow, DUMP_DATA iDumpData )
{
    //
    //  Read Boot Block
    //
    BOOT_BLOCK          bootBlock;

    //
    BOOL bResult = GetBootBlock ( hVolume, lpVolumeLcnInfo, &bootBlock );
    if ( ! bResult )
    {
        return bResult;
    }

    //  get volume disk index ,volume offset on disk;
    bResult = GetVolumeDiskExtents ( hVolume, lpVolumeLcnInfo );
    if ( ! bResult )
    {
        return bResult;
    }

    if ( ! bGetVolumeData )
    {
        return bResult;
    }

    bResult = GetNTFSVolumeData ( hVolume, lpVolumeLcnInfo );
    if ( ! bResult )
    {
        return bResult;
    }

    //
    //  Allocate NTFS_FILE_RECORD_OUTPUT_BUFFER
    if ( lpVolumeLcnInfo->pMFTRecordOututBuffer == NULL )
    {
        lpVolumeLcnInfo->pMFTRecordOututBuffer = ( PNTFS_FILE_RECORD_OUTPUT_BUFFER )
            malloc ( sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );
        lpVolumeLcnInfo->pMFTRecordHeader = ( PFILE_RECORD_HEADER ) ( &lpVolumeLcnInfo->pMFTRecordOututBuffer->FileRecordBuffer[0] );
    }

    //
    //  Read MFT$ : Do Not Trace DATA
    bResult = GetNTFSFileRecord ( hVolume, lpVolumeLcnInfo,  0, bShow, lpVolumeLcnInfo->pMFTRecordOututBuffer, NULL, iDumpData );
    if ( bResult )
    {
        //
        PATTRIBUTE pAttribute = GetAttributeByType ( lpVolumeLcnInfo->pMFTRecordOututBuffer, AttributeData, 0 );
        if ( pAttribute != NULL )
        {
            ULONGLONG bytesLength = GetAttributeLength ( pAttribute );
            ULONGLONG recordCount = bytesLength / lpVolumeLcnInfo->lBytesPerFileRecord;
            lpVolumeLcnInfo->llFileRecordCount = recordCount;
            ShowDetailLineW ( L':', L"MFT Record Count", L"%15s\n", SeparatedNumberType ( recordCount ) );
            PrintTraceW ( L"\n" );
        }
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL GetVolumeDiskInfoEntry (   HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                                PNTFS_FILE_RECORD_OUTPUT_BUFFER pEntryRecord,
                                GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber, SHOW_DATA bShow,
                                WCHAR *pOutputFile,
                                DUMP_DATA iDumpData )
{
    //
    PNTFS_FILE_RECORD_OUTPUT_BUFFER pLocalEntryRecord = NULL;
    if ( pEntryRecord == NULL )
    {
        pLocalEntryRecord = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)
            malloc ( sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );
        pEntryRecord = pLocalEntryRecord;
    }

    //
    //  Get File Record : Trace Data
    BOOL bResult = GetNTFSFileRecord ( hVolume, lpVolumeLcnInfo, iFileNumber, bShow, pEntryRecord, pOutputFile, iDumpData );
    if ( bResult )
    {
        //
        PFILE_RECORD_HEADER  pFileRecordHeader;
        pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &pEntryRecord->FileRecordBuffer [ 0 ] );
        pFileRecordHeader->Flags; // In Use Flag
        PATTRIBUTE pAttribute = GetAttributeByType ( pEntryRecord, AttributeStandardInformation, 0 );
    }

    //
    if ( pLocalEntryRecord != NULL )
    {
        free ( pLocalEntryRecord );
    }

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get Information On The Disk : Called from wsrm
//  Main Read The MFT Table
//////////////////////////////////////////////////////////////////////////////
BOOL GetVolumeDiskInfo ( WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo )
{
    //
    WCHAR szDrive [ LEN_DRIVE_STRING ];

    //
    if ( NULL == pDrive )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    if (    wcsncmp ( pDrive, PREFIX_DSK, wcslen ( PREFIX_DSK ) ) != 0  &&
            wcsncmp ( pDrive, PREFIX_VOL, wcslen ( PREFIX_VOL ) ) != 0      )
    {
        wnsprintf ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), L"\\\\.\\%c:", pDrive [ 0 ] );
    }
    else
    {
        wcscpy_s ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), pDrive );
    }

    //
    //  Copy Drive String
    wcscpy_s ( lpVolumeLcnInfo->szDrive, sizeof ( lpVolumeLcnInfo->szDrive ) / sizeof(WCHAR), szDrive );

    //
    HANDLE hVolume = OpenLogicalDisk ( szDrive );
    if ( INVALID_HANDLE_VALUE == hVolume )
    {
        return FALSE;
    }

    //
    //  Get $MFT : Do Not Trace Data
    BOOL bResult = GetVolumeDiskInfoMFT ( hVolume, lpVolumeLcnInfo, GET_VOLUME_DATA_NO, SHOW_DATA_NO, DUMP_DATA_NONE );

    //
    CloseHandle ( hVolume );

    //
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get File Information for a File Number : Called from wsrm
//////////////////////////////////////////////////////////////////////////////
BOOL GetFileInformation (   WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                            GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber )
{
    //
    WCHAR szDrive [ LEN_DRIVE_STRING ];

    //
    if ( NULL == pDrive )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    if (    wcsncmp ( pDrive, PREFIX_DSK, wcslen ( PREFIX_DSK ) ) != 0  &&
            wcsncmp ( pDrive, PREFIX_VOL, wcslen ( PREFIX_VOL ) ) != 0      )
    {
        wnsprintf ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), L"\\\\.\\%c:", pDrive [ 0 ] );
    }
    else
    {
        wcscpy_s ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), pDrive );
    }

    //
    //  Copy Drive String
    wcscpy_s ( lpVolumeLcnInfo->szDrive, sizeof ( lpVolumeLcnInfo->szDrive ) / sizeof(WCHAR), szDrive );

    //
    HANDLE hVolume = OpenLogicalDisk ( szDrive );
    if ( INVALID_HANDLE_VALUE == hVolume )
    {
        return FALSE;
    }

    //
    //  Get $MFT : Do Not Trace Data
    SHOW_DATA bShow = SHOW_DATA_NO;

    //
    //  If MFT only show record
    if (  iFileNumber == 0 )
    {
        bShow = SHOW_DATA_DO;
    }
    BOOL bResult = GetVolumeDiskInfoMFT ( hVolume, lpVolumeLcnInfo, bGetVolumeData, bShow, DUMP_DATA_NONE );
    if ( bResult && iFileNumber != 0 )
    {
        //
        //  The Get Entry : Look if it"s deleted
        bResult = GetVolumeDiskInfoEntry ( hVolume, lpVolumeLcnInfo, NULL, bGetVolumeData, iFileNumber, SHOW_DATA_DO, NULL, DUMP_DATA_SHORT );
        if ( ! bResult )
        {
            //
            PFILE_RECORD_HEADER pFileRecordHeader = (PFILE_RECORD_HEADER) malloc ( lpVolumeLcnInfo->lBytesPerFileRecord );

            bResult = ReadFileRecord ( hVolume, lpVolumeLcnInfo, iFileNumber, pFileRecordHeader );
            if ( bResult )
            {
                LARGE_INTEGER FileReferenceNumber;
                FileReferenceNumber.QuadPart = iFileNumber;
                ShowDetailRecord ( hVolume, lpVolumeLcnInfo, pFileRecordHeader, iFileNumber, DUMP_DATA_SHORT );
            }

            //
            free ( pFileRecordHeader );
        }
    }

    //
    CloseHandle ( hVolume );

    //
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get File Information for a File Number And Recurse
//////////////////////////////////////////////////////////////////////////////
BOOL LocateFile (   HANDLE hVolume, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                    GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber )
{
    //
    //  Get $MFT : Do Not Trace Data
    BOOL bResult = TRUE;

    //
    if ( isAbortedByCtrlC ( ) )
    {
        return FALSE;
    }

    //
    PNTFS_FILE_RECORD_OUTPUT_BUFFER pEntryRecord = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)
        malloc ( sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );
    ZeroMemory ( pEntryRecord, sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );

    //
    //  The Get Entry : Look if it"s deleted
    bResult = GetVolumeDiskInfoEntry ( hVolume, lpVolumeLcnInfo, pEntryRecord, bGetVolumeData, iFileNumber, SHOW_DATA_NO, NULL, DUMP_DATA_SHORT );
    if ( bResult && pEntryRecord->FileReferenceNumber.QuadPart == iFileNumber )
    {
        PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &pEntryRecord->FileRecordBuffer [ 0 ] );

        ZeroMemory ( szLongFilename, sizeof ( szLongFilename ) );
        ZeroMemory ( szShortFilename, sizeof ( szShortFilename ) );

        //  Init
        PRESIDENT_ATTRIBUTE pResidentAttribute      = NULL;
        PFILENAME_ATTRIBUTE pFilenameAttribute      = NULL;

        //  Search Second Filename if any
        PATTRIBUTE pAttribute = GetAttributeByType ( pFileRecordHeader, AttributeFileName, 1 );
        if ( pAttribute != NULL )
        {
            pResidentAttribute                          = ( PRESIDENT_ATTRIBUTE ) pAttribute;
            pFilenameAttribute                          = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttribute,  pResidentAttribute->ValueOffset );

            if ( pFilenameAttribute != NULL )
            {
                if ( ( pFilenameAttribute->NameType & 0x01 ) != 0 )
                {
                    GetAttributeFilename ( pFilenameAttribute, szLongFilename, sizeof ( szLongFilename ) );
                }

                if ( ( pFilenameAttribute->NameType & 0x02 ) != 0 )
                {
                    GetAttributeFilename ( pFilenameAttribute, szShortFilename, sizeof ( szShortFilename ) );
                }
            }

        }

        //  Search First Filename if any
        if ( wcslen ( szLongFilename ) == 0 )
        {
            pAttribute = GetAttributeByType ( pFileRecordHeader, AttributeFileName, 0 );

            if ( pAttribute != NULL )
            {
                pResidentAttribute                          = ( PRESIDENT_ATTRIBUTE ) pAttribute;

                pFilenameAttribute                          = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttribute,  pResidentAttribute->ValueOffset );

                if ( pFilenameAttribute != NULL )
                {
                    if ( ( pFilenameAttribute->NameType & 0x01 ) != 0 )
                    {
                        GetAttributeFilename ( pFilenameAttribute, szLongFilename, sizeof ( szLongFilename ) );
                    }

                    if ( ( pFilenameAttribute->NameType & 0x02 ) != 0 )
                    {
                        GetAttributeFilename ( pFilenameAttribute, szShortFilename, sizeof ( szShortFilename ) );
                    }
                }
            }
        }

        //
        if ( pAttribute != NULL )
        {
            //
            if ( wcslen ( szLongFilename ) > 0 )
            {
                wcscpy_s ( szFilename, sizeof ( szFilename ) / sizeof(WCHAR), szLongFilename );
            }
            else
            {
                wcscpy_s ( szFilename, sizeof ( szFilename ) / sizeof(WCHAR), szShortFilename );
            }

            WCHAR *pFilename = ( WCHAR * ) malloc ( ( wcslen (szFilename) + 1 ) * 2 );
            wcscpy_s ( pFilename, ( wcslen (szFilename) + 1 ) * 2 , szFilename );
            LARGE_INTEGER largeInteger;
            largeInteger.QuadPart =  pFilenameAttribute->DirectoryFileReferenceNumber;

            //  The Root has a parent as itself
            if ( iFileNumber != largeInteger.LowPart )
            {
                bResult = LocateFile ( hVolume, lpVolumeLcnInfo, bGetVolumeData, largeInteger.LowPart );
                PrintNormalW ( L"\\%s", pFilename );
            }
            else
            {
                PrintNormalW ( L"\\%s", pFilename );
            }

            //
            free ( pFilename );
        }

    }
    else
    {
        //
        PFILE_RECORD_HEADER pFileRecordHeader = (PFILE_RECORD_HEADER) malloc( lpVolumeLcnInfo->lBytesPerFileRecord );

        bResult = ReadFileRecord ( hVolume, lpVolumeLcnInfo, iFileNumber, pFileRecordHeader );
        if ( bResult )
        {
            //
            //  Search Second Filename if any
            PATTRIBUTE pAttribute = GetAttributeByType ( pFileRecordHeader, AttributeFileName, 1 );
            if ( pAttribute == NULL )
            {
                //  Search First Filename if any
                pAttribute = GetAttributeByType ( pFileRecordHeader, AttributeFileName, 0 );
            }

            if ( pAttribute != NULL )
            {
                PRESIDENT_ATTRIBUTE pResidentAttribute      = NULL;
                pResidentAttribute                      = ( PRESIDENT_ATTRIBUTE ) pAttribute;

                PFILENAME_ATTRIBUTE pFilenameAttribute      = NULL;
                pFilenameAttribute                          = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttribute,  pResidentAttribute->ValueOffset );

                if ( pFilenameAttribute != NULL )
                {
                    GetAttributeFilename ( pFilenameAttribute, szFilename, sizeof ( szFilename ) );
                    WCHAR *pFilename = ( WCHAR * ) malloc ( ( wcslen (szFilename) + 1 ) * 2 );
                    wcscpy_s ( pFilename, ( wcslen (szFilename) + 1 ) * 2 , szFilename );
                    LARGE_INTEGER largeInteger;
                    largeInteger.QuadPart =  pFilenameAttribute->DirectoryFileReferenceNumber;

                    //  The Root has a parent as itself
                    if ( iFileNumber != largeInteger.LowPart )
                    {
                        bResult = LocateFile ( hVolume, lpVolumeLcnInfo, bGetVolumeData, largeInteger.LowPart );
                        PrintNormalW ( L"\\%s", pFilename );
                    }
                    else
                    {
                        PrintNormalW ( L"\\%s", pFilename );
                    }

                    //
                    free ( pFilename );
                }
            }

        }
        free ( pFileRecordHeader );
    }

    free ( pEntryRecord );

    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get File Information for a File Number : Called from wsrm
//////////////////////////////////////////////////////////////////////////////
BOOL LocateFile (   WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                    GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber )
{
    //
    WCHAR szDrive [ LEN_DRIVE_STRING ];

    //
    if ( NULL == pDrive )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    if (    wcsncmp ( pDrive, PREFIX_DSK, wcslen ( PREFIX_DSK ) ) != 0  &&
            wcsncmp ( pDrive, PREFIX_VOL, wcslen ( PREFIX_VOL ) ) != 0      )
    {
        wnsprintf ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), L"\\\\.\\%c:", pDrive [ 0 ] );
    }
    else
    {
        wcscpy_s ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), pDrive );
    }

    //
    //  Copy Drive String
    wcscpy_s ( lpVolumeLcnInfo->szDrive, sizeof ( lpVolumeLcnInfo->szDrive ) / sizeof(WCHAR), szDrive );

    //
    HANDLE hVolume = OpenLogicalDisk ( szDrive );
    if ( INVALID_HANDLE_VALUE == hVolume )
    {
        return FALSE;
    }

    //
    //  Read MFT
    BOOL bResult = GetVolumeDiskInfoMFT ( hVolume, lpVolumeLcnInfo, bGetVolumeData, SHOW_DATA_NO, DUMP_DATA_NONE );

    //
    if ( bResult )
    {
        wprintf ( L"%s", pDrive );
        bResult = LocateFile ( hVolume, lpVolumeLcnInfo, bGetVolumeData, iFileNumber );
        PrintNormalW ( L"\n" );
    }

    //
    CloseHandle ( hVolume );

    //
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get File Information for a File Number
//////////////////////////////////////////////////////////////////////////////
BOOL CopyFileInformation (  WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                            GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber,
                            WCHAR *outputFile )
{
    //
    WCHAR szDrive [ LEN_DRIVE_STRING ];

    //
    if ( NULL == pDrive )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    if (    wcsncmp ( pDrive, PREFIX_DSK, wcslen ( PREFIX_DSK ) ) != 0  &&
            wcsncmp ( pDrive, PREFIX_VOL, wcslen ( PREFIX_VOL ) ) != 0      )
    {
        wnsprintf ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), L"\\\\.\\%c:", pDrive [ 0 ] );
    }
    else
    {
        wcscpy_s ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), pDrive );
    }

    //
    //  Copy Drive String
    wcscpy_s ( lpVolumeLcnInfo->szDrive, sizeof ( lpVolumeLcnInfo->szDrive ) / sizeof(WCHAR), szDrive );

    //
    HANDLE hVolume = OpenLogicalDisk ( szDrive );
    if ( INVALID_HANDLE_VALUE == hVolume )
    {
        return FALSE;
    }

    //
    //  Get $MFT : Do Not Trace Data
    BOOL bResult = GetVolumeDiskInfoMFT ( hVolume, lpVolumeLcnInfo, bGetVolumeData, SHOW_DATA_NO, DUMP_DATA_NONE );
    if ( bResult && iFileNumber != 0 )
    {
        //
        //  The Get Entry : Look if it"s deleted
        bResult = GetVolumeDiskInfoEntry ( hVolume, lpVolumeLcnInfo, NULL, bGetVolumeData, iFileNumber, SHOW_DATA_NO, outputFile, DUMP_DATA_NONE );
        if ( ! bResult )
        {
            //
            PFILE_RECORD_HEADER pFileRecordHeader = (PFILE_RECORD_HEADER) malloc( lpVolumeLcnInfo->lBytesPerFileRecord );

            bResult = ReadFileRecord ( hVolume, lpVolumeLcnInfo, iFileNumber, pFileRecordHeader );
            if ( bResult )
            {
                LARGE_INTEGER FileReferenceNumber;
                FileReferenceNumber.QuadPart = iFileNumber;
                if ( outputFile != NULL )
                {
                    bResult = WriteFileData ( hVolume, lpVolumeLcnInfo, pFileRecordHeader, outputFile, iFileNumber );
                }
            }
            free ( pFileRecordHeader );
        }
    }

    //
    CloseHandle ( hVolume );

    //
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  List Files
//////////////////////////////////////////////////////////////////////////////
BOOL ShowFile (     LARGE_INTEGER FileReferenceNumber,
                    PFILE_RECORD_HEADER  pFileRecordHeader,
                    BOOL bDirectoryOnly,
                    BOOL bDeleted,
                    WCHAR *pFilter, WCHAR *pNoMatch )
{
    if ( ! isTypeValid ( pFileRecordHeader ) )
    {
        char szType [ 5 ];
        ZeroMemory ( szType, sizeof ( szType ) );

        return FALSE;
    }

    //
    ZeroMemory ( szLine, sizeof(szLine) );

    //
    BOOL    isDirectory = FALSE;
    if ( ( pFileRecordHeader->Flags & 0x02 ) != 0 )
    {
        isDirectory = TRUE;
    }

    //
    //
    PATTRIBUTE pAttributeSi     = ( PATTRIBUTE ) GetAttributeByType ( pFileRecordHeader, AttributeStandardInformation, 0 );
    PATTRIBUTE pAttributeFn0    = ( PATTRIBUTE ) GetAttributeByType ( pFileRecordHeader, AttributeFileName, 0 );
    PATTRIBUTE pAttributeData   = ( PATTRIBUTE ) GetAttributeByType ( pFileRecordHeader, AttributeData, 0 );

    //  Date
    WCHAR szTime [ 32 ] = L"                ";

    //
    GetFileAttribute ( 0, szFileAttribute, sizeof(szFileAttribute) );
    if ( pAttributeSi != NULL )
    {
        //
        PRESIDENT_ATTRIBUTE pResidentAttributeSi    = NULL;
        pResidentAttributeSi                        = ( PRESIDENT_ATTRIBUTE ) pAttributeSi;

        //
        PSTANDARD_INFORMATION pResidentSi   = (PSTANDARD_INFORMATION) MoveToOffset ( pResidentAttributeSi, pResidentAttributeSi->ValueOffset );
        GetFileAttribute ( pResidentSi->FileAttributes, szFileAttribute, sizeof(szFileAttribute) );

        //
        ConvertDateTime ( pResidentSi->ChangeTime, szTime, TRUE );

    }

    //
    if ( pAttributeFn0 != NULL )
    {
        PRESIDENT_ATTRIBUTE pResidentAttributeFn0   = NULL;
        pResidentAttributeFn0                       = ( PRESIDENT_ATTRIBUTE ) pAttributeFn0;

        PFILENAME_ATTRIBUTE pFilenameAttribute      = NULL;
        pFilenameAttribute                          = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttributeFn0,  pResidentAttributeFn0->ValueOffset );

        if ( pFilenameAttribute != NULL )
        {
            ConvertDateTime ( pFilenameAttribute->ChangeTime, szTime, TRUE );
        }
    }

    //
    wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR), L"%s ", szTime );
    wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );

    if ( pAttributeData != NULL )
    {
        PRESIDENT_ATTRIBUTE pResidentAttributeDataRS        = NULL;
        pResidentAttributeDataRS                            = ( PRESIDENT_ATTRIBUTE ) pAttributeData;

        PNONRESIDENT_ATTRIBUTE pResidentAttributeDataNR     = NULL;
        pResidentAttributeDataNR                            = ( PNONRESIDENT_ATTRIBUTE ) pAttributeData;


        //  File Size
        if ( pAttributeData->Nonresident )
        {
            wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR), L"%9llu/%9llu ",
                            pResidentAttributeDataNR->DataSize, pResidentAttributeDataNR->AllocatedSize );
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );
        }
        else
        {
            wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR), L"%9lu/%9lu ",
                            pResidentAttributeDataRS->ValueLength, pResidentAttributeDataRS->ValueLength );
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );
        }
    }
    else
    {
        wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR), L"%9s %9s ", L"", L"" );
        wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );
    }

    if ( pFileRecordHeader->Flags & 0x02 )
    {
        wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L"<DIR> " );
    }
    else
    {
        wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L"<FIL> " );
    }

    wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR), L"%s ", szFileAttribute );
    wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );

    wnsprintf ( szFormatted, sizeof ( szFormatted ) / sizeof(WCHAR),L"%08ld ", FileReferenceNumber );
    wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szFormatted );

    //
    //  Long Filename
    ZeroMemory ( szFilename, sizeof(szFilename) );
    ZeroMemory ( szLongFilename, sizeof(szLongFilename) );
    ZeroMemory ( szShortFilename, sizeof(szShortFilename) );

    //
    if ( pAttributeFn0 != NULL )
    {
        PRESIDENT_ATTRIBUTE pResidentAttributeFn0   = NULL;
        pResidentAttributeFn0                       = ( PRESIDENT_ATTRIBUTE ) pAttributeFn0;

        PFILENAME_ATTRIBUTE pFilenameAttribute      = NULL;
        pFilenameAttribute                          = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttributeFn0,  pResidentAttributeFn0->ValueOffset );

        //  Long Filename
        if ( pFilenameAttribute->NameType & 0x01 )
        {
            GetAttributeFilename ( pFilenameAttribute, szLongFilename, sizeof(szLongFilename) );
            wcscpy_s ( szFilename, sizeof ( szFilename ) / sizeof(WCHAR), szLongFilename );
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szLongFilename );
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L" " );
        }
        //  Short Filename
        else
        {
            //  Get Another Filename Attribute
            //  Since the First Attribute was not a long filename this is now the long filename
            PATTRIBUTE pAttributeFn1 = ( PATTRIBUTE ) GetAttributeByType ( pFileRecordHeader, AttributeFileName, 1 );
            if ( pAttributeFn1 != NULL )
            {
                PRESIDENT_ATTRIBUTE pResidentAttributeFn1   = ( PRESIDENT_ATTRIBUTE ) pAttributeFn1;
                pFilenameAttribute  = ( PFILENAME_ATTRIBUTE ) MoveToOffset ( pResidentAttributeFn1,  pResidentAttributeFn1->ValueOffset );

                GetAttributeFilename ( pFilenameAttribute, szLongFilename, sizeof(szLongFilename) );
                wcscpy_s ( szFilename, sizeof ( szFilename ) / sizeof(WCHAR), szLongFilename );
                wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szLongFilename );
                wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L" " );
            }
            else
            {
                GetAttributeFilename ( pFilenameAttribute, szShortFilename, sizeof(szShortFilename) );
                wcscpy_s ( szFilename, sizeof ( szFilename ) / sizeof(WCHAR), szShortFilename );
                wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), szShortFilename );
                wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L" " );
            }
        }
    }

    if ( bDeleted )
    {
        if ( *szFilename == '\0' )
        {
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L"<NOT IN USE>" );
        }
        else
        {
            wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L"<DELETED>" );
        }
    }

    //
    wcscat_s ( szLine, sizeof ( szLine ) / sizeof(WCHAR), L"\n" );

    //
    BOOL bShowIt = TRUE;

    //
    if ( pFilter != NULL && wcslen ( pFilter) > 0 )
    {
        if ( wcslen ( szFilename ) > 0 && ! PathMatchSpec ( szFilename, pFilter ) )
        {
            bShowIt = FALSE;
        }
    }

    //
    if ( pNoMatch != NULL && wcslen ( pNoMatch) > 0 )
    {
        if (  wcslen ( szFilename ) > 0 && PathMatchSpec ( szFilename, pNoMatch ) )
        {
            bShowIt = FALSE;
        }
    }

    //
    if ( bShowIt )
    {
        if ( ! bDirectoryOnly || isDirectory )
        {
            PrintNormalW ( L"%s", szLine );
        }
    }

    return TRUE;
}


//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
BOOL ShowFile (     PNTFS_FILE_RECORD_OUTPUT_BUFFER pEntryRecord,
                    PFILE_RECORD_HEADER  pFileRecordHeader,
                    BOOL bDirectoryOnly,
                    BOOL bDeleted,
                    WCHAR *pFilter, WCHAR *pNoMatch )
{
    return ShowFile ( pEntryRecord->FileReferenceNumber, pFileRecordHeader, bDirectoryOnly, bDeleted, pFilter, pNoMatch );
}

//
//////////////////////////////////////////////////////////////////////////////
//  List Files
//////////////////////////////////////////////////////////////////////////////
BOOL ListFiles (    WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo,
                    BOOL bDirectoryOnly,
                    BOOL bDeletedOnly,
                    GET_VOLUME_DATA bGetVolumeData,
                    WCHAR *pFilter, WCHAR *pNoMatch )
{
    //
    WCHAR szDrive [ LEN_DRIVE_STRING ];

    //
    if ( NULL == pDrive )
    {
        return FALSE;
    }

    //
    ZeroMemory ( szDrive, sizeof ( szDrive ) );
    if (    wcsncmp ( pDrive, PREFIX_DSK, wcslen ( PREFIX_DSK ) ) != 0  &&
            wcsncmp ( pDrive, PREFIX_VOL, wcslen ( PREFIX_VOL ) ) != 0      )
    {
        wnsprintf ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), L"\\\\.\\%c:", pDrive [ 0 ] );
    }
    else
    {
        wcscpy_s ( szDrive, sizeof ( szDrive ) / sizeof(WCHAR), pDrive );
    }

    //
    //  Copy Drive String
    wcscpy_s ( lpVolumeLcnInfo->szDrive, sizeof ( lpVolumeLcnInfo->szDrive ) / sizeof(WCHAR), szDrive );

    //
    HANDLE hVolume = OpenLogicalDisk ( szDrive );
    if ( INVALID_HANDLE_VALUE == hVolume )
    {
        return FALSE;
    }

    //
    //  Get $MFT
    BOOL bResult = GetVolumeDiskInfoMFT ( hVolume, lpVolumeLcnInfo, bGetVolumeData, SHOW_DATA_NO, DUMP_DATA_NONE );

    //
    if ( bResult )
    {
        //
        PNTFS_FILE_RECORD_OUTPUT_BUFFER pEntryRecord = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)
            malloc ( sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );

        //
        //  Then Get All Entries Entries
        for ( ULONG iFileNumber = 0; iFileNumber < ( ULONG ) lpVolumeLcnInfo->llFileRecordCount; iFileNumber++ )
        {
            if ( isAbortedByCtrlC() )
            {
                break;
            }

            ZeroMemory ( pEntryRecord, sizeof ( NTFS_FILE_RECORD_OUTPUT_BUFFER ) + lpVolumeLcnInfo->lBytesPerFileRecord - 1 );
            bResult = GetVolumeDiskInfoEntry ( hVolume, lpVolumeLcnInfo, pEntryRecord, bGetVolumeData, iFileNumber, SHOW_DATA_NO, NULL, DUMP_DATA_NONE );

            //
            //  We Have a record
            if ( bResult && pEntryRecord->FileReferenceNumber.QuadPart == iFileNumber )
            {
                PFILE_RECORD_HEADER  pFileRecordHeader = ( PFILE_RECORD_HEADER ) ( &pEntryRecord->FileRecordBuffer [ 0 ] );
                if ( ! bDeletedOnly )
                {
                    //  The FALSE mean that the file is In Use ( Not Deleted)
                    BOOL bDeleted = FALSE;
                    if ( ( pFileRecordHeader->Flags & 0x01 ) == 0 )
                    {
                        bDeleted = TRUE;
                    }
                    ShowFile ( pEntryRecord, pFileRecordHeader, bDirectoryOnly, bDeleted, pFilter, pNoMatch );
                }
            }

            //  Here we are on a delete file
            else /* if iFileNumber > 32 ) */
            {
                //
                PFILE_RECORD_HEADER pFileRecordHeader = (PFILE_RECORD_HEADER) malloc( lpVolumeLcnInfo->lBytesPerFileRecord );

                bResult = ReadFileRecord ( hVolume, lpVolumeLcnInfo, iFileNumber, pFileRecordHeader );
                if ( ! bResult )
                {
                    break;
                }
                LARGE_INTEGER FileReferenceNumber;
                FileReferenceNumber.QuadPart = iFileNumber;
                BOOL bDeleted = TRUE;   // The file has not been seen : so it is deleted
                ShowFile ( FileReferenceNumber, pFileRecordHeader, bDirectoryOnly, bDeleted, pFilter, pNoMatch );

                free ( pFileRecordHeader );
            }

        }   // For

        //
        free ( pEntryRecord );

    }

    //
    CloseHandle ( hVolume );

    //
    return bResult;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Read Data from File
//////////////////////////////////////////////////////////////////////////////
BOOL ReadDataFromFile ( PVOLUME_DISK_INFO lpVolumeLcnInfo, PFILE_LCN lpFileLcn, BOOL bPhysical )
{
    WCHAR szDiskName[MAX_PATH] ;
    ZeroMemory ( szDiskName, sizeof ( szDiskName ) );

    if ( bPhysical )
    {
        wsprintf ( szDiskName, L"\\\\.\\PhysicalDrive%ld", lpVolumeLcnInfo->DiskNumber );
    }
    else
    {
        wcscpy_s ( szDiskName, sizeof ( szDiskName ) / sizeof(WCHAR), lpVolumeLcnInfo->szDrive );
    }

    //
    HANDLE hDisk = NULL;
    if ( bPhysical )
    {
        hDisk = OpenPhysicalDisk ( szDiskName );
    }
    else
    {
        hDisk = OpenLogicalDisk ( szDiskName );
    }

    if ( INVALID_HANDLE_VALUE == hDisk )
    {
        return FALSE;
    }

    char buffer[512] ;

    DWORD  retbytes = 0;

    //
    //  Read Fully The File
    for(int i = 0 ; i < lpFileLcn->lLcnCount ;++i)
    {
        ZeroMemory ( buffer, sizeof(buffer) );

        //
        LARGE_INTEGER llOffset;
        llOffset.QuadPart = lpFileLcn->lcnOffsetAndLength [ i ].llLcnOffset;
        if ( ! bPhysical )
        {
            llOffset.QuadPart -= lpVolumeLcnInfo->llVolumeOffset;
        }

        //
        BOOL bResult = SetFilePointerEx ( hDisk, llOffset, NULL, FILE_BEGIN );
        if ( ! bResult )
        {
            PrintStderrW ( L"Error - SetFilePointerEx Fails With 0x%lx - %s\n", GetLastError(), GetLastErrorText() );
            break;
        }

        //
        bResult = ReadFile ( hDisk, buffer, sizeof(buffer), &retbytes, NULL );
        if ( ! bResult )
        {
            PrintStderrW ( L"Error - ReadFile Fails With 0x%lx %s\n", GetLastError(), GetLastErrorText() );
            break;
        }
        else
        {
    
        }

    }

    //
    CloseHandle(hDisk);

    return TRUE;
}
