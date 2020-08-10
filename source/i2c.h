/*! \file i2c.h */
#pragma once
//*******************************************************************
//Class for accessing i2c Bus
//*******************************************************************
#include <linux/types.h>

class cNeoI2c
{
	bool error_flag;
	__u8 mu8slave_address;
	char mi8devicefile[64];
	//char mi8ErrorMessage[256];
    int fd;
    int errorMsg(const char *_message);
    int open_fd();
    void close_fd();
	void init(const char *_deviceFile, __u8 _u8address);
public:
	cNeoI2c();
	cNeoI2c(__u8 _u8address);
	cNeoI2c(const char *DeviceFile, __u8 _u8address);
	virtual ~cNeoI2c();
	virtual bool fail();
	virtual int setAddress(__u8 _u8address);
	virtual __u8 getAddress();
//	virtual const char *getErrorMessage();
	virtual int setDevicefile(const char *_filename);
	virtual int receive(void *_pRxBuf, __u32 _u32length);
	virtual int receive(__u8 _u8registerAddress, void *_pRxBuf, __u32 _u32length);
	virtual int send(void *_pTxBuf, __u32 _u32length);
	virtual int send(__u8 _u8registerAddress, void *_pTxBuf, __u32 _u32length);
	virtual int send(__u8 _u8value);
};
