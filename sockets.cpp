//////////////////////////////////////////////////////////////////////
// sockets.cpp -- Sockets utility functions
// Date: Sat Sep 29 10:04:08 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "sockets.hpp"

bool
Sockets::import_ip(const char *straddr,u_address& addr) {
        bool ipv4f = !!strchr(straddr,'.');  // IPv4 uses '.'
        
        if ( ipv4f )
                return import_ipv4(straddr,addr);
        else    return import_ipv6(straddr,addr);
}

bool
Sockets::import_ipv4(const char *ipv4,u_address& addr) {

        memset(&addr.addr4,0,sizeof addr.addr4);
        if ( !inet_aton(ipv4,&addr.addr4.sin_addr) )
                return false;                           // Invalid address format
        addr.addr4.sin_family = AF_INET;
        return true;
}

bool
Sockets::import_ipv6(const char *ipv6,u_address& addr) {
        char saddr[strlen(ipv6)+1];
        char *iface;   

        memset(&addr.addr6,0,sizeof addr.addr6);
        strcpy(saddr,ipv6);

        if ( (iface = strrchr(saddr,'%')) != 0 )
                *iface++ = 0;

	addr.addr6.sin6_family = 0;
        if ( !inet_pton(AF_INET6,saddr,&addr.addr6.sin6_addr) )
                return false;
        
        if ( iface )
                addr.addr6.sin6_scope_id = if_nametoindex(iface);

        addr.addr6.sin6_family = AF_INET6;
        return true;
} 

int
Sockets::listen(u_address& address,int port,unsigned backlog,bool reuse_port) {
        static const int one = 1;
	int sock, af = address.addr4.sin_family;
	int rc;

        if ( (sock = socket(af,SOCK_STREAM|SOCK_NONBLOCK,0)) == -1 )
                return -1;              // Socket failure, see errno

	rc = setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof one);
	if ( rc == -1 ) {
		::close(sock);
		return -2;      // Set sock opt failure:
        }

#ifdef SO_REUSEPORT
        if ( reuse_port ) {
		rc = setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,(char *)&one,sizeof one);
                if ( rc == -1 ) {  
			::close(sock);
			return -2;      // Set sock opt failure:
		}
	}
#endif
        
        switch ( af ) {
        case AF_INET:
                {
                        sockaddr_in& sock_addr = address.addr4;
                        
			address.addr4.sin_port = htons(port);
                        rc = ::bind(sock,(const struct sockaddr *)&sock_addr,sizeof sock_addr);
                        if ( rc == -1 ) {  
                                ::close(sock);
                                return -3;              // bind failure
                        }
                }
                break;
        case AF_INET6:
                {
                        sockaddr_in6& addr = address.addr6;
                        
			address.addr6.sin6_port = htons(port);
                        rc = ::bind(sock,(const struct sockaddr *)&addr,sizeof addr);
                        if ( rc == -1 ) {  
                                ::close(sock);
                                return -3;              // bind failure
                        }
                }
        }
        
        rc = ::listen(sock,backlog);
        if ( rc == -1 ) {  
                ::close(sock);
                return -4;              // listen(2) failure
        }
	
	return sock;
}

// End sockets.cpp

