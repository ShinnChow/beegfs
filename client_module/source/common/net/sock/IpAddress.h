#ifndef BEEGFS_IPADDRESS_H_
#define BEEGFS_IPADDRESS_H_

#include <linux/bug.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <net/ipv6.h>
#include <linux/net.h>

// Map IPv4 address as IPv6 address
// ipv4 => ::FF:FF:ipv4
static inline struct in6_addr beegfs_mapped_ipv4(struct in_addr addr)
{
   struct in6_addr out = {0};
   out.s6_addr32[0] = htonl(0x00000000);
   out.s6_addr32[1] = htonl(0x00000000);
   out.s6_addr32[2] = htonl(0x0000FFFF);
   out.s6_addr32[3] = addr.s_addr;
   return out;
}


static inline struct sockaddr_in beegfs_make_sockaddr_in(struct in_addr addr, unsigned short port)
{
   struct sockaddr_in out = {0};
   out.sin_family = AF_INET;
   out.sin_port = htons(port);
   out.sin_addr = addr;
   return out;
}

static inline struct sockaddr_in6 beegfs_make_sockaddr_in6(struct in6_addr addr, unsigned short port)
{
   struct sockaddr_in6 out = {0};
   out.sin6_family = AF_INET6;
   out.sin6_port = htons(port);
   out.sin6_addr = addr;
   return out;
}

// helper for type safety. This was important when changing sockaddr types to support IPv6...
// pointer casts are very dangerous since there is limited type checking
static inline struct sockaddr *beegfs_get_sockaddr(struct sockaddr_in6 *addr)
{
   return (struct sockaddr *) addr;
}

static inline uint16_t beegfs_get_port(const struct sockaddr_in6 *addr)
{
   return ntohs(addr->sin6_port);
}

// support compatibility fallback to AF_INET sockets.

typedef struct Beegfs_Sockaddr Beegfs_Sockaddr;
struct Beegfs_Sockaddr
{
   union
   {
      struct sockaddr_in sa;
      struct sockaddr_in6 sa6;
   };
};

static inline struct sockaddr *bsa_ptr(struct Beegfs_Sockaddr *bsa)
{
   return (struct sockaddr *) &bsa->sa;
}

static inline size_t bsa_size(struct Beegfs_Sockaddr *bsa)
{
   struct sockaddr *ptr = bsa_ptr(bsa);
   switch (ptr->sa_family)
   {
      case 0: return sizeof *bsa; // hack for recv()
      case AF_INET: return sizeof bsa->sa;
      case AF_INET6: return sizeof bsa->sa6;
      default: return 0; // shouldn't happen.
   }
}


// domain should be AF_INET or AF_INET6.
static inline bool bsa_make(int domain, struct in6_addr addr, unsigned short port, Beegfs_Sockaddr *outaddr)
{
   Beegfs_Sockaddr out = {0};
   bool res = true;
   if (domain == AF_INET)
   {
      if (ipv6_addr_v4mapped(&addr))
      {
         struct in_addr inaddr = {addr.s6_addr32[3]};
         out.sa = beegfs_make_sockaddr_in(inaddr, port);
      }
      else if (ipv6_addr_equal(&addr, &in6addr_any))
      {
         // hacky special case: zero address
         struct in_addr inaddr = {htonl(INADDR_ANY)};
         out.sa = beegfs_make_sockaddr_in(inaddr, port);
      }
      else if (ipv6_addr_equal(&addr, &in6addr_loopback))
      {
         // hacky special case: loopback address.
         // there are probably more
         struct in_addr inaddr = {htonl(INADDR_LOOPBACK)};
         out.sa = beegfs_make_sockaddr_in(inaddr, port);
      }
      else
      {
         // invalid, but very common. Happens if we detected that IPv6 is disabled on the system and
         // are trying to connect to a remote IPv6 address we received from mgmtd. So just indicate
         // failure and let the caller decide what to do.
         res = false;
      }
   }
   else // assume AF_INET6
   {
      out.sa6 = beegfs_make_sockaddr_in6(addr, port);
   }

   *outaddr = out;
   return res;
}

static inline Beegfs_Sockaddr bsa_make_for_recv(void)
{
   Beegfs_Sockaddr out = {0};
   return out;
}

static inline int beegfs_kernel_connect(struct socket *sock, struct sockaddr *addr,
   int addrlen, int flags)
{
#ifdef KERNEL_HAS_KERNEL_CONNECT_ADDR_UNSIZED
   return kernel_connect(sock, (struct sockaddr_unsized *)addr, addrlen, flags);
#else
   return kernel_connect(sock, addr, addrlen, flags);
#endif
}

static inline int beegfs_kernel_bind(struct socket *sock, struct sockaddr *addr, int addrlen)
{
#ifdef KERNEL_HAS_KERNEL_CONNECT_ADDR_UNSIZED
   return kernel_bind(sock, (struct sockaddr_unsized *)addr, addrlen);
#else
   return kernel_bind(sock, addr, addrlen);
#endif
}

#endif
