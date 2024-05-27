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

#include "PrintRoutine.h"
#include "strstr.h"

//
#define	BLOCK_SIZE				512
#define	MEGA_BYTE				(1024*1024)

#define	DISPLAY_EVERY			60

static int iFileNumber			= 0;
static char szCurrentDisk [ MAX_PATH ];
static FILE	*hFile				= NULL;
static char rootPathName  [ MAX_PATH ];

extern bool isAbortedByCtrlC();

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
static void FormatDate ( long iTime, char *pText, size_t lenText )
{
	char sign = ' ';
	if ( iTime < 0 )
	{
		sign = '-';
		iTime = -1 * iTime;
	}

	long seconds	= iTime % 60;
	iTime = iTime / 60;
	long minutes	= iTime % 60;
	iTime = iTime / 60;
	long hours		= iTime % 60;
	sprintf_s ( pText, lenText, "%c%02d:%02d:%02d", sign, hours, minutes, seconds );
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
static void RemoveCRLF ( char *pText )
{
	if ( pText != NULL && strlen ( pText ) > 0 )
	{
		int i = ( int) strlen ( pText ) - 1;
		while ( i >= 0 )
		{
			if ( pText [ i ] == '\r' || pText [ i ] == '\n' )
			{
				pText [ i ] = 0;
			}
			else
			{
				return;
			}

			i--;
		}
	}
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
static char *GetFilename ( const char *pDisk, int iFileNumber )
{
	char *pFilename				= (char *) malloc ( MAX_PATH );
	char	szText [ BLOCK_SIZE ]	= "";
	strcpy_s ( pFilename, MAX_PATH, pDisk );
	strcat_s ( pFilename, MAX_PATH, "fillwdsk_" );
	memset ( szText, 0, sizeof(szText) );
	sprintf_s ( szText, sizeof(szText), "%d", iFileNumber );
	strcat_s ( pFilename, MAX_PATH, szText );
	strcat_s ( pFilename, MAX_PATH, ".tmp" );

	return pFilename;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
static char *GetSummaryFilename ( const char *pDisk )
{
	char *pFilename				= (char *) malloc ( MAX_PATH );
	strcpy_s ( pFilename, MAX_PATH, pDisk );
	strcat_s ( pFilename, MAX_PATH, "fillwdsk.txt" );

	return pFilename;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
static bool Clean ( const char *pDisk, int iFileNumber )
{
	for ( int iX = 0; iX <= iFileNumber; iX++ )
	{
		char *pFilename = GetFilename ( pDisk, iX );
		PrintDirectA ( "Deleting '%s'\n", pFilename );
		_unlink ( pFilename );
	}

	return true;
}

//
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
bool FillOneDisk ( const char *pDisk )
{
	strcpy_s ( szCurrentDisk, sizeof (szCurrentDisk), pDisk );

	DWORD	dwSectorPerCluster		= NULL;
	DWORD	dwBytePerSector			= NULL;
	DWORD	dwNumberOfFreeCluster	= NULL;
	DWORD	dwTotalNumberOfCluster	= NULL;

	strcpy_s ( rootPathName, sizeof(rootPathName), pDisk );
#if 0
	rootPathName [ 1 ] = 0;
	strcat_s ( rootPathName, sizeof(rootPathName), ":\\" );
#endif

	if ( strlen ( rootPathName ) > 1 )
	{
		if ( rootPathName [ strlen ( rootPathName ) - 1 ] != '\\' )
		{
			strcat_s ( rootPathName, sizeof(rootPathName), "\\" );
		}
	}

	BOOL bResult = GetDiskFreeSpaceA ( 
		rootPathName, 
		&dwSectorPerCluster, &dwBytePerSector, &dwNumberOfFreeCluster, &dwTotalNumberOfCluster );
	DWORD	dwClusterSize				= ( dwSectorPerCluster * dwBytePerSector );
	if ( ! bResult )
	{
		//	In case of problem get disk C: cluster size
		BOOL bResult = GetDiskFreeSpaceA ( "C:\\", &dwSectorPerCluster, &dwBytePerSector, &dwNumberOfFreeCluster, &dwTotalNumberOfCluster );
		dwClusterSize				= ( dwSectorPerCluster * dwBytePerSector );

		//
		ULARGE_INTEGER FreeBytesAvailableToCaller;
		ULARGE_INTEGER TotalNumberOfBytes;
		ULARGE_INTEGER TotalNumberOfFreeBytes;
		bResult = GetDiskFreeSpaceExA ( rootPathName, &FreeBytesAvailableToCaller, &TotalNumberOfBytes, &TotalNumberOfFreeBytes );
		dwNumberOfFreeCluster = (DWORD) ( TotalNumberOfFreeBytes.QuadPart / ( long long ) dwClusterSize );
		dwTotalNumberOfCluster = (DWORD) ( TotalNumberOfBytes.QuadPart / ( long long ) dwClusterSize );
	}

	//
	long long toBeWritten = (long long) dwSectorPerCluster * (long long) dwBytePerSector * (long long) dwNumberOfFreeCluster;

	bool bContinue						= true;
	size_t	iBufferLengthMax			= ( ( MEGA_BYTE * 128 ) / dwClusterSize ) * dwClusterSize;
	iFileNumber						= 0;

	//
	long long elapsedEvery = 0;

	//
	long long elapsedFromStart = 0;

	long long bytesWriten = 0;
	long long realWriten = 0;

	long clusterDone = 0;

	//
	size_t	iCount		= 0;

	char	*pBuffer				= NULL;
	size_t	iBufferLength			= iBufferLengthMax;

	pBuffer = ( char * ) malloc ( iBufferLength );	
	if ( pBuffer == NULL )
	{
		Clean ( rootPathName, iFileNumber );
		return false;
	}

	//
	//	Fill Buffer
	unsigned int seed = (unsigned int) time(NULL);
	srand ( seed );
	memset ( pBuffer, 0, iBufferLength );
	short * pShortBuffer = (short *) pBuffer;
	for ( size_t iX = 0; iX < iBufferLength; iX++ )
	{
		pBuffer [ iX ] = rand() & 0xff;
	}

	for ( size_t iX = 0; iX < iBufferLength / sizeof(short); iX++ )
	{
		pShortBuffer [ iX ] = rand();
	} 

	//
	time_t startTime = time(NULL);
	time_t startTimeEvery = time(NULL);

	//
	do
	{
		iBufferLength					= iBufferLengthMax;
		size_t	iWrite					= NULL;
		char *	pFilename				= GetFilename( rootPathName, iFileNumber );

		hFile = NULL;
		if ( CheckPathExistA ( pFilename )  )
		{
			iFileNumber++;
			continue;
		}

		//
		iCount		= 0;
		OpenFileCcsA ( &hFile, pFilename, "wb" );
		if ( hFile )
		{
			do
			{
				iWrite = fwrite ( pBuffer, 1, iBufferLength, hFile );
				if ( ferror ( hFile ) )
				{
					iBufferLength /= 2;
				}
				else
				{
					iCount++;
					bytesWriten += iBufferLength;
					realWriten += iWrite;
					clusterDone += ( long ) ( iBufferLength / dwClusterSize );
				}

				//
				time_t currentTime = time(NULL);
				elapsedEvery =  currentTime - startTimeEvery;
				elapsedFromStart = currentTime - startTime;

				//
				if ( elapsedEvery > DISPLAY_EVERY )
				{
					long donePerSecond = clusterDone / ( long ) elapsedFromStart;
					if ( donePerSecond > 0 )
					{
						char szEstimated [ 32 ] = "\0";
						char szRemaining [ 32 ] = "\0";
						long estimated = ( long ) dwNumberOfFreeCluster / donePerSecond;
						long remaining = ( long ) ( dwNumberOfFreeCluster - clusterDone ) / donePerSecond;
						FormatDate ( estimated, szEstimated, sizeof ( szEstimated ) );
						FormatDate ( remaining, szRemaining, sizeof ( szRemaining ) );
						struct tm tmTime;
						localtime_s( &tmTime, &currentTime );
						PrintDirectA ( "%02d:%02d:%02d - Estimated Total Time %s (%ld seconds) - Remaining %s\n", 
							tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec, 
							szEstimated, estimated, szRemaining );
						startTimeEvery = currentTime;
					}

				}

			}
			while ( iBufferLength > BLOCK_SIZE && ! isAbortedByCtrlC() );

			fclose ( hFile );
		}
		else
		{
			PrintStderrA ( "Error opening filename '%s'\n", pFilename );
			perror ( "fopen_s" );
		}

		if ( iCount == 0 )
		{
			bContinue = false;
		}

		//
		//		Increment Filenumber
		iFileNumber++;

	}
	while ( bContinue && ! isAbortedByCtrlC() );

	//
	//		Free Memory
	free ( pBuffer );

	//
	//	Clean Files
//	if ( ! isAborted )
	{
		Clean ( rootPathName, iFileNumber );
	}

	time_t endTime = time(NULL);
	elapsedFromStart = endTime - startTime;

	//
	//	Write Summary
	if ( ! isAbortedByCtrlC() )
	{
		char *pFilename = GetSummaryFilename ( rootPathName );
		OpenFileCcsA ( &hFile, pFilename, "w" );
		if ( hFile )
		{
			time_t currentTime = time(NULL);
			struct tm tmTime;
			localtime_s (&tmTime, &currentTime );
			char szDate [ MAX_PATH ];
			RemoveCRLF ( szDate );
			asctime_s ( szDate, &tmTime );
			fprintf ( hFile, "Fill Disk for           : %s\n", pDisk );
			fprintf ( hFile, "Executed at             : %s\n", szDate );
			fprintf ( hFile, "Program                 : %s\n", PROGRAM_NAME_P );
			fprintf ( hFile, "Date                    : %s\n", PROGRAM_DATE_F );
			fprintf ( hFile, "Version                 : %s\n", PROGRAM_VERSION );
			fprintf ( hFile, "Sector Per Cluster      : %ld\n", (unsigned long) dwSectorPerCluster );
			fprintf ( hFile, "Byte Per Sector         : %ld\n", (unsigned long) dwBytePerSector );
			fprintf ( hFile, "Number Of Free Cluster  : %ld\n", (unsigned long) dwNumberOfFreeCluster );
			fprintf ( hFile, "Total Number Of Cluster : %ld\n", (unsigned long) dwTotalNumberOfCluster );
			fprintf ( hFile, "Seconds                 : %lld\n", elapsedFromStart );
			FormatDate ( (long) elapsedFromStart, szDate, sizeof ( szDate ) );
			fprintf ( hFile, "Duration                : %s\n", szDate );
			fprintf ( hFile, "Written                 : %lld\n", bytesWriten );
			fprintf ( hFile, "Real Written            : %lld\n", realWriten );
			fprintf ( hFile, "To Be Written           : %lld\n", toBeWritten );
			fprintf ( hFile, "Buffer Length           : %ld\n", ( long ) iBufferLengthMax );
			fprintf ( hFile, "Cluster Done            : %ld\n", clusterDone );

			fclose ( hFile );
		}
	}

	//
	return true;

}