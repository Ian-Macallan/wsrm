#include "stdafx.h"

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include "srm.h"
#include "DiskFileInfos.h"

#include "PrintRoutine.h"

static ULONG BytesPerFileRecord;
static HANDLE hVolume;
static BOOT_BLOCK bootBlock;
static PFILE_RECORD_HEADER MFT;

template <class T1, class T2> inline
T1* Padd(T1* p, T2 n) { return (T1*)((char *)p + n); }

//
//////////////////////////////////////////////////////////////////////////////
//  RUN Length
//////////////////////////////////////////////////////////////////////////////
static ULONG XERunLength(PUCHAR run)
{
    return ( *run & 0xf ) + ( ( *run >> 4 ) & 0xf ) + 1;
}

//
//////////////////////////////////////////////////////////////////////////////
//  Get LCN from RUN
//////////////////////////////////////////////////////////////////////////////
static LONGLONG XERunLCN(PUCHAR run)
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
ULONGLONG XERunCount(PUCHAR run)
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
BOOL XEFindRun(PNONRESIDENT_ATTRIBUTE pNonResident, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count)
{
    if ( vcn < pNonResident->LowVcn || vcn > pNonResident->HighVcn )
    {
        return FALSE;
    }

    *lcn = 0;
    ULONGLONG base = pNonResident->LowVcn;
    for (   PUCHAR run = PUCHAR(Padd(pNonResident, pNonResident->RunArrayOffset));
            *run != 0;
            run += XERunLength(run) ) {
        *lcn    += XERunLCN(run);
        *count  = XERunCount(run);
        if ( base <= vcn && vcn < base + *count ) {
            *lcn    = XERunLCN(run) == 0 ? 0 : *lcn + vcn - base;
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
PATTRIBUTE XEFindAttribute(PFILE_RECORD_HEADER fileRecordHeader, ATTRIBUTE_TYPE type, PWSTR name, int index)
{
    int count = 0;
    for (   PATTRIBUTE pAttribute = PATTRIBUTE(Padd(fileRecordHeader, fileRecordHeader->AttributeOffset));
            pAttribute->AttributeType != -1;
            pAttribute = Padd(pAttribute, pAttribute->Length)) {

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
                    _wcsicmp(name, PWSTR(Padd(pAttribute, pAttribute->NameOffset))) == 0 )
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
VOID XEFixupUpdateSequenceArray(PFILE_RECORD_HEADER fileRecordHeader)
{
    PUSHORT usa = PUSHORT(Padd(fileRecordHeader, fileRecordHeader->RecHdr.UsaOffset));
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
VOID XEReadSector ( ULONGLONG sector, ULONG count, PVOID buffer )
{
    ULARGE_INTEGER offset;
    OVERLAPPED overlap = {0};
    ULONG n;
    offset.QuadPart = sector * bootBlock.BytesPerSector;
    overlap.Offset = offset.LowPart;
    overlap.OffsetHigh = offset.HighPart;

    ReadFile ( hVolume, buffer, count * bootBlock.BytesPerSector, &n, &overlap );
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEReadLCN(ULONGLONG lcn, ULONG count, PVOID buffer)
{
    XEReadSector( lcn * bootBlock.SectorsPerCluster, count * bootBlock.SectorsPerCluster, buffer );
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEReadExternalAttribute(PNONRESIDENT_ATTRIBUTE pNonResident, ULONGLONG vcn, ULONG count, PVOID buffer)
{
    ULONGLONG lcn, XERunCount;
    ULONG readcount, left;
    PUCHAR bytes = PUCHAR(buffer);
    for ( left = count; left > 0; left -= readcount ) {
        XEFindRun( pNonResident, vcn, &lcn, &XERunCount );
        readcount = ULONG(min(XERunCount, left));
        ULONG n = readcount * bootBlock.BytesPerSector * bootBlock.SectorsPerCluster;
        if (lcn == 0)
            memset ( bytes, 0, n );
        else
            XEReadLCN(lcn, readcount, bytes);
        vcn += readcount;
        bytes += n;
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
ULONG XEAttributeLength(PATTRIBUTE pNonResident)
{
    return pNonResident->Nonresident == FALSE
        ? PRESIDENT_ATTRIBUTE(pNonResident)->ValueLength
        : ULONG(PNONRESIDENT_ATTRIBUTE(pNonResident)->DataSize);
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
ULONG XEAttributeLengthAllocated(PATTRIBUTE pNonResident)
{
    return pNonResident->Nonresident == FALSE
        ? PRESIDENT_ATTRIBUTE(pNonResident)->ValueLength
        : ULONG(PNONRESIDENT_ATTRIBUTE(pNonResident)->AllocatedSize);
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEReadAttribute(PATTRIBUTE pNonResident, PVOID buffer)
{
    //  Resident
    if (pNonResident->Nonresident == FALSE) {
        PRESIDENT_ATTRIBUTE rpNonResident = PRESIDENT_ATTRIBUTE(pNonResident);
        memcpy(buffer, Padd(rpNonResident, rpNonResident->ValueOffset), rpNonResident->ValueLength);
    }
    //  Non Resident
    else {
        PNONRESIDENT_ATTRIBUTE npNonResident = PNONRESIDENT_ATTRIBUTE(pNonResident);
        XEReadExternalAttribute(npNonResident, 0, ULONG(npNonResident->HighVcn) + 1, buffer);
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEReadVCN ( PFILE_RECORD_HEADER fileRecordHeader, ATTRIBUTE_TYPE type, ULONGLONG vcn, ULONG count, PVOID buffer )
{
    PNONRESIDENT_ATTRIBUTE pNonResident = PNONRESIDENT_ATTRIBUTE(XEFindAttribute(fileRecordHeader, type, 0, 0));
    if (pNonResident == 0 || (vcn < pNonResident->LowVcn || vcn > pNonResident->HighVcn)) {
        // Support for huge fileRecordHeaders
        PATTRIBUTE pNonResidentlist = XEFindAttribute(fileRecordHeader, AttributeAttributeList, 0, 0);
        DebugBreak();
    }
    XEReadExternalAttribute(pNonResident, vcn, count, buffer);
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEReadFileRecord ( ULONG index, PFILE_RECORD_HEADER fileRecordHeader )
{
    ULONG clusters = bootBlock.ClustersPerFileRecord;
    if (clusters > 0x80) clusters = 1;
    PUCHAR p = new UCHAR[bootBlock.BytesPerSector * bootBlock.SectorsPerCluster * clusters];
    ULONGLONG vcn = ULONGLONG(index) * BytesPerFileRecord / bootBlock.BytesPerSector / bootBlock.SectorsPerCluster;
    XEReadVCN(MFT, AttributeData, vcn, clusters, p);
    LONG m = (bootBlock.SectorsPerCluster * bootBlock.BytesPerSector / BytesPerFileRecord) - 1;
    ULONG n = m > 0 ? (index & m) : 0;
    memcpy(fileRecordHeader, p + n * BytesPerFileRecord, BytesPerFileRecord);
    delete [] p;
    XEFixupUpdateSequenceArray(fileRecordHeader);
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XELoadMFT()
{
    BytesPerFileRecord = bootBlock.ClustersPerFileRecord < 0x80
        ? bootBlock.ClustersPerFileRecord
            * bootBlock.SectorsPerCluster
            * bootBlock.BytesPerSector
            : 1 << (0x100 - bootBlock.ClustersPerFileRecord);

    MFT = PFILE_RECORD_HEADER(new UCHAR[BytesPerFileRecord]);

    //  Read MFT from Disk
    XEReadSector(bootBlock.MftStartLcn * bootBlock.SectorsPerCluster, BytesPerFileRecord / bootBlock.BytesPerSector, MFT);
    XEFixupUpdateSequenceArray(MFT);
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
static BOOL XEBitset(PUCHAR bitmap, ULONG i)
{
    return (bitmap[i >> 3] & (1 << (i & 7))) != 0;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
VOID XEFindDeleted()
{
    PATTRIBUTE pAttribute = XEFindAttribute(MFT, AttributeBitmap, 0, 0);
    PUCHAR bitmap = new UCHAR[XEAttributeLengthAllocated(pAttribute)];
    XEReadAttribute(pAttribute, bitmap);
    ULONG n = XEAttributeLength(XEFindAttribute(MFT, AttributeData, 0, 0)) / BytesPerFileRecord;
    PFILE_RECORD_HEADER fileRecordHeader = PFILE_RECORD_HEADER(new UCHAR[BytesPerFileRecord]);
    for (ULONG i = 0; i < n && ! isAbortedByCtrlC(); i++) {
        if (XEBitset(bitmap, i)) continue;
        XEReadFileRecord(i, fileRecordHeader);

        //  Look for Files or Directory not in use
        if ( fileRecordHeader->RecHdr.Type.uType == 'ELIF' && (fileRecordHeader->Flags & 1) == 0 ) {

            //
            pAttribute = XEFindAttribute(fileRecordHeader, AttributeFileName, 0, 0);
            if (pAttribute == 0) continue;

            //
            PFILENAME_ATTRIBUTE pFilenameAttribute = PFILENAME_ATTRIBUTE(Padd(pAttribute, PRESIDENT_ATTRIBUTE(pAttribute)->ValueOffset));

            WCHAR szTime [ 32 ];
            ConvertDateTime ( pFilenameAttribute->ChangeTime, szTime, TRUE );

            PATTRIBUTE pAttribute2 = XEFindAttribute(fileRecordHeader, AttributeStandardInformation, 0, 0);
            WCHAR szAttributes [ 16 ] = L"               ";
            if ( pAttribute2 != NULL )
            {
                PSTANDARD_INFORMATION pSI = PSTANDARD_INFORMATION(Padd(pAttribute2, PRESIDENT_ATTRIBUTE(pAttribute2)->ValueOffset));

                ConvertDateTime ( pSI->ChangeTime, szTime, TRUE );

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_READONLY )
                {
                    szAttributes [ 0 ] = L'R';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_HIDDEN )
                {
                    szAttributes [ 1 ] = L'H';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_SYSTEM )
                {
                    szAttributes [ 2 ] = L'S';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_ARCHIVE )
                {
                    szAttributes [ 3 ] = L'A';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_NORMAL )
                {
                    szAttributes [ 4 ] = L'N';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_COMPRESSED )
                {
                    szAttributes [ 5 ] = L'C';
                }

                if ( pSI->FileAttributes & FILE_ATTRIBUTE_ENCRYPTED )
                {
                    szAttributes [ 6 ] = L'E';
                }
            }

            szAttributes [ 7 ] = L'\0';

            //
            PrintNormalW ( L"%s ", szTime );

            PATTRIBUTE pAttributeData = XEFindAttribute(fileRecordHeader, AttributeData, 0, 0);
            if ( pAttributeData != NULL )
            {
                if ( pAttributeData->Nonresident )
                {
                    PNONRESIDENT_ATTRIBUTE pNonResident = ( PNONRESIDENT_ATTRIBUTE )  ( ( char * ) pAttributeData );
                    //  File Size
                    PrintNormalW ( L"%12lld/%12lld ", pNonResident->DataSize, pNonResident->AllocatedSize );
                }
                else
                {
                    PRESIDENT_ATTRIBUTE pResident       = ( PRESIDENT_ATTRIBUTE )  ( ( char * ) pAttributeData );
                    PrintNormalW ( L"%12ld/%12ld ", pResident->ValueLength, pResident->ValueLength );
                }
            }
            else
            {
                PrintNormalW ( L"%12s %12s ", L"", L"" );
            }

            if ( fileRecordHeader->Flags & 0x02 )
            {
                PrintNormalW ( L"<DIR> " );
            }
            else
            {
                PrintNormalW ( L"<FIL> " );
            }

            PrintNormalW ( L"%s ", szAttributes );

            //  If Long Filename print it
            if ( pFilenameAttribute->NameType & 0x01 )
            {
                PrintNormalA("%8lu %.*ws", i, int(pFilenameAttribute->NameLength), pFilenameAttribute->Name);
            }
            else
            {
                //  Find a second name
                pAttribute = XEFindAttribute(fileRecordHeader, AttributeFileName, 0, 1);
                if (pAttribute != 0 ) {
                    pFilenameAttribute = PFILENAME_ATTRIBUTE(Padd(pAttribute, PRESIDENT_ATTRIBUTE(pAttribute)->ValueOffset));
                    PrintNormalA("%8lu - %.*ws", i, int(pFilenameAttribute->NameLength), pFilenameAttribute->Name);
                }
                //  Not Found : print the first
                else
                {
                    PrintNormalA("%8lu %.*ws", i, int(pFilenameAttribute->NameLength), pFilenameAttribute->Name);               }
                }

            PrintNormalA("\n" );
        }
    }
}

//
//////////////////////////////////////////////////////////////////////////////
//  Read a index
//  Get Attribute for this index
//  Dump it on a disk file
//////////////////////////////////////////////////////////////////////////////
VOID XEDumpData(ULONG index, WCHAR *filename)
{
    PFILE_RECORD_HEADER fileRecordHeader = PFILE_RECORD_HEADER(new UCHAR[BytesPerFileRecord]);
    ULONG n;
    XEReadFileRecord ( index, fileRecordHeader );

    //  Not a File
    if ( fileRecordHeader->RecHdr.Type.uType != 'ELIF' ) return;

    //  Search Data
    PATTRIBUTE pAttribute = XEFindAttribute(fileRecordHeader, AttributeData, 0, 0);
    if (pAttribute == 0) return;

    //
    PUCHAR buf = new UCHAR[XEAttributeLengthAllocated(pAttribute)];
    XEReadAttribute(pAttribute, buf);
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    WriteFile(hFile, buf, XEAttributeLength(pAttribute), &n, 0);
    CloseHandle(hFile);
    delete [] buf;
}

//
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
int XEMain ( WCHAR cDrive, BOOL bFindDelete, BOOL bDump, int index, WCHAR *filename )
{
    WCHAR drive [] = L"\\\\.\\E:";
    ULONG n;
    drive[4] = cDrive;
    hVolume = CreateFileW(drive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if ( hVolume != INVALID_HANDLE_VALUE )
    {
        ReadFile ( hVolume, &bootBlock, sizeof bootBlock, &n, 0 );
        XELoadMFT();
        if ( bFindDelete ) XEFindDeleted();
        if ( bDump ) XEDumpData ( index, filename );
        CloseHandle(hVolume);
    }
    else
    {
        PrintStderrW ( L"XEMain : Error %s\n", GetLastErrorText () );
    }
    return 0;
}
