#ifndef FILE_ADDRESS_H
#define FILE_ADDRESS_H

#pragma pack(push,1)
typedef struct BIOS_PARAMETER_BLOCK 
{
    USHORT BytesPerSector;
    UCHAR  SectorsPerCluster;
    USHORT ReservedSectors;
    UCHAR  Fats;
    USHORT RootEntries;
    USHORT Sectors;
    UCHAR  Media;
    USHORT SectorsPerFat;
    USHORT SectorsPerTrack;
    USHORT Heads;
    ULONG  HiddenSectors;
    ULONG  LargeSectors;
} BIOS_PARAMETER_BLOCK,*PBIOS_PARAMETER_BLOCK;

typedef struct _PACKED_BOOT_SECTOR 
{
    UCHAR Jump[3];                                
    UCHAR Oem[8];                                 
    BIOS_PARAMETER_BLOCK PackedBpb;            
    UCHAR Unused[4];                              
    LONGLONG NumberSectors;           
    LONGLONG MftStartLcn;                      
    LONGLONG Mft2StartLcn;                             
    CHAR ClustersPerFileRecordSegment;            
    UCHAR Reserved0[3];
    CHAR DefaultClustersPerIndexAllocationBuffer; 
    UCHAR Reserved1[3];
    LONGLONG SerialNumber;                        
    ULONG Checksum;                               
    UCHAR BootStrap[0x200-0x044];        
} PACKED_BOOT_SECTOR,*PPACKED_BOOT_SECTOR;
#pragma pack(pop)

typedef struct _LCN
{
	LONGLONG lcn_offset;
		LONGLONG lcn_length;
}*PLCN,LCN;

typedef struct _VOLUME_LCN_INFO
{
	LONG   lcn_count;
	LONG   disk_num;
	LONG   sectors_per_clusters;
	LONG   bytes_per_setcor;
	LONGLONG volume_offset;
	LCN      lcn[1];
}*PVOLUME_LCN_INFO,VOLUME_LCN_INFO;

HANDLE OpenBitmapFile(const char * path);
void   GetDiskInfo(const char* volume_lable,PVOLUME_LCN_INFO pvolume_lcn_info);
void   GetVolumeOffsetOnDisk(HANDLE handle,LONG &disk_num,LONGLONG &volume_offset);
void   GetFileOffset(HANDLE handle,PVOLUME_LCN_INFO pvolume_lcn_info);
void   WriteDataToFile(PVOLUME_LCN_INFO pvolume_lcn_info);
void FreeBitmapFile();

#endif 