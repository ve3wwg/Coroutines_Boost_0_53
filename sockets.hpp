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

#endif // SOCKETS_HPP

// End sockets.hpp
