/// By Fanxiushu 2016-04-27

#pragma once

#pragma pack(1)

///ISO传输时候附加的包信息,其实就是从IOCTL.H复制的iso_packet_t等结构
struct iso_packet
{
	unsigned int offset;
	unsigned int length;
	unsigned int status;
};
struct iso_packet_hdr
{
	unsigned int start_frame;  ///开始帧
	unsigned int flags;       //主要用于判断是否设置 START_ISO_TRANSFER_ASAP
	unsigned int error_count;  ///错误包个数
	unsigned int number_packets; // iSO 包个数
								 ////
	iso_packet packets[1]; ////
};
#define ISO_PACKET_HDR_SIZE    (4*sizeof(unsigned int))

///////////
struct usbtx_header_t
{
	int        type;  // 1 获取描述符, 2 vendor or class ， 3 传输数据,  4 重置, 5 获取状态, 6 操作feature
	int        result;
	int        data_length; ///对应写入USB设备的的操作，操作成功后，这个值代表实际写入多少
	int        reserved; //保留
	/////
	union{
		///
		struct {
			int          type;     // 1 获取或设置设备描述符， 2 设置配置描述符, 3 获取或设置接口描述符， 4 获取或设置端口描述符, 5 获取微软信息URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR
			int          subtype;  // (type=1,3,4) 1 获取设备描述符， 2 获取配置描述符， 3 获取字符串;;;;; (type=2) 1设置config(index=-1 & value=-1 unconfigure)， 2 设置 interface
			int          is_read;  // (type=1,3,4) is_read为TRUE获取描述符，FALSE 设置描述符
			int          index;    // 序号, (type=5 LOBYTE(index)=interfaceNumber, HIBYTE(index)=Recipient)
			int          value;    // 值, 获取string时定义成language_id, (type=5 为 MS_FeatureDescriptorIndex)

		}descriptor;
		////////
		struct{
			int          type;    //1 CLASS请求， 2 VENDOR请求
			int          subtype; //1 device; 2 interface ; 3 endpoint; 4 other
			int          is_read; //是从设备读，还是写入设备
			int          request;
			int          index;
			int          value;
		}vendor;
		////
		struct {
			int           type; // 1 控制传输,  2 中断或批量传输， 3 同步传输
			int           ep_address; //端口位置   如果 (ep_address &0x80) 则是读，否则写;  控制传输时候，如果为0表示使用默认端口
			int           is_read;    //是从设备读，还是写入设备
			union {
				struct { //中断，批量，同步传输
					int           number_packets; //同步传输时候，包个数,如果为0，则组合到一起传输，>0则在头后面跟iso_packet_hdr_t结构，大小为 ISO_PACKET_HDR_SIZE + number_packets*sizeof(iso_packet_t)
					int           reserved0;      ////
					char          is_split;      ///中断批量传输，或同步传输是否拆分成多块， 
					char          reserved[3];   ///
				};
				struct { //控制传输
					unsigned char setup_packet[8]; /////控制传输时候，发送的8个字节的控制码
					unsigned int  timeout;         /////URB_FUNCTION_CONTROL_TRANSFER_EX 对应的超时值，单位毫秒,为0 表示不使用超时，等同于URB_FUNCTION_CONTROL_TRANSFER
				};
			};

		}transfer;
		////////
		struct {
			int           type; /// 1 IOCTL_INTERNAL_USB_RESET_PORT重置设备； 2 IOCTL_INTERNAL_USB_CYCLE_PORT 重置设备； 3 重置端口URB_FUNCTION_RESET_PIPE; 4 中断端口 URB_FUNCTION_ABORT_PIPE
			int           ep_address;
		}reset;
		/////
		struct {
			int           type; /// 1 device; 2 interface ; 3 endpoint; 4 other status; 5 获取当前配置描述符；6 根据interface获取当前接口的alterantesetting; 7 获取current frame number
			int           index; ///
		}status;
		//////
		struct {
			int           type;     //// 1 SET请求， 2 CLEAR请求
			int           subtype;  /// 1 device; 2 interface ; 3 endpoint; 4 other
			int           index;    ///
			int           value;    ///
		}feature;
		////////
	};
	////////////

	/////
	char  data[0]; ////占位符
};

#pragma pack()

//// function
void* virt_usb_open();
void virt_usb_close(void* handle);

//插入和移除USB设备
int virt_usb_plugin(void* handle, const char* dev_id, const char* hw_ids, const char* comp_ids);
int virt_usb_unplug(void* handle);
int virt_usb_replug(void* handle); //模拟插拔

//获取USB设备数据
usbtx_header_t* virt_usb_begin(void* handle);
int virt_usb_end(void* handle, usbtx_header_t* header);

