/// By fanxiushu 2016-04-06

#pragma once


// {ED3FB623-0C23-48B8-880A-9D1A07263FFD}
DEFINE_GUID(GUID_XUSB_VIRT_INTERFACE,
	0xed3fb623, 0xc23, 0x48b8, 0x88, 0xa, 0x9d, 0x1a, 0x7, 0x26, 0x3f, 0xfd);


#define IOCTL_PDO_ADD \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PDO_REMOVE \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define IOCTL_BEGIN_TRANS_USB_DATA \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_END_TRANS_USB_DATA \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_NEITHER, FILE_ANY_ACCESS)

/////
#define HW_IDS_COUNT    260

struct ioctl_pdo_create_t
{
    //用于获取URB 通知, 采用 ULONGLONG ,兼容32和64位程序
	ULONGLONG     hSemaphore;
	ULONGLONG     hRemoveEvent; ///设备发生移除会设置这个值
	LONG          BusAddress; ///这只是个建议值，驱动会根据这个值，搜索没再使用的值,如果是0，则忽略

	///MULTI SZ 类型
	WCHAR hardware_ids[HW_IDS_COUNT];
	///MULTI SZ
	WCHAR compatible_ids[HW_IDS_COUNT];
    	
};

#pragma pack(1)

///ISO传输时候附加的包信息
struct iso_packet_t
{
	unsigned int offset;
	unsigned int length;
	unsigned int status; 
};
struct iso_packet_hdr_t
{
	unsigned int start_frame;  ///开始帧
	unsigned int flags;       //主要用于判断是否设置 START_ISO_TRANSFER_ASAP
	unsigned int error_count;  ///错误包个数
	unsigned int number_packets; // iSO 包个数
	////
	iso_packet_t packets[1]; ////
};
#define ISO_PACKET_HDR_SIZE    (4*sizeof(unsigned int))

struct ioctl_usbtx_header_t
{
	ULONGLONG        inter_handle;        //  是等待处理的文件IRP 指针
	LONG             inter_seqno;         //  每个IRP的序列号，由驱动产生，和inter_handle一起用来保证请求包的唯一性验证  
	LONG             data_length;         //  数据的长度; 如果是读设备，则读取的字节数； 如果是写数据到设备，写入前是需要写入的字节数，写入成功后，实际写入的字节数,ISO 传输会包括 iso_packet_hdr_t结构大小
	LONG             result;              //  返回是否成功
	LONG             reserved1;           //  保留
	////


	int        type;  // 1 获取描述符, 2 vendor or class ， 3 传输数据,  4 重置, 5 获取状态, 6 操作feature
	int        reserved[ 3 ]; //保留
	/////
	union{
		///
		struct{
			int          type;     // 1 获取或设置设备描述符， 2 设置配置描述符, 3 获取或设置接口描述符， 4 获取或设置端口描述符
			int          subtype;  // (type=1,3,4) 1 获取设备描述符， 2 获取配置描述符， 3 获取字符串;;;;; (type=2) 1设置config(index=-1 & value=-1 unconfigure)， 2 设置 interface
			int          is_read;  // (type=1,3,4) is_read为TRUE获取描述符，FALSE 设置描述符
			int          index;    // 序号
			int          value;    // 值, 获取string时定义成language_id

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
			union{
				int               number_packets; //同步传输时候，包个数,如果为0，则组合到一起传输，>0则在头后面跟iso_packet_hdr_t结构，大小为 ISO_PACKET_HDR_SIZE + number_packets*sizeof(iso_packet_t)
				struct{
					unsigned char setup_packet[8]; /////控制传输时候，发送的8个字节的控制码
				};
			};
			char          is_split; ///中断批量传输，或同步传输是否拆分成多块， 
			char          reserved[3]; ///
		}transfer;
		////////
		struct {
			int           type; /// 1 IOCTL_INTERNAL_USB_RESET_PORT重置设备； 2 IOCTL_INTERNAL_USB_CYCLE_PORT 重置设备； 3 重置端口URB_FUNCTION_RESET_PIPE; 4 中断端口 URB_FUNCTION_ABORT_PIPE
			int           ep_address;
		}reset;
		/////
		struct {
			int           type; /// 1 device; 2 interface ; 3 endpoint; 4 other status; 5 获取当前配置描述符；6 根据interface获取当前接口的alterantesetting
			int           index; ///
		}status;
		//////
		struct {
			int           type;     //// 1 SET请求， 2 CLEAR请求
			int           subtype;  ///  1 device; 2 interface ; 3 endpoint; 4 other
			int           index;    ///
			int           value;    ///
		}feature;
		////////
	};
	////////////

};

#pragma pack()


