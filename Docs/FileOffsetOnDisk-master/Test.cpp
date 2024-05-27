#include <iostream>
#include <windows.h>
#include "FileAddress.h"

using namespace std;

int main(int argc,char *argv[])
{
	HANDLE handle = OpenBitmapFile("E:\\2.txt");
	char *drv = "\\\\.\\E:";
	PVOLUME_LCN_INFO  pvolume_lcn_info;

	std::cout<<"sizeof volume lcn info "<<sizeof(VOLUME_LCN_INFO)+4*sizeof(LCN)<<std::endl;
	pvolume_lcn_info = (PVOLUME_LCN_INFO)malloc(sizeof(VOLUME_LCN_INFO)+4*sizeof(LCN));
	if(NULL == pvolume_lcn_info)
		std::cout<<"failed to alloc mem for lcn"<<GetLastError<<std::endl;
	
	GetDiskInfo(drv,pvolume_lcn_info);
	GetFileOffset(handle,pvolume_lcn_info);
	std::cout<<pvolume_lcn_info->lcn_count<<std::endl;

	for(int i = 0 ;i<pvolume_lcn_info->lcn_count;++i)
	{
		std::cout<<"offset "<<pvolume_lcn_info->lcn[i].lcn_offset<<std::endl;
		std::cout<<"length "<<pvolume_lcn_info->lcn[i].lcn_length<<std::endl;

	}

	if(0 != pvolume_lcn_info->lcn_count)
		WriteDataToFile(pvolume_lcn_info);

	free(pvolume_lcn_info);
	CloseHandle(handle);
	system("pause");
	return 0;
}