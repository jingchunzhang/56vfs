#ifndef  TEXCEPTION_H
#define  TEXCEPTION_H

class TException  
{
public:	
	TException();

	virtual ~TException();

	TException(int code,const char* fmt,...);

	TException(const char* fmt,...);

public:
	virtual void getError(int& code,char* msg,int len);

	const char* getErrMsg(void);

	virtual void print();
protected:
	int m_code;
    char m_msg[2048];
};

#endif
