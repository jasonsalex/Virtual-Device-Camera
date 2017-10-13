//////Fanxiushu 2016-10-06

#include <Windows.h>
#include <stdio.h>
#include "uvc_vcam.h"

//// RGB -> YUV 从网络查询的算法
void rgb24_yuy2(void* rgb, void* yuy2, int width, int height)
{
	int R1, G1, B1, R2, G2, B2, Y1, U1, Y2, V1;
	unsigned char* pRGBData = (unsigned char *)rgb;
	unsigned char* pYUVData = (unsigned char *)yuy2;

	for (int i = 0; i<height; ++i)
	{
		for (int j = 0; j<width / 2; ++j)
		{
			B1 = *(pRGBData + (height - i - 1)*width * 3 + j * 6);
			G1 = *(pRGBData + (height - i - 1)*width * 3 + j * 6 + 1);
			R1 = *(pRGBData + (height - i - 1)*width * 3 + j * 6 + 2);
			B2 = *(pRGBData + (height - i - 1)*width * 3 + j * 6 + 3);
			G2 = *(pRGBData + (height - i - 1)*width * 3 + j * 6 + 4);
			R2 = *(pRGBData + (height - i - 1)*width * 3 + j * 6 + 5);
			
			Y1 = ((66 * R1 + 129 * G1 + 25 * B1 + 128) >> 8) + 16; 
			U1 = (((-38 * R1 - 74 * G1 + 112 * B1 + 128) >> 8) + ((-38 * R2 - 74 * G2 + 112 * B2 + 128) >> 8)) / 2 + 128;
			Y2 = ((66 * R2 + 129 * G2 + 25 * B2 + 128) >> 8) + 16;
			V1 = (((112 * R1 - 94 * G1 - 18 * B1 + 128) >> 8) + ((112 * R2 - 94 * G2 - 18 * B2 + 128) >> 8)) / 2 + 128;

			*(pYUVData + i*width * 2 + j * 4) =     max(min( Y1, 255), 0);
			*(pYUVData + i*width * 2 + j * 4 + 1) = max(min( U1, 255), 0);
			*(pYUVData + i*width * 2 + j * 4 + 2) = max(min( Y2, 255), 0);
			*(pYUVData + i*width * 2 + j * 4 + 3) = max(min( V1, 255), 0);
		}
	}
}

////////////////////
struct vcam_param
{
	HBITMAP hbmp;
	HDC hdc;
	void* rgb_data;

	int width;
	int height; 
	const char* text;
	int i_color;
	int           clr_flip;
	int           i_size; 
	int           sz_flip;
};
int create_dib(vcam_param* p, int w, int h)
{
	if (p->width == w && p->height == h && p->hbmp ) return 0;
	////
	if (p->hbmp)DeleteObject(p->hbmp);
	if (p->hdc)DeleteDC(p->hdc);
	p->hbmp = 0; p->hdc = 0;
	p->hdc = CreateCompatibleDC(NULL);

	BITMAPINFOHEADER bi; memset(&bi, 0, sizeof(bi));
	bi.biSize = sizeof(bi);
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24; //RGB
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0; 


	p->hbmp = CreateDIBSection(p->hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &p->rgb_data, NULL, 0);
	if (!p->hbmp) {
		p->rgb_data = 0; 
		printf("CreateDIBSection err=%d\n", GetLastError() );
		return -1;
	}
	SelectObject(p->hdc, p->hbmp); ///
	////
	p->width = w;
	p->height = h;

	p->clr_flip = 1;
	p->i_color = 0;

	p->i_size = 20;
	p->sz_flip = 2; ///

	return 0;
}
void draw_text(vcam_param* p)
{
	if (!p->hbmp)return;
	////
	int len = p->width*p->height * 3; //
	memset(p->rgb_data, p->i_color, len); /// 背景色渐变
	p->i_color += p->clr_flip;
	if (p->i_color <=0 || p->i_color >=245 ) p->clr_flip = -p->clr_flip; ///
	
	////
	p->i_size += p->sz_flip;
	if (p->i_size <= 10 || p->i_size >= 60 ) p->sz_flip = -p->sz_flip;

	HFONT font = CreateFont(p->i_size, p->i_size*3/4, 1, 0, 800, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, NULL);
	HFONT old = (HFONT)SelectObject(p->hdc, font);
	RECT rc = {0,0,p->width, p->height};
	SetBkMode(p->hdc, TRANSPARENT);
	SetTextColor(p->hdc, RGB(255,255-p->i_color%255,(p->i_color/3+20)%255 ));
	DrawText(p->hdc, p->text, strlen(p->text), &rc, DT_CENTER | DT_VCENTER|DT_SINGLELINE );
	SelectObject(p->hdc, old);
	DeleteObject(font); 
}

int frame_callback(frame_t* frame)
{
	vcam_param* p = (vcam_param*)frame->param;
	////
//	memset(frame->buffer, 0xee, frame->length); ////
//	printf("frame_len=%d,w=%d,h=%d\n", frame->length, frame->width, frame->height);

	//create_dib(p, frame->width, frame->height); ///

	//draw_text(p);

	frame->buffer =(char*) p->rgb_data;

	//if(p->rgb_data) rgb24_yuy2(p->rgb_data, frame->buffer, frame->width, frame->height );

	//frame->delay_msec = 33; ///每帧的停留时间， 毫秒， 

	return 0;
}

int main(int argc, char** argv)
{
	uvc_vcam_t uvc1;
	vcam_param p1; memset(&p1, 0, sizeof(p1)); p1.text = "Fanxiushu; Virtual USB Camera";
	uvc1.pid = 0xcc10; uvc1.vid = 0xbb10; //乱造， 不能与真实的重复
	uvc1.manu_fact = "Fanxiushu"; uvc1.product = "test Virtual USB Camera";
	uvc1.frame_callback = frame_callback;
	uvc1.param = &p1;

	void* vcam1 = vcam_create(&uvc1); ///
	///
	uvc_vcam_t uvc2; 
	vcam_param p2; memset(&p2, 0, sizeof(p2)); p2.text = "Fanxiushu 2; Virtual USB Camera 2";
	uvc2.pid = 0xcc11; uvc2.vid = 0xbb11;//乱造, 不能与真实的重复
	uvc2.manu_fact = "Fanxiushu 2"; uvc2.product = "test Virtual USB Camera 2";
	uvc2.frame_callback = frame_callback; ///
	uvc2.param = &p2; ///

	void* vcam2 = vcam_create(&uvc2); ////
	/////
	printf("any key exit\n"); getchar();

	vcam_destroy(vcam1);
	vcam_destroy(vcam2);
	if (p1.hbmp)DeleteObject(p1.hbmp); if (p1.hdc)DeleteDC(p1.hdc);
	if (p2.hbmp)DeleteObject(p2.hbmp); if (p2.hdc)DeleteDC(p2.hdc);
	////
	return 0;
}

