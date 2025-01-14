#ifndef    UNIXSOCKET
#define    UNIXSOCKET

#ifdef		__linux__

#include "ISocket.hpp"

#include <iostream>

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

class      UnixSocket : public ISocket
{
private:
  int     socket_;

public:
	UnixSocket();
	~UnixSocket();

  virtual bool CreateSocket(int domain, int type, int protocol);
  virtual bool BindSocket(int family, int port);
  virtual receive RecvData(int len, int flags);
  virtual bool SendData(in_addr ip, int port, char *buf, int len, int flags);
};

#endif		/* __linux__ */

#endif		/* UNIXSOCKET */
