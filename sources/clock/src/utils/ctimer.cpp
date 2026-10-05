#include "ctimer.h"
#include <sys/time.h>



/*! Construtor */
cTimer::cTimer()
{
    m_u32TimeNow = 0;
    m_u32TimeOut = 0;
    m_enabled = true;
}

cTimer::~cTimer()
= default;

//! Defines how long the timer will take to generate a TimeOut (in ms)
/*!
      \param MSeg : Desired timeout period in ms.
*/
void cTimer::SetTimeOut(uint32_t MSeg)
{
    m_enabled = true;
    timeval tv{};
    gettimeofday(&tv, nullptr);
    m_u32TimeNow = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    m_u32TimeOut = m_u32TimeNow + MSeg;

}

//! Detect if a TimeOut occurred
/*!
      \return true if it did. Guess what if it didn't?
*/
bool cTimer::IsTimeOut() const
{
    bool ret;
    if (!m_enabled)
    {
        return false;
    }

    timeval tv{};
    gettimeofday(&tv, nullptr);
    uint32_t m_u32Time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    if (m_u32TimeOut >= m_u32TimeNow)
        ret = (m_u32Time >= m_u32TimeOut) || (m_u32Time < m_u32TimeNow);
    else
        ret = (m_u32Time >= m_u32TimeOut) && (m_u32Time < m_u32TimeNow);

    return ret;
}

//! Get how much time is remaining until timeout
/*!
      \return time remeining in ms.
      \sa SetTimeOut(uint32_t _MSeg)
    */
uint32_t cTimer::ReadTimeOut() const
{
    if (IsTimeOut())
        return 0;

    timeval tv{};
    gettimeofday(&tv, nullptr);

    return m_u32TimeOut - (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void cTimer::Enable(bool _enable)
{
    m_enabled = _enable;
}

bool cTimer::IsEnabled() const
{
    return m_enabled;
}

//eof ctimer.cpp