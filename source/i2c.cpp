#include "i2c.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <pthread.h>

#include <errno.h>


#define NEO_I2C_WAIT()      usleep(7000)
//*******************************************************************
//Class for accessing i2c Bus
//*******************************************************************

pthread_mutex_t     gI2cmutexBus = PTHREAD_MUTEX_INITIALIZER;

#define DEFAULTDEVICE "/dev/i2c-1"

cNeoI2c::cNeoI2c()
{
	init(DEFAULTDEVICE, -1);
}

cNeoI2c::cNeoI2c(__u8 _u8address)
{
	init(DEFAULTDEVICE, _u8address);
}


cNeoI2c::cNeoI2c(const char *_devicefile, __u8 _u8address)
{
	init(_devicefile, _u8address);
}

cNeoI2c::~cNeoI2c()
{
	close_fd();
}

void cNeoI2c::init(const char *_devicefile, __u8 _u8address)
{
	strcpy(mi8devicefile, _devicefile);
    mu8slave_address = _u8address;
	error_flag=false;
    fd = 0;
    //strcpy(mi8ErrorMessage, "OK");
}

int cNeoI2c::errorMsg(const char *_message)
{
//	strcpy(mi8ErrorMessage, _message);
	fprintf(stderr, "cNeoI2c: %s\n", _message);
	error_flag=true;
	close_fd();
	return -1;
}


void cNeoI2c::close_fd()
{
	if (fd)
	{
		close(fd);
		fd = 0;
	}
}


int cNeoI2c::open_fd()
{
    char msg[256];
	error_flag = false;
	close_fd();
	if (mu8slave_address == -1)
		return errorMsg("ERROR slave address is not set\n");

	if ((fd = open(mi8devicefile, O_RDWR)) < 0)
    {
        sprintf(msg, "ERROR opening: %s\n", mi8devicefile);
        return errorMsg(msg);
    }
	if (ioctl(fd, I2C_SLAVE, mu8slave_address) < 0)
    {
        sprintf(msg, "ERROR address: 0x%02x\n", mu8slave_address);
        return errorMsg(msg);
    }
	return 0;
}

bool cNeoI2c::fail()
{
	return error_flag;
}


int cNeoI2c::setAddress(__u8 _u8address)
{
	mu8slave_address = _u8address;
	return open_fd();
}

__u8 cNeoI2c::getAddress()
{
	return mu8slave_address;
}


//const char *cNeoI2c::getErrorMessage()
//{
//	return mi8ErrorMessage;
//}

int cNeoI2c::setDevicefile(const char *filename)
{
    strcpy(mi8devicefile, filename);
	return open_fd();
}

int cNeoI2c::receive(void *_pRxBuf, __u32 _u32length)
{
	if (_pRxBuf == NULL)
		return errorMsg("Receive method received a null TxBuf pointer.\n");
	if (_u32length < 1)
		return errorMsg("Receive method received an invalid buffer length.\n");

	if (!fd)
		 if (open_fd() == -1)
			  return -1;

	error_flag=false;

    pthread_mutex_lock(&gI2cmutexBus);
	if (read(fd, _pRxBuf, _u32length) != (ssize_t)_u32length)
    {
        char msg[256];
        sprintf(msg, "i2c read error! Address: 0x%02x dev file: %s\n", mu8slave_address, mi8devicefile);
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg(msg);
    }
    NEO_I2C_WAIT();
    pthread_mutex_unlock(&gI2cmutexBus);
	return 1;
}

int cNeoI2c::receive(__u8 _u8registerAddress, void *_pRxBuf, __u32 _u32length)
{

	if (_pRxBuf == NULL)
		return errorMsg("Receive method received a null TxBuf pointer.\n");
	if (_u32length < 1)
		return errorMsg("Receive method received an invalid buffer length.\n");

	if (!fd)
		if (open_fd() == -1)
			  return -1;

	error_flag=false;

    pthread_mutex_lock(&gI2cmutexBus);
	if (write(fd, &_u8registerAddress, 1) != 1)
    {
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg("A i2c write error!\n");
    }

	if (read(fd, _pRxBuf, _u32length) != (ssize_t)_u32length)
    {
        char msg[256];
        sprintf(msg, "i2c read error! Address: 0x%02x dev file: %s\n", mu8slave_address, mi8devicefile );
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg(msg);
    }
    NEO_I2C_WAIT();
    pthread_mutex_unlock(&gI2cmutexBus);
	return 1;
}


int cNeoI2c::send(void *_pTxBuf, __u32 _u32length)
{

	if (_pTxBuf == NULL)
		return errorMsg("Send method received a null TxBuf pointer.\n");
	if (_u32length < 1)
		return errorMsg("Send method received an invalid buffer length.\n");

	if (!fd)
		if (open_fd() == -1)
			  return -1;

	error_flag=false;

    pthread_mutex_lock(&gI2cmutexBus);
	if(write(fd, _pTxBuf, _u32length) != (ssize_t)_u32length)
    {
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg("B i2c write error!\n");
    }
    NEO_I2C_WAIT();
    pthread_mutex_unlock(&gI2cmutexBus);
	return 1;
}


int cNeoI2c::send(__u8 _u8registerAddress, void *_pTxBuf, __u32 _u32length)
{
	__u32 i;

    if (_pTxBuf == 0)
		return errorMsg("Send method received a null TxBuf pointer.\n");
	if (_u32length < 1)
		return errorMsg("Send method received an invalid buffer length.\n");

	if (!fd)
		if (open_fd() == -1)
			  return -1;

	__u8 *data = new __u8[_u32length + 1];
	if(data == NULL)
        return errorMsg("Send method out of memory error.\n");

	data[0]=_u8registerAddress;

	for ( i = 0; i < _u32length ; i++ )
	{
		data[i+1] = ((__u8 *)_pTxBuf)[i];
	}


	error_flag=false;

    pthread_mutex_lock(&gI2cmutexBus);
	if(write(fd, data, _u32length+1) != (ssize_t)_u32length+1)
    {
        delete []data;
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg("C i2c write error!\n");
    }
    NEO_I2C_WAIT();
    pthread_mutex_unlock(&gI2cmutexBus);
    delete []data;
	return 1;
}

int cNeoI2c::send(__u8 _u8value)
{

	if (!fd)
		if (open_fd() == -1)
			  return -1;

	error_flag=false;
    pthread_mutex_lock(&gI2cmutexBus);
	if(write(fd, &_u8value, 1) != 1)
    {
        pthread_mutex_unlock(&gI2cmutexBus);
        return errorMsg("D i2c write error!\n");
    }
    NEO_I2C_WAIT();
    pthread_mutex_unlock(&gI2cmutexBus);
	return 1;
}

