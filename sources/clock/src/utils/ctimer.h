/*! \file ctimer.h */
#pragma once
#include <stdint.h>

class cTimer
{
    private:
		uint32_t m_u32TimeOut;
        uint32_t m_u32TimeNow;
        bool m_enabled;
   	public:
   	    cTimer();
   	    ~cTimer();
		void SetTimeOut(uint32_t _MSeg);
		[[nodiscard]] bool IsTimeOut() const;
		uint32_t ReadTimeOut() const;
		void Enable(bool _enable);
		bool IsEnabled() const;
};

//eof ctimer.h
