#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "TException.h"

TException::TException()
{
	m_code = 0;
	m_msg[0] = '\0';
}

TException::~TException()
{
}

TException::TException(int code,const char* fmt,...)
{
	va_list		ap;
	va_start(ap,fmt);
	vsnprintf(m_msg,sizeof(m_msg),fmt,ap);
	va_end(ap);
	m_code = code;
}

TException::TException(const char* fmt,...)
{
	va_list		ap;
	va_start(ap,fmt);
	vsnprintf(m_msg,sizeof(m_msg),fmt,ap);
	va_end(ap);
	m_code = -1;
}

void TException::getError(int& code,char* msg,int len)
{
	code = m_code;
	snprintf(msg,len,"%s",m_msg);
}

const char* TException::getErrMsg(void)
{
	return m_msg;
}

void TException::print()
{
	printf("Exception!!Code[%d],Msg[%s]\n",m_code,m_msg);
}

