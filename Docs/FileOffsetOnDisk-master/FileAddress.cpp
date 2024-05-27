#include <iostream>
#include <windows.h>
#include "FileAddress.h"

//open file ,and set file size. if file size < 1KB, MFT can store the data engouth,
//so FSCTL_GET_RETRIEVAL_POINTERS ioctl will failed by error code  ERROR_HANDLE_EOF.
HANDLE OpenBitmapFile(const char * path)
{
	if(NULL == path)
		return NULL;

	HANDLE file_handle = NULL;
	HANDLE mapfile_handle = NULL;

	file_handle = CreateFileA(path,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if(INVALID_HANDLE_VALUE == file_handle)
	{
		std::cout<<"open file failed ,errror = "<<GetLastError()<<std::endl;
	}
	else
	{
		std::cout<<"success open file "<<path<<std::endl;
	}

	// set file size ,2M
	mapfile_handle = CreateFileMapping(file_handle,NULL,PAGE_READWRITE | SEC_COMMIT,0,1024*1024*2,NULL);
	if(NULL == mapfile_handle)
		std::cout<<"failed to create file map "<<GetLastError()<<std::endl;

	if(mapfile_handle)
		CloseHandle(mapfile_handle);

	return file_handle;
}

//get volume sectors per clusters, bytes per sectors
//get volume disk index ,volume offset on disk;
void   GetDiskInfo(const char* volume_lable,PVOLUME_LCN_INFO pvolume_lcn_info)
{
	if(NULL == volume_lable)
		return ;

	HANDLE volume_handle = NULL;

	volume_handle = CreateFileA(volume_lable,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if(INVALID_HANDLE_VALUE == volume_handle)
	{
		std::cout<<"open volume failed ,errror = "<<GetLastError()<<std::endl;
		return ;
	}
	else
	{
		std::cout<<"success open volume "<<volume_lable<<std::endl;
	}

	PACKED_BOOT_SECTOR   boot_sector;
	DWORD               retbytes = 0;

	//can be replaced by GetDiskFreeSpace
	SetFilePointer(volume_handle,0,0,FILE_BEGIN);
	memset(&boot_sector,0,sizeof(PACKED_BOOT_SECTOR));
	BOOL ret = ReadFile(volume_handle,
		&boot_sector,
		512,
		&retbytes,
		NULL);
	if(!ret)
	{
		std::cout<<"failed to read first sector "<<GetLastError()<<std::endl;
		std::cout<<"sizeof packed_boot_sector "<<sizeof(PACKED_BOOT_SECTOR)<<std::endl;
		goto end;
	}

	std::cout<<"ntfs cluster "<<(int)(boot_sector.PackedBpb.SectorsPerCluster)<<std::endl;
	std::cout<<"ntfs sector size "<<boot_sector.PackedBpb.BytesPerSector<<std::endl;
	pvolume_lcn_info->sectors_per_clusters = boot_sector.PackedBpb.SectorsPerCluster;
	pvolume_lcn_info->bytes_per_setcor    = boot_sector.PackedBpb.BytesPerSector;

	int disk_num = 0;

	//get volume disk index ,volume offset on disk;
	GetVolumeOffsetOnDisk(volume_handle,pvolume_lcn_info->disk_num,pvolume_lcn_info->volume_offset);

end:
	CloseHandle(volume_handle);
}

void   GetVolumeOffsetOnDisk(HANDLE handle,LONG &disk_num,LONGLONG &volume_offset)
{
	VOLUME_DISK_EXTENTS   volume_extents;
	DWORD                 retbytes = 0;

	BOOL ret = DeviceIoControl(handle,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		NULL,
		NULL,
		&volume_extents,
		sizeof(VOLUME_DISK_EXTENTS),
		&retbytes,
		NULL);
	if(!ret)
	{
		std::cout<<"failed to get volume disk extents "<<GetLastError()<<std::endl;
	}

	disk_num = volume_extents.Extents[0].DiskNumber;
	volume_offset = volume_extents.Extents[0].StartingOffset.QuadPart;

	std::cout<<"volume disknum "<<disk_num<<" offset "<<volume_offset<<std::endl;
}

void   GetFileOffset(HANDLE handle,PVOLUME_LCN_INFO pvolume_lcn_info)
{
	if(NULL == handle)
	{
		std::cout<<"handle is null"<<std::endl;
		return ;
	}

	STARTING_VCN_INPUT_BUFFER  vcn_buffer;
	RETRIEVAL_POINTERS_BUFFER  retrieval_buffer;
	DWORD                      retbytes = 0;
	DWORD                      error = 0;
	LONGLONG                   pre_vcn;
	LONG                       vcn_num = 0;

	vcn_buffer.StartingVcn.QuadPart = pre_vcn = 0;

	// every time its return only one Extent, if there are many ,you need get many times
	do
	{
		BOOL ret = DeviceIoControl(handle,
			FSCTL_GET_RETRIEVAL_POINTERS,
			&vcn_buffer,
			sizeof(STARTING_VCN_INPUT_BUFFER),
			&retrieval_buffer,
			sizeof(RETRIEVAL_POINTERS_BUFFER),
			&retbytes,
			NULL);

		error = GetLastError();
		switch(error)
		{
		case ERROR_HANDLE_EOF:
			std::cout<<"file record is end of "<<std::endl;
			break;
		case ERROR_MORE_DATA:
			vcn_buffer.StartingVcn = retrieval_buffer.Extents[0].NextVcn;
			std::cout<<"starting vcn is "<<retrieval_buffer.StartingVcn.QuadPart<<std::endl;
			std::cout<<"new starting vcn is "<<vcn_buffer.StartingVcn.QuadPart<<std::endl;
		case NO_ERROR:
			std::cout<<"sector cluster offset is "<<retrieval_buffer.Extents[0].Lcn.QuadPart<<std::endl;
			std::cout<<"sector cluster length is "<<retrieval_buffer.Extents[0].NextVcn.QuadPart-pre_vcn<<std::endl;
			pre_vcn = vcn_buffer.StartingVcn.QuadPart;

			if(++vcn_num > 4)
			{
				std::cout<<"need more data"<<std::endl;
				return ;
			}

			pvolume_lcn_info->lcn_count = vcn_num;
			pvolume_lcn_info->lcn[pvolume_lcn_info->lcn_count-1].lcn_offset = retrieval_buffer.Extents[0].Lcn.QuadPart * (LONGLONG)pvolume_lcn_info->sectors_per_clusters*(LONGLONG)pvolume_lcn_info->bytes_per_setcor \
				+ (LONGLONG)pvolume_lcn_info->volume_offset;
			pvolume_lcn_info->lcn[pvolume_lcn_info->lcn_count-1].lcn_length = (retrieval_buffer.Extents[0].NextVcn.QuadPart-pre_vcn)*pvolume_lcn_info->sectors_per_clusters * pvolume_lcn_info->bytes_per_setcor;
			std::cout<<"retrieval externt count "<<retrieval_buffer.ExtentCount<<std::endl;
			std::cout<<"vcn num is "<<pvolume_lcn_info->lcn_count<<std::endl;

			break;
		default:
			std::cout<<"error code is "<<error<<std::endl;
			break;
		};
	}while(ERROR_MORE_DATA == error);
}

//write success ,the data will in disk which the file data are,but you can't see when you open the file manual
//because we hav't update cluster used bitmap in MFT
void   WriteDataToFile(PVOLUME_LCN_INFO pvolume_lcn_info)
{
	char disk_name[MAX_PATH] ;

	memset(disk_name,0,MAX_PATH);
	sprintf(disk_name,"\\\\.\\PhysicalDrive%d",pvolume_lcn_info->disk_num);
	std::cout<<disk_name<<std::endl;

	HANDLE disk_handle = CreateFileA(disk_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if(INVALID_HANDLE_VALUE == disk_handle)
	{
		std::cout<<"failed to open disk "<<GetLastError()<<std::endl;
		return ;
	}

	char buffer[512] ;
	memset(buffer,0x31,512);
	DWORD  retbytes = 0;

	//my file offset is 1988550144 bytes ,32 bits is engough,so high move is NULL;
	SetFilePointer(disk_handle,pvolume_lcn_info->lcn[0].lcn_offset,NULL,FILE_BEGIN);
	BOOL ret = WriteFile(disk_handle,buffer,sizeof(buffer),&retbytes,NULL);
	if(ret)
		std::cout<<"success write buf "<<buffer<<" to disk "<<std::endl;

	SetFilePointer(disk_handle,pvolume_lcn_info->lcn[0].lcn_offset,NULL,FILE_BEGIN);
	ret = ReadFile(disk_handle,buffer,sizeof(buffer),&retbytes,NULL);
	if(ret)
		std::cout<<"success read buf "<<buffer<<std::endl;

	CloseHandle(disk_handle);
	return ;
}