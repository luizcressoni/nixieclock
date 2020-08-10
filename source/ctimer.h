/*! \file ctimer.h */

#include <linux/types.h>

class cTimer
{
    private:
        __u32 m_u32TimeOut;
        __u32 m_u32TimeAgora;
        bool habilitado;
   	public:
   	    cTimer(void);
   	    ~cTimer(void);
		void SetTimeOut(__u32 _MSeg);
		bool IsTimeOut(void);
		__u32 ReadTimeOut(void);
		void Enable(bool _enable);
		bool IsEnabled(void);
};

//eof ctimer.h
