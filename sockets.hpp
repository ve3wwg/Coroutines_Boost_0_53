//////////////////////////////////////////////////////////////////////
// sockets.hpp -- Socket related
// Date: Sat Sep 29 09:54:52 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef SOCKETS_HPP
#define SOCKETS_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

union u_address {
	struct sockaddr		addr;
	struct sockaddr_in 	addr4;		// AF_INET
	struct sockaddr_in6	addr6;		// AF_INET6
};

namespace Sockets {
	bool import_ip(const char *straddr,u_address& addr);
	bool import_ipv4(const char *ipv4,u_address& addr);
	bool import_ipv6(const char *ipv6,u_address& addr);
	int listen(u_address& address,int port,unsigned backlog,bool reuse_port=false);
}

#endif // SOCKETS_HPP

// End sockets.hpp
