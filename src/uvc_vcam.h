//// By Fanxiushu 2016-10-04
/////
#pragma once

struct frame_t
{
	char* buffer;
	int   length;
	int   width;
	int   height;
	int   delay_msec; ///Õ£¡Ù ±º‰
	////
	void* param;  ///
};

typedef int (*FRAME_CALLBACK)(frame_t* frame);

struct uvc_vcam_t
{
	int pid;
	int vid;
	const char* manu_fact;
	const char* product; 
	FRAME_CALLBACK  frame_callback; ///
	void* param; 
};

//// function
void* vcam_create(uvc_vcam_t* uvc);

void vcam_destroy(void* handle);

