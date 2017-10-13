/// by fanxiushu 2016-10-01
///符合 USB Video Class 协议的摄像头 版本 1.0
///驱动接口来自本人开发的虚拟USB驱动。
///处理了主要的部分协议, 如需要完善，可在此代码基础上修改完善

////
#include <Windows.h>
#include "uvc_struct.h"
#include "virt_dev.h"
#include "uvc_vcam.h"

#define EP_ADDR    0x82 //用于视频流传输的端口

class uvc_vcam
{
protected:
	int curr_config;      ///当前配置接口
	int curr_interface;   ///当前接口
	int curr_alt_setting; ////当前接口的选择子

	int curr_format_index;
	int curr_frame_index; 

protected:
	///
	int iso_ep_addr;

	int format_count; ///
	GUID guidFormat; /// RGB24 default
	int  bitsPerPixel; ///

	int frame_count;
	struct {
		int width;
		int height; 
	}frames[10]; ///

	///用于视频传输过程
	int frame_length;
	char* frame_buffer;
	int frame_pos; ///当前帧的传输位置
	unsigned char frame_flip; ///每个帧进行 0 和 1 翻转

	int frame_delay_msec; //每个帧的停留时间，默认 33 毫秒 ,用于控制速度 fps 
	//
	usb_device_descriptor dev_desc; ///
	///
	char str_buffer[1*1024];
	int  str_count;
	usb_string_descriptor* str_desc[10]; //定义10个，其实就 0,1,2
	///
	char cfg_buffer[16*1024];
	int  cfg_length; ///
	usb_config_descriptor* cfg_desc;
	/////

	void fill_dev_desc(unsigned short pid, unsigned int vid);
	void fill_str_desc(const char* manu_fact, const char* product);
	void fill_cfg_desc();

protected:
	FRAME_CALLBACK frame_callback; ///
	void* cbk_param;

	void* usb_ptr; 
	HANDLE hThread;
	bool quit;
	static DWORD CALLBACK __thread(void* _p) {
		uvc_vcam* uvc = (uvc_vcam*)_p;
		uvc->loop(); 
		return 0;
	}
	void loop();

	void descriptor(usbtx_header_t* hdr); //处理描述相关
	void vendor_control(usbtx_header_t* hdr); //
	void control_transfer(usbtx_header_t* hdr); //
	void iso_transfer(usbtx_header_t* hdr);
	int  video_encode(unsigned char* vid_buf, int vid_len); //每个iso packet都得有个player header

public:
	uvc_vcam();
	~uvc_vcam();
	////
public:
	int create( struct uvc_vcam_t* uvc );
};

uvc_vcam::uvc_vcam()
{
	curr_config = -1;
	curr_interface = -1;
	curr_alt_setting = -1; ///-1 表示当前没选择任何接口

	curr_format_index = 1; //默认
	curr_frame_index = 1; ///默认值
	////
	iso_ep_addr = EP_ADDR;

	format_count = 1; ///
//	guidFormat = { 0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 } };// MEDIASUBTYPE_RGB24 
//	bitsPerPixel = 24; /// RGB 三原色

	guidFormat = { 0x32595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } }; // MEDIASUBTYPE_YUY2
	bitsPerPixel = 16; //// YUY2 
	/////
	frame_count = 3; ///
	frames[0] = { 640, 480 };
	frames[1] = { 1920, 1080};
	frames[2] = { 1280,  720 };
	//////
	usb_ptr = NULL; ///
	hThread = NULL; ///
	quit = false; ////
	/////
	frame_pos = 0;
	frame_length = 0;
	frame_buffer = NULL;
	frame_flip = 0; 
	frame_delay_msec = 33; // ms 
}
uvc_vcam::~uvc_vcam()
{
	///
	quit = true; 
	if(usb_ptr) virt_usb_unplug(usb_ptr); 
	
	if (hThread) {	
		///
		::WaitForSingleObject(hThread, 5000); 
		::TerminateThread(hThread, 0);
		CloseHandle(hThread);
	}
	////
	if (usb_ptr) {
		virt_usb_close(usb_ptr);
	}
	////
	if (frame_buffer)free(frame_buffer); 
}

void uvc_vcam::fill_dev_desc(unsigned short pid, unsigned int vid)
{
	dev_desc.bLength = 0x12;  // length 18
	dev_desc.bDescriptorType = 0x01;  // type 1
	dev_desc.bcdUSB = 0x0200; // usb 2.0
	dev_desc.bDeviceClass = 0xEF; // 固定为ef ，表示是misc设备
	dev_desc.bDeviceSubClass = 0x02; // common class
	dev_desc.bDeviceProtocol = 0x01; //Interface Association Descriptor
	dev_desc.bMaxPacketSize0 = 0x40; // 64
	dev_desc.idVendor = vid;
	dev_desc.idProduct = pid;
	dev_desc.bcdDevice = 0x00; /// 设备出厂编号
	dev_desc.iManufacturer = 0x01; // 厂商信息序号，固定为1 
	dev_desc.iProduct = 0x02;     //产品信息序号，固定2
	dev_desc.iSerialNumber = 0x00; //无序列号
	dev_desc.bNumConfigurations = 1; //一个配置描述符
}
void uvc_vcam::fill_str_desc(const char* manu_fact, const char* product)
{
	usb_string_descriptor* str = (usb_string_descriptor*)str_buffer;
	str->bLength = 0x04; 
	str->bDescriptorType = 0x03; // string
	str->wData[0] = 0x0409;/// english
	str_desc[0] = str;
	///////
	str = (usb_string_descriptor*)( (char*)str + str->bLength ); //// manu_fact
	str->bDescriptorType = 0x03; 
	::MultiByteToWideChar(CP_ACP, 0, manu_fact, -1, (PWCHAR)str->wData, 256); 
	int len = wcslen((PWCHAR)str->wData) * sizeof(WCHAR);
	str->bLength = 2 + len;
	str_desc[1] = str; ////// iManufacturer
	//////
	str = (usb_string_descriptor*)((char*)str + str->bLength); ////
	str->bDescriptorType = 0x03;
	::MultiByteToWideChar(CP_ACP, 0, product, -1, (PWCHAR)str->wData, 256);
	len = wcslen((PWCHAR)str->wData) * sizeof(WCHAR);
	str->bLength = 2 + len;
	str_desc[2] = str; ////// iProduct

	str_count = 3;
}
void uvc_vcam::fill_cfg_desc()
{
	//配置描述符
	cfg_desc = (usb_config_descriptor*)cfg_buffer; ///
	////
	cfg_desc->bLength = 0x09; /// Length
	cfg_desc->bDescriptorType = 0x02; /// config
	cfg_desc->wTotalLength = 9; /// modified 
	cfg_desc->bNumInterfaces = 2; ///两个接口，一个 video control interface, 一个 video stream interface
	cfg_desc->bConfigurationValue = 1;
	cfg_desc->iConfiguration = 0; ///
	cfg_desc->bmAttributes = 0x80;///电源模式，随便乱填
	cfg_desc->bMaxPower = 0x40; //最大电流，乱填
	
	///// IAD Descriptor 
	usb_interface_assoc_descriptor* iad = (usb_interface_assoc_descriptor*)( (char*)cfg_desc + cfg_desc->bLength );
	iad->bLength = 0x08; ///Length;
	iad->bDescriptorType = 0x0B; /// IAD descriptor
	iad->bFirstInterface = 0; 
	iad->bInterfaceCount = 0x02; /// two interface
	iad->bFunctionClass = 0x0E; /// video
	iad->bFunctionSubClass = 0x03; /// 
	iad->bFunctionProtocol = 0x00;
	iad->iFunction = 0x02; /// string index 'Product' 

	//////
	usb_interface_descriptor* intr = (usb_interface_descriptor*)((char*)iad + iad->bLength);
	intr->bLength = 0x09; /// Length
	intr->bDescriptorType = 0x04; /// interface descriptor
	intr->bInterfaceNumber = 0;
	intr->bAlternateSetting = 0; 
	intr->bNumEndpoints = 0x00; // 0x01; /// (一个) 0个 intrrupt endpoint 做什么用的 ？ ???--------------
	intr->bInterfaceClass = 0x0E; // video 
	intr->bInterfaceSubClass = 0x01; /// video control
	intr->bInterfaceProtocol = 0x00; //
	intr->iInterface = 0x02; /// string index , 'Product'

	/////Video Control Interface Header Descriptor 
	uvc_header_descriptor* vchdr = (uvc_header_descriptor*)((char*)intr + intr->bLength);
	vchdr->bLength = 0x0D; /// length = 12 + 1
	vchdr->bDescriptorType = 0x24; /// video control interface
	vchdr->bDescriptorSubType = 0x01; /// video control header
	vchdr->bcdUVC = 0x0100; /// UVC 1.0
	vchdr->wTotalLength = 0x4F; ////总长度, 79, 到 Video Control Extension Unit Descriptor 结束
	vchdr->dwClockFrequency = 0x01C9C380; // (30 MHz)
	vchdr->bInCollection = 0x01; /// 有1个stream 属于这个video control
	vchdr->baInterfaceNr[0] = 0x01; ///这个 stream 的interfacenumber

	/////Video Control Input Terminal Descriptor 
	uvc_camera_terminal_descriptor* ctd = (uvc_camera_terminal_descriptor*)((char*)vchdr + vchdr->bLength);
	ctd->bLength = 0x12; // Length 18
	ctd->bDescriptorType = 0x24; // video control interface
	ctd->bDescriptorSubType = 0x02; // input termal
	ctd->bTerminalID = 0x01; //// ID 唯一值
	ctd->wTerminalType = 0x0201; //// ITT_CAMERA
	ctd->bAssocTerminal = 0x00; //不关联到 output termal destripor
	ctd->iTerminal = 0; ///string index , 0,不存在描述
	ctd->wObjectiveFocalLengthMax = 0;
	ctd->wObjectiveFocalLengthMin = 0;
	ctd->wOcularFocalLength = 0;
	ctd->bControlSize = 3; ////
	ctd->bmControls[0] = 0x0A; /// ???
	ctd->bmControls[1] = ctd->bmControls[2] = 0x00; ////???

	////Video Control Processing Unit Descriptor
	uvc_processing_unit_descriptor* pud = (uvc_processing_unit_descriptor*)((char*)ctd + ctd->bLength);
	pud->bLength = 0x0B; //Length 11
	pud->bDescriptorType = 0x24; ///video control
	pud->bDescriptorSubType = 0x05; /// processing unit
	pud->bUnitID = 0x02; ///
	pud->bSourceID = 0x01; ////
	pud->wMaxMultiplier = 0x00;
	pud->bControlSize = 0x02; 
	pud->bmControls[0] = 0x7B; ///亮度，对比度等等信息
	pud->bmControls[1] = 0x17; ///
	pud->iProcessing = 0; ///

	/////Video Control Output Terminal Descriptor
	uvc_output_terminal_descriptor* otd = (uvc_output_terminal_descriptor*)((char*)pud + pud->bLength); 
	otd->bLength = 0x09; /// Length 9
	otd->bDescriptorType = 0x24; // video control
	otd->bDescriptorSubType = 0x03; // output terminal
	otd->bTerminalID = 0x03; /// ID 
	otd->wTerminalType = 0x0101; /// TT_STREMAING
	otd->bAssocTerminal = 0x00; //没有关联
	otd->bSourceID = 0x02; ////
	otd->iTerminal = 0; ////

	/////Video Control Extension Unit Descriptor
	uvc_extension_unit_descriptor_1_3* eud = (uvc_extension_unit_descriptor_1_3*)((char*)otd + otd->bLength);
	eud->bLength = 0x1C; /// Length 28
	eud->bDescriptorType = 0x24; /// video control interface
	eud->bDescriptorSubType = 0x06; /// Extension Unit
	eud->bUnitID = 0x04; ///
	// {88F04DA6-F713-454A-B625-3793AE447516}
	static const GUID g =
	{ 0x88f04da6, 0xf713, 0x454a,{ 0xb6, 0x25, 0x37, 0x93, 0xae, 0x44, 0x75, 0x16 } };
	memcpy(eud->guidExtensionCode, &g, sizeof(GUID)); //// 干嘛用的 ？？？
	eud->bNumControls = 0x08; /// ??
	eud->bNrInPins = 0x01; ////
	eud->baSourceID[0] = 0x01; //// sourid 
	eud->bControlSize = 0x03; ///s
	eud->bmControls[0] = 0x00; // vendor-special
	eud->bmControls[1] = 0x00; ///
	eud->bmControls[2] = 0x00; ///
	eud->iExtension = 0x00; 

	/////video control interface -> Endpoint Descriptor
	/////Class-specific VC Interrupt Endpoint Descriptor 

	////video stream interface 
	intr = (usb_interface_descriptor*)((char*)eud + eud->bLength);
	intr->bLength = 0x09; /// Length
	intr->bDescriptorType = 0x04; /// interface
	intr->bInterfaceNumber = 0x01; ////
	intr->bAlternateSetting = 0x00; ///
	intr->bNumEndpoints = 0x00; /// Default Control pipe only
	intr->bInterfaceClass = 0x0E; // video 
	intr->bInterfaceSubClass = 0x02; // Video Streaming
	intr->bInterfaceProtocol = 0x00;
	intr->iInterface = 0x00; ////

	//////VC-Specific VS Video Input Header Descriptor
	uvc_input_header_descriptor* vshdr = (uvc_input_header_descriptor*)((char*)intr + intr->bLength);
	vshdr->bLength = 0x0E; // Length 14
	vshdr->bDescriptorType = 0x24; /// video stream interface
	vshdr->bDescriptorSubType = 0x01; ///input header
	vshdr->bNumFormats = 0x01; /// 一个格式化信息
	vshdr->wTotalLength = 14 + 27 + frame_count* 34 + 6 ; ///  总长度 ,到 VS Color Matching Descriptor Descriptor结束
	vshdr->bEndpointAddress = iso_ep_addr; ///
	vshdr->bmInfo = 0x00; ///
	vshdr->bTerminalLink = 0x03; /// connect Output Terminal Descriptor ...
	vshdr->bStillCaptureMethod = 0x00; ///不提供静态图像
	vshdr->bTriggerSupport = 0x00; //不提供硬件事件，好像对应的就是 控制interface的中断传输端点 ?
	vshdr->bTriggerUsage = 0x00;
	vshdr->bControlSize = 0x01;
	vshdr->bmaControls[0] = 0x00; ///

	/////VS Uncompressed Format Type Descriptor
	uvc_format_uncompressed* fmt = (uvc_format_uncompressed*)((char*)vshdr + vshdr->bLength);
	fmt->bLength = 0x1B; /// Length 27
	fmt->bDescriptorType = 0x24; ///video stream interface
	fmt->bDescriptorSubType = 0x04; /// format type
	fmt->bFormatIndex = 0x01; ///
	fmt->bNumFrameDescriptors = frame_count; ////
	memcpy(fmt->guidFormat, &guidFormat, sizeof(GUID));
	fmt->bBitsPerPixel = bitsPerPixel;
	fmt->bDefaultFrameIndex = 0x01; 
	fmt->bAspectRatioX = 0x00;
	fmt->bAspectRatioY = 0x00;
	fmt->bmInterfaceFlags = 0x00;
	fmt->bCopyProtect = 0x00; 

	///VS Uncompressed Frame Type Descriptor 
	//// frames
	uvc_frame_uncompressed* frame = (uvc_frame_uncompressed*)((char*)fmt + fmt->bLength);
	for (int i = 0; i < frame_count; ++i) {
		////
		frame->bLength = 34; /// Length 34
		frame->bDescriptorType = 0x24; // video stream interface
		frame->bDescriptorSubType = 0x05; /// frame type
		frame->bFrameIndex = i + 1; 
		frame->bmCapabilities = 0x00;
		frame->wWidth = frames[i].width;
		frame->wHeight = frames[i].height;
		frame->dwMinBitRate = 0x00bb8800; // 96KB/s
		frame->dwMaxBitRate = 0x0BB80000; /// 24.5 MB/s
		frame->dwMaxVideoFrameBufferSize = (bitsPerPixel / 8)* frame->wWidth* frame->wHeight; ////
		frame->dwDefaultFrameInterval = 0x00051615; // (33 ms -> 30.00 fps)
		frame->bFrameIntervalType = 0x02; ///
		frame->dwFrameInterval[0] = 0x00051615; // (33 ms -> 30.00 fps)
		frame->dwFrameInterval[1] = 0x000A2C2B; // (66 ms -> 14.99 fps)

		////
		frame = (uvc_frame_uncompressed*)((char*)frame + frame->bLength); ////
	}

	/////VS Color Matching Descriptor Descriptor
	uvc_color_matching_descriptor* cmd = (uvc_color_matching_descriptor*)frame;
	cmd->bLength = 0x06; // Length
	cmd->bDescriptorType = 0x24;
	cmd->bDescriptorSubType = 0x0D; // color matching
	cmd->bColorPrimaries = 0x01; ///(BT.709, sRGB)
	cmd->bTransferCharacteristics = 0x01; //(BT.709)
	cmd->bMatrixCoefficients = 0x04; //(SMPTE 170M)

	///就模拟一个端口就可以了
	//////Interface Descriptor ,描述用于视频数据传输的 iso端口
	intr = (usb_interface_descriptor*)((char*)cmd + cmd->bLength);
	intr->bLength = 0x09;
	intr->bDescriptorType = 0x04; ///
	intr->bInterfaceNumber = 0x01;
	intr->bAlternateSetting = 0x01;
	intr->bNumEndpoints = 0x01; /// 1 endpoint
	intr->bInterfaceClass = 0x0E; //video
	intr->bInterfaceSubClass = 0x02; /// video stream
	intr->bInterfaceProtocol = 0x00;
	intr->iInterface = 0x00; 

	//// Endpoint Descriptor
	usb_endpoint_descriptor* ep = (usb_endpoint_descriptor*)((char*)intr + intr->bLength);
	ep->bLength = 0x07; // Length 7
	ep->bDescriptorType = 0x05; /// endpoint descriptor
	ep->bEndpointAddress = iso_ep_addr;
	ep->bmAttributes = 0x05; /// (TransferType=Isochronous  SyncType=Asynchronous  EndpointType=Data)
	ep->wMaxPacketSize = 0x0C00; //// 
	    //Bits 15..13             : 0x00 (reserved, must be zero)
		//Bits 12..11 : 0x01 (1 additional transactions per microframe->allows 513..1024 byte per packet)
		//Bits 10..0 : 0x400 (1024 bytes per packet)
	ep->bInterval = 0x01; 

	//////
	cfg_desc->wTotalLength = ( ((char*)ep + ep->bLength) - cfg_buffer );

	////////
}

/// thread loop
void uvc_vcam::loop()
{
	while (!quit) {
		///
		usbtx_header_t* hdr = virt_usb_begin(usb_ptr); 
		if (!hdr)continue;
		///
		hdr->result = -1; ////首先设置失败，凡是没被处理的，都失败

		if (hdr->type == 1) { // descriptor
			////
			descriptor(hdr); ///
		}
		else if (hdr->type == 2) { /// class or vender
			///
		//	printf( "class or vender request type=%d,subtype=%d\n", hdr->vendor.type, hdr->vendor.subtype );
			vendor_control(hdr); ///
			////
		}
		else if (hdr->type == 3) { //数据传输
			if (hdr->transfer.type == 3) { //同步传输
			//	printf("iso transfer\n");
				iso_transfer(hdr); ////
			}
			else if (hdr->transfer.type == 1) { //控制传输，处理某些请求描述符直接采用控制传输请求
				control_transfer(hdr);
			}
			else {
				printf("not supported transfer type=%d\n", hdr->transfer.type );
			}
		}
		else if (hdr->type == 4) {//重置设备或者端口，这里假设都成功
			printf("reset device: type=%d\n", hdr->reset.type );
			hdr->result = 0;
		}
		else if (hdr->type == 6) {//FEATURE
			if (hdr->feature.type == 2) { //CLEAR
				hdr->result = 0; ///
			}
		}
		else { // unknow request
			printf("unknow usb request type=%d, subtype=%d\n", hdr->type, hdr->descriptor.type ); ///
		}
		///
	//	if(hdr->result <0) printf("not process usb request type=%d, subtype=%d\n", hdr->type, hdr->descriptor.type); ///
		/////
		virt_usb_end(usb_ptr, hdr); ///
	}
	////
}
void uvc_vcam::descriptor(usbtx_header_t* hdr)
{
	if (hdr->descriptor.type == 1) { //获取描述符 ,hdr->descriptor.is_read ==TRUE 
		if (hdr->descriptor.subtype == 1) { //设备描述符
			hdr->result = 0;
			memcpy(hdr->data, &this->dev_desc, 18);
			hdr->data_length = 18; ///
		}
		else if (hdr->descriptor.subtype == 2) { //配置描述符
			if (hdr->data_length < 0x09) {
				printf("falt err cfg_desc length not match\n");
				hdr->result = -1;
			}
			else {
				hdr->result = 0;
				memcpy(hdr->data, cfg_desc, min(hdr->data_length, cfg_desc->wTotalLength)); ////
			}
			//////
		}
		else if (hdr->descriptor.subtype == 3) { //获取字符串
												 ////
			if (hdr->descriptor.index >= str_count || hdr->descriptor.index < 0) { //
				hdr->result = -1;
			}
			else {
				hdr->result = 0; ///
				hdr->data_length = str_desc[hdr->descriptor.index]->bLength; //这里没对原来的data_length判断，认为都有足够缓存来接纳字符串
				memcpy(hdr->data, str_desc[hdr->descriptor.index], hdr->data_length);
			}
			//////////
		}
		/////////
	}
	else if (hdr->descriptor.type == 2) { //设置配置或者接口描述符
		if (hdr->descriptor.subtype == 1) { // set config
			hdr->result = 0;
			if (hdr->descriptor.index == -1 && hdr->descriptor.value == -1) this->curr_config = -1;
			else this->curr_config = hdr->descriptor.value; ///选择的配置描述符
		}
		else if (hdr->descriptor.subtype == 2) { // set interface
			hdr->result = 0; ////
			this->curr_interface = hdr->descriptor.index;
			this->curr_alt_setting = hdr->descriptor.value;
		}
		//////
	}
	else {
		printf("unknow descriptor request type=%d, subtype=%d\n", hdr->descriptor.type, hdr->descriptor.subtype);
	}
	/////
}
void uvc_vcam::vendor_control(usbtx_header_t* hdr)
{
	printf("vender type=%d,subtype=%d, request=0x%X, index=0x%X, value=0x%X,LEN=%d\n", 
		hdr->vendor.type, hdr->vendor.subtype, hdr->vendor.request, hdr->vendor.index, hdr->vendor.value, hdr->data_length );
	////
	switch (hdr->vendor.request)
	{
/*	case UVC_GET_INFO:
		if(hdr->data_length ==1){ //被设置 为 1
			hdr->result = 0; 
			hdr->data[0] = 0x01; /// GET / SET 
			////
		}
		break; */
	case UVC_GET_DEF:
		hdr->result = 0; memset(hdr->data, 0, hdr->data_length); ///
		break;

	case UVC_GET_CUR:
		{
			if ( LOBYTE(hdr->vendor.index) == 1 ) { /// 视频流接口
				if (hdr->data_length < 26) { printf("UVC 1.0  GET_CUR must use 26 bytes \n"); break; }
				////
				uvc_streaming_control* sc = (uvc_streaming_control*)hdr->data; 
				sc->bmHint = 0x01; /// supported dwFrameInterval
				sc->bFormatIndex = curr_format_index;
				sc->bFrameIndex = curr_frame_index;
				sc->dwFrameInterval = 0x00051615; // (33 ms -> 30.00 fps) 
				sc->wKeyFrameRate = 0;
				sc->wPFrameRate = 0;
				sc->wCompQuality = 0;
				sc->wCompWindowSize = 0;
				sc->wDelay = 0x0A; //// 停留 毫秒 ms ???
				sc->dwMaxVideoFrameSize = frames[curr_frame_index - 1].width* frames[curr_frame_index - 1].height* (bitsPerPixel/8); ////
				sc->dwMaxPayloadTransferSize = 1024; //// ???? 
				//////
				hdr->result = 0; ////
			}
		}

	case UVC_SET_CUR:
		{
			if (LOBYTE(hdr->vendor.index) == 1) { /// 视频流接口, 设置
				if (hdr->data_length < 26) { printf("UVC 1.0  SET_CUR must use 26 bytes \n"); break; }
				/////
				uvc_streaming_control* sc = (uvc_streaming_control*)hdr->data;
				if (sc->bFormatIndex < 1 || sc->bFormatIndex > format_count) break;
				if (sc->bFrameIndex < 1 || sc->bFrameIndex > frame_count) break;

				curr_format_index = sc->bFormatIndex;
				curr_frame_index = sc->bFrameIndex;

				hdr->result = 0; /////
			}
		}
		break;

	case UVC_GET_MIN:
		{
			if (LOBYTE(hdr->vendor.index) == 1) { /// 视频流接口
				if (hdr->data_length < 26) { printf("UVC 1.0  GET_MIN must use 26 bytes \n"); break; }
				////
				uvc_streaming_control* sc = (uvc_streaming_control*)hdr->data;
				memset(sc, 0, 26); 
				sc->bFormatIndex = 1;
				sc->bFrameIndex = 1;

				hdr->result = 0; ////
			}
		}
		break;

	case UVC_GET_MAX:
		{
			if (LOBYTE(hdr->vendor.index) == 1) { /// 视频流接口
				if (hdr->data_length < 26) { printf("UVC 1.0  GET_MAX must use 26 bytes \n"); break; }
				////
				uvc_streaming_control* sc = (uvc_streaming_control*)hdr->data;
				memset(sc, 0, 26);
				sc->bFormatIndex = format_count;
				sc->bFrameIndex = frame_count;

				hdr->result = 0; ////
			}
			/////
		}
		break;
	}
}
void uvc_vcam::control_transfer(usbtx_header_t* hdr)
{
	unsigned char* code = hdr->transfer.setup_packet;
	unsigned short value = *(unsigned short*)&code[2];
	unsigned short index = *(unsigned short*)&code[4];

	///驱动接口没处理好，由于微软把某些 控制传输抽取出来作为另外的URB_FUNCTION** 处理，造成两套接口。。。。
	usbtx_header_t bak_hdr;
	memcpy(&bak_hdr.transfer, &hdr->transfer, sizeof(hdr->transfer)); ///保存transfer内容，处理完好恢复

	if (code[1] == 6) { // GET_DESCRIPTOR 获取描述符
		hdr->type = 1;
		hdr->descriptor.type = 1; ////
		hdr->descriptor.subtype = HIBYTE(value); 
		hdr->descriptor.index = index;
		hdr->descriptor.is_read = 1;
		hdr->descriptor.value = 0;
		
		descriptor(hdr); ////
	}
	else if (code[1] == 8) { // GET_CONFIGURATION ///获取配置
		printf("control transfer not supoorted GET_CONFIG\n");
	}
	else if (code[1] == 11) { // SET_INTERFACE 设置接口
		int intfNum = LOBYTE(index); ///
		int altSet = LOBYTE(value); ///
		hdr->result = 0;
		this->curr_alt_setting = altSet;
		this->curr_interface = intfNum; 
		printf("SET_INTERFACE: intf_num=%d, altset=%d\n", intfNum, altSet );
	}
	else if (code[1] == 1) { /// CLEAR_FEATURE
		hdr->result = 0;
		printf("control transfer CLEAR_FEAUTURE \n");
	}
	else if (code[0] == 0xA1) { // CLASS 请求
		hdr->type = 2; ///
		hdr->vendor.type = 1; // CALSS
		hdr->vendor.is_read = 1;
		hdr->vendor.request = code[1];
		hdr->vendor.index =index;
		hdr->vendor.value = value;

		vendor_control(hdr); ///
	}
	else {
		printf("control transfer not supported request=0x%X\n", code[1]);
	}

	/////////恢复
	hdr->type = 3; 
	memcpy(&hdr->transfer, &bak_hdr.transfer, sizeof(hdr->transfer));
}
void uvc_vcam::iso_transfer(usbtx_header_t* hdr)
{
	iso_packet_hdr* iso_hdr = (iso_packet_hdr*)hdr->data; 
	int HDR_LEN = ISO_PACKET_HDR_SIZE + iso_hdr->number_packets * sizeof(iso_packet); ///
	char* buffer = hdr->data + HDR_LEN;///
	int   buf_len = hdr->data_length - HDR_LEN ; ///
	if (buf_len <= 12 ) { //如果长度小于playerheader头
		printf("falt error\n");
		return;
	}
	////
	int frmlen = (bitsPerPixel / 8)*frames[curr_frame_index - 1].width*frames[curr_frame_index - 1].height; ////

	if (frame_length != frmlen || !frame_buffer) {
		frame_length = frmlen; 
		frame_pos = 0;
		if (frame_buffer)free(frame_buffer);
		frame_buffer = (char*)malloc(frame_length); ////
	}
	///
	if (frame_pos <= 0 ) { //一个帧的开始
		///
		Sleep(frame_delay_msec); ///前一个帧停留一段时间
		////
		frame_t frame;
		frame.buffer = frame_buffer; 
		frame.length = frame_length;
		frame.delay_msec = frame_delay_msec; ///
		frame.width = frames[curr_frame_index - 1].width;
		frame.height = frames[curr_frame_index - 1].height;
		frame.param = cbk_param; ///

		int r = frame_callback( &frame ); 
		if (r < 0) {
			return;
		}
		//////
		frame_delay_msec = frame.delay_msec; ////
	}
	
	/////
	int ret = 0;

	iso_hdr->error_count = 0; //no error 

	for (int i = 0; i < iso_hdr->number_packets - 1 ; ++i ) {
		iso_packet* f_pkt = &iso_hdr->packets[i]; 
		iso_packet* n_pkt = &iso_hdr->packets[i + 1]; 
		int dt = n_pkt->offset - f_pkt->offset;
		int r = video_encode( (unsigned char*)buffer + f_pkt->offset, dt); 

		if (r <= 0) continue;
		ret += r; 
		f_pkt->length = r; 
		f_pkt->status = 0; //success
		
		if (ret >= buf_len) break;

		if (frame_pos <= 0) { //帧结束
			break;
		}
		////
		if (i == iso_hdr->number_packets - 2 && ret < buf_len ) {
			dt = n_pkt->offset - f_pkt->offset;
			dt = min(buf_len - ret, dt);
			
			r = video_encode((unsigned char*)buffer + n_pkt->offset, dt); 
			if (r <= 0)continue;
			ret += r;
			n_pkt->length = r; 
			n_pkt->status = 0; ////
		}
		/////
	}

	hdr->result = 0; /// success 
	hdr->data_length = ret + HDR_LEN; ////
	
	/////
//	printf("iso_pkt_num=%d, data_len=%d\n", iso_hdr->number_packets, buf_len );
}

int uvc_vcam::video_encode(unsigned char* vid_buf, int vid_len)
{
	int VID_HDR_LEN = 2; ////
	
	unsigned char* vid_data = vid_buf + VID_HDR_LEN;
	int data_len = vid_len - VID_HDR_LEN;

	unsigned char* vid_hdr = vid_buf;
	
	if (data_len <= 0) return -1; //数据长度不够

	memset(vid_hdr, 0, VID_HDR_LEN); ////
	vid_hdr[0] = VID_HDR_LEN; 

	vid_hdr[1] |= frame_flip; ////
	vid_hdr[1] |= UVC_STREAM_EOH; ///

	int len = min( frame_length - frame_pos, data_len); ///

	memcpy(vid_data, frame_buffer + frame_pos, len); ////

	frame_pos += len; ////
	//////
	if (frame_pos >= frame_length) { ///帧结束
		vid_hdr[1] |= UVC_STREAM_EOF ;
		////
		frame_flip = !frame_flip; //翻转
		frame_pos = 0; //从新开始
		////
	}
	//////
	return VID_HDR_LEN + len; ////
}

int uvc_vcam::create(uvc_vcam_t* uvc)
{
	cbk_param = uvc->param;
	this->frame_callback = uvc->frame_callback; ///

	fill_dev_desc(uvc->pid, uvc->vid);
	fill_str_desc(uvc->manu_fact, uvc->product);
	fill_cfg_desc();
	/////
	char devid[256]; 
	sprintf(devid, "usb\\vid_%.4x&pid_%.4x", uvc->vid, uvc->pid); ///
	usb_ptr = virt_usb_open();
	if (!usb_ptr) {
		printf("Can Not virt_usb_open, make sure load driver.\n");
		return -1; 
	}
	int r = virt_usb_plugin(usb_ptr,  devid, devid, "USB\\DevClass_00&SubClass_00\nUSB\\DevClass_00\nUSB\\COMPOSITE"); ///
	if (r < 0) {
		virt_usb_close(usb_ptr);
		usb_ptr = NULL;
		printf("Can Not Plug USB .\n");
		return -1;
	}
	///
	DWORD tid;
	hThread = CreateThread(NULL, 0, __thread, this, 0, &tid); ////
	/////
	return 0;
}

void* vcam_create(uvc_vcam_t* uvc)
{
	uvc_vcam* vcam = new uvc_vcam;
	int r = vcam->create(uvc);
	if (r < 0) {
		delete vcam;
		return NULL;
	}
	return vcam;
}
void vcam_destroy(void* handle)
{
	uvc_vcam* vcam = (uvc_vcam*)handle;
	if (!vcam)return;

	delete vcam;
}

/////


