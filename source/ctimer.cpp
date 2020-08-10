#include "ctimer.h"

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>


/*! Construtor */
cTimer::cTimer(void)
{
    m_u32TimeAgora = 0;
    m_u32TimeOut = 0;
    habilitado = true;
}

cTimer::~cTimer(void)
{

}

//! Define o tempo que o timer leva para gerar TimeOut (em ms)
/*!
      \param _MSeg :tempo desejado para timeout em ms.
*/
void cTimer::SetTimeOut(__u32 _MSeg)
{
    habilitado = true;
    timeval tv;
    gettimeofday(&tv, NULL);
    m_u32TimeAgora = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    m_u32TimeOut = m_u32TimeAgora + _MSeg;

}

//! Determina se o objeto atingiu o tempo limite definido em SetTimeOut
/*!
      \return true se ocorreu
*/
bool cTimer::IsTimeOut(void)    //se ja deu timeout ou nao
{
    bool ret;
    if (!habilitado)
    {
        return false;
    }

    timeval tv;
    gettimeofday(&tv, NULL);
    __u32 m_u32Time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    if (m_u32TimeOut >= m_u32TimeAgora)
    {
        //condicao normal
        ret = (m_u32Time >= m_u32TimeOut) || (m_u32Time < m_u32TimeAgora);
    }
    else
    {
        //condicao inversa
        ret = (m_u32Time >= m_u32TimeOut) && (m_u32Time < m_u32TimeAgora);
    }

    return ret;
}

//! Determina quanto falta para TimeOut
/*!
      \return tempo restante para timeout em ms.
      \sa SetTimeOut(__u32 _MSeg)
    */
__u32 cTimer::ReadTimeOut(void)  //qto falta para o timeout
{
    if (IsTimeOut())
    {
        return 0;
    }

    timeval tv;
    gettimeofday(&tv, NULL);

    return m_u32TimeOut - (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

//! Habilita ou desabilita o timer
/*!
      \param _enable :True para habilitar, false para desabilitar
      \warning SetTimeOut(__u32 _MSeg) automaticamente habilita o timer
    */
void cTimer::Enable(bool _enable)
{
    habilitado = _enable;
}

//! verifica se esta habilitado
/*!
      \return true se estiver habilitado
*/
bool cTimer::IsEnabled(void)
{
    return habilitado;
}

