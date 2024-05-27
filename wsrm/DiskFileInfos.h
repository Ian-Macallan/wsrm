#ifndef DISKFILEINFOS_H
#define DISKFILEINFOS_H

#include "stdafx.h"

#include <Windows.h>
#include <winioctl.h>

//
#if 0
#pragma pack(push,1)

//
//	Bios Parameter Block
typedef struct BIOS_PARAMETER_BLOCK 
{
	USHORT				BytesPerSector;
	UCHAR				SectorsPerCluster;
	USHORT				ReservedSectors;
	UCHAR				Fats;
	USHORT				RootEntries;
	USHORT				Sectors;
	UCHAR				Media;
	USHORT				SectorsPerFat;
	USHORT				SectorsPerTrack;
	USHORT				Heads;
	ULONG				HiddenSectors;
	ULONG				LargeSectors;
} BIOS_PARAMETER_BLOCK, *PBIOS_PARAMETER_BLOCK;

//
//	Boot Sector
typedef struct _PACKED_BOOT_SECTOR 
{
    UCHAR					Jump[3];                                
    UCHAR					Oem[8];                                 
    BIOS_PARAMETER_BLOCK	PackedBpb;            
    UCHAR					Unused[4];                              
    LONGLONG				NumberSectors;           
    LONGLONG				MftStartLcn;                      
    LONGLONG				Mft2StartLcn;                             
    CHAR					ClustersPerFileRecordSegment;            
    UCHAR					Reserved0[3];
    CHAR					DefaultClustersPerIndexAllocationBuffer; 
    UCHAR					Reserved1[3];
    LONGLONG				SerialNumber;                        
    ULONG					Checksum;                               
    UCHAR					BootStrap[0x200-0x044];        
} PACKED_BOOT_SECTOR, *PPACKED_BOOT_SECTOR;
#pragma pack(pop)
#endif

//
#pragma pack(push,1)

typedef struct {
	UCHAR Jump[3];
	UCHAR Format[8];

	//
	USHORT BytesPerSector;
	UCHAR SectorsPerCluster;
	USHORT BootSectors;
	UCHAR Mbz1;
	USHORT Mbz2;
	USHORT Reserved1;
	UCHAR MediaType;
	USHORT Mbz3;
	USHORT SectorsPerTrack;
	USHORT NumberOfHeads;
	ULONG PartitionOffset;
	ULONG Reserved2[2];

	//
	ULONGLONG TotalSectors;
	ULONGLONG MftStartLcn;
	ULONGLONG Mft2StartLcn;
	ULONG ClustersPerFileRecord;
	ULONG ClustersPerIndexBlock;
	ULONGLONG VolumeSerialNumber;
	UCHAR Code[0x1AE];
	USHORT BootSignature;
} BOOT_BLOCK, *PBOOT_BLOCK;

/**
	TypeThe type of NTFS record.When the value of Typeis considered as a sequence of fourone-byte characters,
	it normally spells an acronym for the type. Defined values include:
	‘FILE’‘INDX’‘BAAD’‘HOLE’‘CHKD’
	UsaOffset The offset, in bytes, from the start of the structure to the Update Sequence Array.
	UsaCount The number of values in the Update Sequence Array.
	Usn The Update Sequence Number of the NTFS record
 **/
typedef struct {
	//	Type will be something as 'FILE'
	union
	{
		ULONG uType;
		char szType [ 4 ];
	} Type;

	USHORT UsaOffset;	// offset to fixup pattern
	USHORT UsaCount;	// Size of fixup-list +1
	USN Usn;			// log file seq number
} NTFS_RECORD_HEADER, *PNTFS_RECORD_HEADER;

/**
	SequenceNumber	The number of times that the MFT entry has been reused.
	LinkCount 	The number of directory links to the MFT entry.
	AttributeOffset The offset, in bytes, from the start of the structure to the first attribute of the MFTentry.
	Flags A bit array of flags specifying properties of the MFT entry.
		The values defined include:	
			InUse      0x0001  // The MFT entry is in use
			Directory  0x0002	// The MFT entry represents a directory
	BytesInUse The number of bytes used by the MFT entry.
	BytesAllocated The number of bytes allocated for the MFT entry.
	BaseFileRecord If the MFT entry contains attributes that overflowed a base MFT entry, 
		this membercontains the file reference number of the base entry; otherwise, 
		it contains zero.
	NextAttributeNumber The number that will be assigned to the next attribute added to the MFT entry.

	An entry in the MFT consists of a FILE_RECORD_HEADER followed by a sequence of attributes.
 **/

//
//	Type needed for interpreting the MFT-records
typedef struct {
	NTFS_RECORD_HEADER RecHdr;		// An NTFS_RECORD_HEADER structure with a Type of 'FILE'.
	USHORT SequenceNumber;			// Sequence number - The number of times									// sequence nr in MFT					
									// that the MFT entry has been reused.
	USHORT LinkCount;				// Hard link count - The number of directory links to the MFT entry			// Hard-link count
	USHORT AttributeOffset;			// Offset to the first Attribute - The offset, in bytes,					// Offset to seq of Attributes
									// from the start of the structure to the first attribute of the MFT		
	USHORT Flags;					// Flags - A bit array of flags specifying properties of the MFT entry		// 0x01 = In Use; 0x02 = Dir
									// InUse 0x0001 - The MFT entry is in use
									// Directory 0x0002 - The MFT entry represents a directory
	ULONG BytesInUse;				// Real size of the FILE record - The number of bytes used by the MFT entry.	//	Real size of the record
	ULONG BytesAllocated;			// Allocated size of the FILE record - The number of bytes						// Allocated size of the record
									// allocated for the MFT entry
	ULONGLONG BaseFileRecord;		// reference to the base FILE record - If the MFT entry contains				// ptr to base MFT rec or 0
									// attributes that overflowed a base MFT entry, this member
									// contains the file reference number of the base entry;
									// otherwise, it contains zero
	USHORT NextAttributeNumber;		// Next Attribute Id - The number that will be assigned to						// Minimum Identificator +1
									// the next attribute added to the MFT entry.
} FILE_RECORD_HEADER, *PFILE_RECORD_HEADER;

//
//////////////////////////////////////////////////////////////////////////////
//	From The Book
//////////////////////////////////////////////////////////////////////////////

typedef enum {
	AttributeStandardInformation = 0x10,
	AttributeAttributeList = 0x20,
	AttributeFileName = 0x30,
	AttributeObjectId = 0x40,
	AttributeSecurityDescriptor = 0x50,
	AttributeVolumeName = 0x60,
	AttributeVolumeInformation = 0x70,
	AttributeData = 0x80,
	AttributeIndexRoot = 0x90,
	AttributeIndexAllocation = 0xA0,
	AttributeBitmap = 0xB0,
	AttributeReparsePoint = 0xC0,
	AttributeEAInformation = 0xD0,
	AttributeEA = 0xE0,
	AttributePropertySet = 0xF0,
	AttributeLoggedUtilityStream = 0x100
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

/**
	The bitmap attribute contains an array of bits.The file “\$Mft” contains a bitmap
	attribute that records which MFT table entries are in use, and directories normally
	contain a bitmap attribute that records which index blocks contain valid entries.
 **/

/**
	The security descriptor attribute is stored on disk as a standard self-relative security
	descriptor.This attribute does not normally appear in MFT entries on NTFS 3.0 format
    volumes.
**/

/**
	AttributeType	The type of the attribute.The following types are defined:
	Length			The size, in bytes, of the resident part of the attribute.
	Nonresident		Specifies, when true, that the attribute value is nonresident.
	NameLength		The size, in characters, of the name (if any) of the attribute.
	NameOffset		The offset, in bytes, from the start of the structure to the attribute name.
					The attributename is stored as a Unicode string.
	Flags			A bit array of flags specifying properties of the attribute.
					The values defined include:
						Compressed     0x0001  // The attribute is compressed
	AttributeNumber A numeric identifier for the instance of the attribute.
 **/

//
//
typedef struct {
	ATTRIBUTE_TYPE AttributeType;
	ULONG Length;
	BOOLEAN Nonresident;
	UCHAR NameLength;
	USHORT NameOffset;
	USHORT Flags;               // 0x0001 = Compressed
	USHORT AttributeNumber;
} ATTRIBUTE, *PATTRIBUTE;

/**
	Attribute		An ATTRIBUTE structure containing members common to resident and nonresident attributes.
	ValueLength		The size, in bytes, of the attribute value.
	ValueOffset		The offset, in bytes, from the start of the structure to the attribute value.
	Flags			A bit array of flags specifying properties of the attribute.
					The values defined include:Indexed        0x0001  // The attribute is indexed
 **/

typedef struct {
	ATTRIBUTE Attribute;
	ULONG ValueLength;
	USHORT ValueOffset;
	USHORT Flags; 	// 0x0001 = Indexed
} RESIDENT_ATTRIBUTE, *PRESIDENT_ATTRIBUTE;

/**
Attribute			An ATTRIBUTE structure containing members common to resident and nonresidentattributes

LowVcn				The lowest valid Virtual Cluster Number (VCN) of this portion of the attribute value.
					Unless the attribute value is very fragmented (to the extent that an attribute list isneeded to describe it),
					there is only one portion of the attribute value, and the value ofLowVcn is zero.
HighVcn				The highest valid VCN of this portion of the attribute value.
RunArrayOffset		The offset,	in bytes, from the start of the structure to the run array that contains the mappings between VCNs and Logical Cluster Numbers (LCNs).
CompressionUnit		The compression unit for the attribute expressed as the logarithm to the base two ofthe number of clusters in a compression unit. 
					If CompressionUnitis zero, the attributeis not compressed.
AllocatedSize		The size, in bytes, of disk space allocated to hold the attribute value.
DataSize			The size, in bytes, of the attribute value.
					This may be larger than the AllocatedSizeifthe attribute value is compressed or sparse.
InitializedSize		The size, in bytes, of the initialized portion of the attribute value.
CompressedSize		The size, in bytes, of the attribute value after compression.
					This member is only present when the attribute is compressed.


 **/

typedef struct {
	ATTRIBUTE Attribute;
	ULONGLONG LowVcn;
	ULONGLONG HighVcn;
	USHORT RunArrayOffset;
	UCHAR CompressionUnit;
	UCHAR AlignmentOrReserved[5];
	ULONGLONG AllocatedSize;
	ULONGLONG DataSize;
	ULONGLONG InitializedSize;
	ULONGLONG CompressedSize;    // Only when compressed
} NONRESIDENT_ATTRIBUTE, *PNONRESIDENT_ATTRIBUTE;


/**

		CreationTime				The time when the file was created in the standard time format (that is, the number of
									100-nanosecond intervals since January 1, 1601).
		ChangeTime					The time when the file attributes were last changed in the standard time format (that
									is, the number of 100-nanosecond intervals since January 1, 1601).
		LastWriteTime				The time when the file was last written in the standard time format (that is, the number
									of 100-nanosecond intervals since January 1, 1601).
		LastAccessTime				The time when the file was last accessed in the standard time format (that is, the number
									of 100-nanosecond intervals since January 1, 1601).
		FileAttributes				The attributes of the file. Defined attributes include:
									FILE_ATTRIBUTE_READONLY
									FILE_ATTRIBUTE_HIDDEN
									FILE_ATTRIBUTE_SYSTEM
									FILE_ATTRIBUTE_DIRECTORY
									FILE_ATTRIBUTE_ARCHIVE
									FILE_ATTRIBUTE_NORMAL
									FILE_ATTRIBUTE_TEMPORARY
									FILE_ATTRIBUTE_SPARSE_FILE
									FILE_ATTRIBUTE_REPARSE_POINT
									FILE_ATTRIBUTE_COMPRESSED
									FILE_ATTRIBUTE_OFFLINE
									FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
									FILE_ATTRIBUTE_ENCRYPTED
	AlignmentOrReservedOrUnknown	Normally contains zero. Interpretation unknown
	QuotaId							A numeric identifier of the disk quota that has been charged for the file (probably an
									index into the file “\$Extend\$Quota”). If quotas are disabled, the value of QuotaId is
									zero.This member is only present in NTFS 3.0. If a volume has been upgraded from
									an earlier version of NTFS to version 3.0, this member is only present if the file has
									been accessed since the upgrade.
	SecurityId						A numeric identifier of the security descriptor that applies to the file (probably an
									index into the file “\$Secure”).This member is only present in NTFS 3.0. If a volume
									has been upgraded from an earlier version of NTFS to version 3.0, this member is
									only present if the file has been accessed since the upgrade.
	QuotaCharge						The size, in bytes, of the charge to the quota for the file. If quotas are disabled, the
									value of QuotaCharge is zero.This member is only present in NTFS 3.0. If a volume
									has been upgraded from an earlier version of NTFS to version 3.0, this member is
									only present if the file has been accessed since the upgrade.
	Usn								The Update Sequence Number of the file. If journaling is not enabled, the value of
									Usn is zero.This member is only present in NTFS 3.0. If a volume has been upgraded
									from an earlier version of NTFS to version 3.0, this member is only present if the file
									has been accessed since the upgrade.

 **/

typedef struct {
	ULONGLONG CreationTime;
	ULONGLONG ChangeTime;
	ULONGLONG LastWriteTime; 
	ULONGLONG LastAccessTime;
	ULONG FileAttributes; 
	ULONG AlignmentOrReservedOrUnknown[3];
	ULONG QuotaId;                        // NTFS 3.0 only
	ULONG SecurityId;                     // NTFS 3.0 only
	ULONGLONG QuotaCharge;                // NTFS 3.0 only
	USN Usn;                              // NTFS 3.0 only
} STANDARD_INFORMATION, *PSTANDARD_INFORMATION;

/**
		AttributeType				The type of the attribute.
		Length						The size, in bytes, of the attribute list entry.
		NameLength					The size, in characters, of the name (if any) of the attribute.
		NameOffset					The offset, in bytes, from the start of the ATTRIBUTE_LIST structure to the attribute
									name.The attribute name is stored as a Unicode string.
		LowVcn						The lowest valid Virtual Cluster Number (VCN) of this portion of the attribute value.
		FileReferenceNumber			The file reference number of the MFT entry containing the NONRESIDENT_ATTRIBUTE
									structure for this portion of the attribute value.
		AttributeNumber				A numeric identifier for the instance of the attribute.
 **/
typedef struct {
	ATTRIBUTE_TYPE AttributeType; 
	USHORT Length;
	UCHAR NameLength;
	UCHAR NameOffset;
	ULONGLONG LowVcn;
	ULONGLONG FileReferenceNumber;
	USHORT AttributeNumber;
	USHORT AlignmentOrReserved[3];
} ATTRIBUTE_LIST, *PATTRIBUTE_LIST;

/**
	DirectoryFileReferenceNumber	The file reference number of the directory in which the filename is entered.
	CreationTime					The time when the file was created in the standard time format (that is. the number of
									100-nanosecond intervals since January 1, 1601).This member is only updated when
									the filename changes and may differ from the field of the same name in the STANDARD_
									INFORMATION structure.
	ChangeTime						The time when the file attributes were last changed in the standard time format (that
									is, the number of 100-nanosecond intervals since January 1, 1601).This member is
									only updated when the filename changes and may differ from the field of the same
									name in the STANDARD_INFORMATION structure
	LastWriteTime					The time when the file was last written in the standard time format (that is, the number
									of 100-nanosecond intervals since January 1, 1601).This member is only updated
									when the filename changes and may differ from the field of the same name in the
									STANDARD_INFORMATION structure.
	LastAccessTime					The time when the file was last accessed in the standard time format (that is, the number
									of 100-nanosecond intervals since January 1, 1601).This member is only updated
									when the filename changes and may differ from the field of the same name in the
									STANDARD_INFORMATION structure.
	AllocatedSize					The size, in bytes, of disk space allocated to hold the attribute value.This member is
									only updated when the filename changes.
	DataSize						The size, in bytes, of the attribute value.This member is only updated when the filename
									changes.
	FileAttributes					The attributes of the file.This member is only updated when the filename changes and
									may differ from the field of the same name in the STANDARD_INFORMATION structure.
	NameLength						The size, in characters, of the filename.
	NameType						The type of the name.A type of zero indicates an ordinary name, a type of one indicates
									a long name corresponding to a short name, and a type of two indicates a short
									name corresponding to a long name.
	Name							The name, in Unicode, of the file.

 **/
typedef struct {
	ULONGLONG DirectoryFileReferenceNumber;
	ULONGLONG CreationTime;   // Saved when filename last changed
	ULONGLONG ChangeTime;     // ditto
	ULONGLONG LastWriteTime;  // ditto
	ULONGLONG LastAccessTime; // ditto
	ULONGLONG AllocatedSize;  // ditto
	ULONGLONG DataSize;       // ditto
	ULONG FileAttributes;     // ditto
	ULONG AlignmentOrReserved;
	UCHAR NameLength;
	UCHAR NameType;           // 0x01 = Long, 0x02 = Short
	WCHAR Name[1];
} FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;

/**

	ObjectId			The unique identifier assigned to the file.
	BirtVolumeId		The unique identifier of the volume on which the file was first created. Need not be
						present.
	BirthObjectId		The unique identifier assigned to the file when it was first created. Need not be
						present.
	DomainId			Reserved. Need not be present.

 **/
typedef struct {
	GUID ObjectId;
	union {
		struct {
			GUID BirthVolumeId;
			GUID BirthObjectId;
			GUID DomainId;
		} ;
		UCHAR ExtendedInfo[48];
	};
} OBJECTID_ATTRIBUTE, *POBJECTID_ATTRIBUTE;

/**
	Unknown				Interpretation unknown.
	MajorVersion		The major version number of the NTFS format.
	MinorVersion		The minor version number of the NTFS format.

 **/
typedef struct {
	ULONG Unknown[2];
	UCHAR MajorVersion;
	UCHAR MinorVersion;
	USHORT Flags;
} VOLUME_INFORMATION, *PVOLUME_INFORMATION;

/**
	EntriesOffset		The offset, in bytes, from the start of the structure to the first DIRECTORY_ENTRY
						structure.
	IndexBlockLength	The size, in bytes, of the portion of the index block that is in use.
	AllocatedSize		The size, in bytes, of disk space allocated for the index block.
	Flags				A bit array of flags specifying properties of the index.The values defined include:
						SmallDirectory	0x0000 // Directory fits in index root
						LargeDirectory	0x0001 // Directory overflows index root


 **/
typedef struct {
	ULONG EntriesOffset;
	ULONG IndexBlockLength;
	ULONG AllocatedSize;
	ULONG Flags;         // 0x00 = Small directory, 0x01 = Large directory
} DIRECTORY_INDEX, *PDIRECTORY_INDEX;

/**
	Type						The type of the attribute that is indexed.
	CollationRule				A numeric identifier of the collation rule used to sort the index entries.
	BytesPerIndexBlock			The number of bytes per index block.
	ClustersPerIndexBlock		The number of clusters per index block.
	DirectoryIndex				A DIRECTORY_INDEX structure.

 **/
typedef struct {
	ATTRIBUTE_TYPE Type;
	ULONG CollationRule;
	ULONG BytesPerIndexBlock;
	ULONG ClustersPerIndexBlock;
	DIRECTORY_INDEX DirectoryIndex;
} INDEX_ROOT, *PINDEX_ROOT;

/**
	Ntfs				An NTFS_RECORD_HEADER structure with a Type of ‘INDX’.
	IndexBlockVcn		The VCN of the index block.
	DirectoryIndex		A DIRECTORY_INDEX structure.
 **/
typedef struct {
	NTFS_RECORD_HEADER Ntfs;
	ULONGLONG IndexBlockVcn;
	DIRECTORY_INDEX DirectoryIndex; 
} INDEX_BLOCK_HEADER, *PINDEX_BLOCK_HEADER;

/**

	FileReferenceNumber		The file reference number of the file described by the directory entry.
	Length					The size, in bytes, of the directory entry.
	AttributeLength			The size, in bytes, of the attribute that is indexed.
	Flags					A bit array of flags specifying properties of the entry.The values defined include:
							HasTrailingVcn 0x0001 // A VCN follows the indexed attribute
							LastEntry 0x0002 // The last entry in an index block
 **/
typedef struct {
	ULONGLONG FileReferenceNumber; 
	USHORT Length;
	USHORT AttributeLength;
	ULONG Flags;           // 0x01 = Has trailing VCN, 0x02 = Last entry// 
	FILENAME_ATTRIBUTE Name;// 
	ULONGLONG Vcn;      // VCN in IndexAllocation of earlier entries
} DIRECTORY_ENTRY, *PDIRECTORY_ENTR;


/**
	ReparseTag				The reparse tag identifies the type of reparse point.The high order three bits of the tag
							indicate whether the tag is owned by Microsoft, whether there is a high latency in
							accessing the file data, and whether the filename is an alias for another object.
	ReparseDataLength		The size, in bytes, of the reparse data in the ReparseData member.
	ReparseData				The reparse data.The interpretation of the data depends upon the type of the reparse
point.
 **/
typedef struct {
	ULONG ReparseTag;
	USHORT ReparseDataLength; 
	USHORT Reserved;
	UCHAR ReparseData[1];
} REPARSE_POINT, *PREPARSE_POINT;


/**
	EaLength				The size, in bytes, of the extended attribute information.
	EaQueryLength			The size, in bytes, of the buffer needed to query the extended attributes when calling
							ZwQueryEaFile.
 **/
typedef struct {
	ULONG EaLength;
	ULONG EaQueryLength;
} EA_INFORMATION, *PEA_INFORMATION;

/**
	NextEntryOffset			The number of bytes that must be skipped to get to the next entry.
	Flags					A bit array of flags qualifying the extended attribute.
	EaNameLength			The size, in bytes, of the extended attribute name.
	EaValueLength			The size, in bytes, of the extended attribute value.
	EaName					The extended attribute name.
	EaData					The extended attribute data.
 **/
typedef struct {
	ULONG NextEntryOffset; 
	UCHAR Flags;
	UCHAR EaNameLength;
	USHORT EaValueLength;
	CHAR EaName[1];	// 	UCHAR EaData[];
} EA_ATTRIBUTE, *PEA_ATTRIBUTE;

/**

 **/
typedef struct {
	WCHAR AttributeName[64];
	ULONG AttributeNumber;
	ULONG Unknown[2];
	ULONG Flags;
	ULONGLONG MinimumSize;
	ULONGLONG MaximumSize;
} ATTRIBUTE_DEFINITION, *PATTRIBUTE_DEFINITION;

#pragma pack(pop)

//
//	LCN Structure
typedef struct _LCN
{
	//
	//	Offset From Volume
	LONGLONG llLcnOffset;
	LONGLONG llLcnLength;
} *PLCN, LCN;

typedef struct _FILE_LCN
{
	LONG		lLcnCount;
	LCN			lcnOffsetAndLength[1];
} *PFILE_LCN, FILE_LCN;

//
//	Volume Info
typedef struct _VOLUME_DISK_INFO
{
	LONG								DiskNumber;
	WCHAR								szDrive [ MAX_PATH ];
	LONG								SectorsPerCluster;
	LONG								BytesPerSector;
	LONG								ClustersPerFileRecord;
	LONG								ClustersPerIndexBlock;

	//
	PNTFS_FILE_RECORD_OUTPUT_BUFFER		pMFTRecordOututBuffer;
	PFILE_RECORD_HEADER					pMFTRecordHeader;

	//
	LONG								lBytesPerFileRecord;
	LONGLONG							llVolumeOffset;
	LONGLONG							llFileRecordCount;
} *PVOLUME_DISK_INFO, VOLUME_DISK_INFO;


//
typedef enum {
		GET_VOLUME_DATA_NO	= 0,
		GET_VOLUME_DATA_DO	= 1
} GET_VOLUME_DATA;


//
//	External Functions

//	Open Volume
extern HANDLE OpenBitmapAndCreateMapping ( WCHAR *lpFullPathname );

//	Get Info on Volume
extern BOOL GetVolumeDiskInfo (	WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo );

//
extern BOOL GetFileInformation ( WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo, GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber );
extern BOOL LocateFile ( WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo, GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber );

//
extern BOOL CopyFileInformation ( WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo, GET_VOLUME_DATA bGetVolumeData, ULONG iFileNumber, WCHAR *pOutfile );

//	Get File Clusters
extern BOOL GetFileOffset ( HANDLE handle, PVOLUME_DISK_INFO lpVolumeLcnInfo, PFILE_LCN lpFileLcn, int iLcnMax );

//	Show Files
extern BOOL ListFiles ( WCHAR *pDrive, PVOLUME_DISK_INFO lpVolumeLcnInfo, BOOL bDirectoryOnly, BOOL bDeletedOnly, GET_VOLUME_DATA bGetVolumeData, 
						WCHAR *pFilter, WCHAR *pNoMatch );

//	Convert Date to String
extern WCHAR *ConvertDateTime ( ULONGLONG llFileTime, WCHAR *pSystemTime, BOOL bShortTime );


#endif 
