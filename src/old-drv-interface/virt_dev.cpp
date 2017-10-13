////
#include <WinSock2.h>
#include <InitGuid.h>
#include <WinIoCtl.h>
#include <stdio.h>
//#include "usb_info.h"
#include <SetupAPI.h>
#pragma comment(lib,"setupapi")
#include <cfgmgr32.h>
#include "ioctl.h"
//#include "../../usb_include/usb.h"
#include "virt_dev.h"

struct virt_usb_t
{
	HANDLE hFile;

	HANDLE hSemaphore;
	int    bus_addr;

	int buf_size;
};

static char* find_virt_usb_path(char* dev_path)
{
	HDEVINFO devList;
	DWORD index = 0;
	SP_DEVINFO_DATA devInfo = { sizeof(SP_DEVINFO_DATA) };
	SP_DEVICE_INTERFACE_DATA         devInter = { sizeof(SP_DEVICE_INTERFACE_DATA) };

	////

	devList = SetupDiGetClassDevs(&GUID_XUSB_VIRT_INTERFACE, NULL, NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (devList == (HANDLE)-1){
		printf("SetupDiGetClassDevs: [%s] error=%d\n", "USB", GetLastError());
		return NULL;
	}

	while (SetupDiEnumDeviceInfo(devList, index++, &devInfo)){
		char buf[4096];
		PSP_DEVICE_INTERFACE_DETAIL_DATA devDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buf;
		devDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if (!SetupDiEnumDeviceInterfaces(devList, NULL, &GUID_XUSB_VIRT_INTERFACE, index - 1, &devInter)){
			printf("SetupDiEnumDeviceInterfaces err\n");
			continue;
		}

		if (!SetupDiGetDeviceInterfaceDetail(devList, &devInter, devDetail, sizeof(buf), NULL, NULL)){
			printf("SetupDiGetDeviceInterfaceDetail err\n");
			continue;
		}
		/////
		strcpy(dev_path, devDetail->DevicePath); ////

		///////////////
	}
	SetupDiDestroyDeviceInfoList(devList);

	if (dev_path[0] == 0) return NULL;

	return dev_path;
}

void* virt_usb_open()
{
	char dev_path[260] = { 0 };
	if (!find_virt_usb_path(dev_path)){
		printf("*** not found virtual usb path.\n");
		return NULL;
	}

	HANDLE hFile = CreateFile(dev_path, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE){
		printf("open usb virtual device err=%d\n", GetLastError());
		
		return NULL;
	}

	virt_usb_t* dev = new virt_usb_t;
	dev->hFile = hFile;
	dev->hSemaphore = NULL;
	dev->bus_addr = 0;
	dev->buf_size = 1024 * 64;

	return dev;
}
void virt_usb_close(void* handle)
{
	virt_usb_t* dev = (virt_usb_t*)handle;
	if (!dev) return ;
	////
	if (dev->hSemaphore){
		CloseHandle((HANDLE)dev->hSemaphore);
		dev->hSemaphore = 0;
	}

	CloseHandle(dev->hFile);

	delete dev; 
}

int virt_usb_plugin(void* handle,const char* hw_ids, const char* comp_ids)
{
	virt_usb_t* dev = (virt_usb_t*)handle;
	if (!dev) return -1;
	////
	int bus_addr = 0;
	ioctl_pdo_create_t cp; memset(&cp, 0, sizeof(cp));

	const wchar_t* devid;
	devid = L"usb\\vid_05ac&pid_12a8";
//	devid = L"usb\\vid_05ca&pid_18c6";
//	devid = L"usb\\vid_0ac8&pid_3420"; // 0ac8, 3420
//	devid = L"usb\\vid_0781&pid_5580"; // U store

//	wcscpy(cp.hardware_ids, devid);
//	wcscpy(cp.compatible_ids, L"USB\\COMPOSITE");

	///
	if (!hw_ids || strlen(hw_ids) == 0) {
		return -1;
	}
	wchar_t t[HW_IDS_COUNT]; int i; memset(t, 0, sizeof(t));
	MultiByteToWideChar(CP_ACP, 0, hw_ids, -1, t, 259);
	t[258] = 0; t[259] = 0; //两个零结尾
	for (i = 0; i < 260; ++i){
		if (t[i] == L'\n') t[i] = L'\0';
	}
	memcpy(cp.hardware_ids, t, sizeof(t)); 

	if (comp_ids){
		memset(t, 0, sizeof(t));
		MultiByteToWideChar(CP_ACP, 0, comp_ids, -1, t, 259);
		t[258] = 0; t[259] = 0; //两个0结尾
		for (i = 0; i < 260; ++i){
			if (t[i] == L'\n') t[i] = L'\0';
		}
		memcpy(cp.compatible_ids, t, sizeof(t));
		//////
	}
	/////

	HANDLE hsem = CreateSemaphore(NULL, 0, MAXLONG, NULL);
	cp.hSemaphore = (ULONGLONG)hsem;
	dev->hSemaphore = hsem;

	DWORD bytes;
	BOOL bRet = DeviceIoControl(dev->hFile, IOCTL_PDO_ADD, &cp, sizeof(cp), &bus_addr, sizeof(int), &bytes, NULL);
	if (bRet) dev->bus_addr = bus_addr;

	DWORD err = GetLastError();
	printf("IOCTL_PDO_ADD: ret=%d,err=%d\n", bRet, err );

	return bRet ? 0: -1;
}
int virt_usb_unplug(void* handle)
{
	virt_usb_t* dev = (virt_usb_t*)handle;
	if (!dev) return -1;
	////
	DWORD bytes;
	BOOL bRet = DeviceIoControl(dev->hFile, IOCTL_PDO_REMOVE, NULL, 0, NULL, 0, &bytes, NULL);
	if (bRet){
		if (dev->hSemaphore){
			CloseHandle((HANDLE)dev->hSemaphore);
			dev->hSemaphore = 0;
		}
	}

	return bRet?0:-1;
}

usbtx_header_t* virt_usb_begin(void* handle )
{
	virt_usb_t* dev = (virt_usb_t*)handle;
	if (!dev) return NULL;
	//////
	if( WaitForSingleObject(dev->hSemaphore, 10*1000 ) == WAIT_TIMEOUT ) 
		return NULL; /////
	///////
	int buf_size = dev->buf_size;

	ioctl_usbtx_header_t* hdr = (ioctl_usbtx_header_t*)malloc(buf_size);

	DWORD bytes=0;
	memset(hdr, 0, sizeof(ioctl_usbtx_header_t)); 
	BOOL bRet = DeviceIoControl(dev->hFile, IOCTL_BEGIN_TRANS_USB_DATA, NULL, 0, hdr,buf_size, &bytes, NULL);
	if (!bRet){
		if (GetLastError() == ERROR_MORE_DATA  ){ // -> STATUS_BUFFER_OVERFLOW
			printf("buffer too small.\n");
			if (bytes != sizeof(ioctl_usbtx_header_t) ){ free(hdr); printf("bufferoverflow: ret not valid.\n"); return NULL; }
			buf_size = hdr->data_length + sizeof(ioctl_usbtx_header_t) + 4096; ///
			
			dev->buf_size = buf_size; 

			free(hdr);
			hdr = (ioctl_usbtx_header_t*)malloc(buf_size);

			bytes = 0;
			bRet = DeviceIoControl(dev->hFile, IOCTL_BEGIN_TRANS_USB_DATA, NULL, 0, hdr, buf_size, &bytes, NULL);
			////
		}
	}
	if (!bRet){
		printf("IOCTL begin: err=%d\n", GetLastError() );
		free(hdr);
		return NULL;
	}
	////
	if (hdr->data_length + sizeof(ioctl_usbtx_header_t) > buf_size){ //
		printf("---- buffer not larger.\n");
		buf_size = hdr->data_length + sizeof(ioctl_usbtx_header_t) + 4096; ///

		hdr = (ioctl_usbtx_header_t*)realloc(hdr, buf_size);

		dev->buf_size = buf_size; 
	}
	//////
	usbtx_header_t* ret = (usbtx_header_t*)( (char*)hdr + 24 );
	ret->data_length = hdr->data_length; 
	ret->result = (int)bytes - sizeof(ioctl_usbtx_header_t);

	return  ret;
	/////
}

int virt_usb_end(void* handle, usbtx_header_t* header )
{
	virt_usb_t* dev = (virt_usb_t*)handle;
	if (!dev) return -1;
	//////
	ioctl_usbtx_header_t* hdr = (ioctl_usbtx_header_t*)((char*)header - 24); 
	hdr->result = header->result;
	hdr->data_length = header->data_length;

	/////
	DWORD bytes;
	BOOL bRet = DeviceIoControl(dev->hFile, IOCTL_END_TRANS_USB_DATA, hdr, sizeof(ioctl_usbtx_header_t) + hdr->data_length, NULL, 0, &bytes, NULL);
//	printf("virt_usb_end: bRet=%d, err=%d\n", bRet, GetLastError());

	free(hdr);

	return 0;
}


#if 0

#include "usb_dev.h"

void cbk(void* header, void* data, int ret, void* param)
{
	usbtx_header_t* hdr = (usbtx_header_t*)header;
	if (ret >= 0){
		hdr->result = 0;
		hdr->data_length = ret;
	}
	else{
		hdr->result = -1;
	}
	
//	printf("**TRANS** <--end IOCTL: type=%d, trans_type2=%d, Ret=%d\n\n", hdr->type, hdr->descriptor.type, ret );

	virt_usb_end(param, hdr); 
	/////
}

void test()
{
	list<usb_info_t> usb_infos;
	enum_usb_devices(&usb_infos);
	const char* dev_id;
	void* usb = 0;

	dev_id = "usb\\vid_05ac&pid_12a8";
//	dev_id = "usb\\vid_05ca&pid_18c6";
//	dev_id = "usb\\vid_0ac8&pid_3420";
//	dev_id = "usb\\vid_0781&pid_5580"; // U 盘
	for (list<usb_info_t>::iterator it = usb_infos.begin(); it != usb_infos.end(); ++it){
		if (strnicmp(it->dev_id, dev_id, strlen(dev_id)) == 0){
			printf("**** [%s] [%s] [%s] \n", it->hw_id, it->dev_id, it->dev_desc);

			usb = xusb_open(&(*it));
			//	break;
		}
	}
//	xusb_close(usb); exit(0);
	////
	void* h = virt_usb_open();
	virt_usb_add(h);

	while (1){
		///
		usbtx_header_t* hdr = virt_usb_begin(h );
		if (!hdr)continue;
		////
	//	printf("-->begin IOCTL: type=%d, type2=%d, dataLen=%d ", hdr->type, hdr->descriptor.type, hdr->data_length );
	//	if (hdr->type == 3) printf(", bRead=%d, ep_address=0x%X\n", hdr->transfer.is_read, hdr->transfer.ep_address); else printf("\n");

		int r;
	
		///
		r = xusb_ioctl(usb, hdr, hdr->data, hdr->data_length, cbk, h );

//		printf("<--end IOCTL: type=%d, type2=%d, Ret=%d\n\n", hdr->type,hdr->descriptor.type, r );
		/////

	}
	//////
//	xusb_close(usb);
}

int main(int argc, char** argv)
{

//	void* h = virt_usb_open();
//	virt_usb_add(h);
	int sz = sizeof(usbtx_header_t);

	test();
	return 0;
}

#endif


