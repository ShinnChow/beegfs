#include "app/config/Config.h"
#include "common/Common.h"
#include <common/net/sock/RDMASocket.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/toolkit/ListTk.h>

#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <net/sock.h>
#include <net/addrconf.h>


#define NIC_STRING_LEN  1024

// TODO In next cleanup PR, fix ordering of function and remove forward declarations
void __NIC_findAllTCP(NicAddressFilter *nicAddressFilter, int domain, NicAddressList* outList);
void __NIC_filterInterfacesForRDMA(NicAddressList* nicList, int domain, NicAddressList* outList);

void NIC_findAll(NicAddressFilter *nicAddressFilter, bool useRDMA, bool onlyRDMA, int domain,
   NicAddressList* outList)
{
   // find standard TCP/IP interfaces
   __NIC_findAllTCP(nicAddressFilter, domain, outList);

   // find RDMA interfaces (based on TCP/IP interfaces query results)
   if(useRDMA && RDMASocket_rdmaDevicesExist() )
   {
      NicAddressList tcpInterfaces;

      NicAddressList_init(&tcpInterfaces);

      __NIC_findAllTCP(nicAddressFilter, domain, &tcpInterfaces);

      __NIC_filterInterfacesForRDMA(&tcpInterfaces, domain, outList);

      ListTk_kfreeNicAddressListElems(&tcpInterfaces);
      NicAddressList_uninit(&tcpInterfaces);
   }

   if (onlyRDMA)
   {
      NicAddressListIter nicIter;
      NicAddressListIter_init(&nicIter, outList);
      while (!NicAddressListIter_end(&nicIter))
      {
         NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
         if (nicAddr->nicType != NICADDRTYPE_RDMA)
         {
            nicIter = NicAddressListIter_remove(&nicIter);
            kfree(nicAddr);
         }
         else
            NicAddressListIter_next(&nicIter);
      }
   }
}

static void push_nic_address(NicAddressFilter *nicAddressFilter, NicAddressList* outList,
      struct net_device *dev, NicAddrType_t nicType, struct in6_addr ipAddr)
{
   NicAddress *nicAddr = NULL;

   NicAddress nicAddrProto = {0};
   strncpy(nicAddrProto.name, dev->name, IFNAMSIZ);
   nicAddrProto.nicType = nicType;
   nicAddrProto.ipAddr = ipAddr;
#ifdef BEEGFS_RDMA
   nicAddrProto.ibdev = NULL;
#endif

   if (! NicAddressFilter_isAllowed(nicAddressFilter, &nicAddrProto))
      return;  // address filtered out

   nicAddr = (NicAddress *) os_kmalloc(sizeof(NicAddress));

   if (!nicAddr)
   {
      printk_fhgfs(KERN_WARNING, "%s:%d: memory allocation failed. size: %zu\n",
            __func__, __LINE__, sizeof(*nicAddr) );
      return;
   }

   *nicAddr = nicAddrProto;
   NicAddressList_append(outList, nicAddr);
}

void __NIC_findAllTCP(NicAddressFilter *nicAddressFilter, int domain, NicAddressList* outList)
{
   struct net_device *dev;

   // find standard TCP/IP interfaces

   // TODO: do we need to lock RTNL?
   for (dev = first_net_device(&init_net); dev; dev = next_net_device(dev))
   {
      {
         unsigned int flags;
#ifdef KERNEL_HAS_NETIF_GET_FLAGS
         flags = netif_get_flags(dev);
#else
         flags = dev_get_flags(dev);
#endif
         if (flags & IFF_LOOPBACK)
            continue; // loopback interface => skip
      }

      if (dev->type == ARPHRD_LOOPBACK)
         continue;

      // push IPv4 addresses
      {
         struct in_device* in_dev = in_dev_get(dev);
         if (in_dev)
         {
            struct in_ifaddr *ifa = in_dev->ifa_list;
            if (ifa)
            {
               for (; ifa; ifa = ifa->ifa_next)
               {
                  NicAddrType_t nicType = NICADDRTYPE_STANDARD;
                  struct in6_addr ipAddr = beegfs_mapped_ipv4((struct in_addr) {ifa->ifa_local});
                  push_nic_address(nicAddressFilter, outList, dev, nicType, ipAddr);
               }
            }
            else
            {
               printk_fhgfs_debug(KERN_INFO, "found in_dev interface without in_ifaddr ifa_list: %s\n", dev->name);
            }

            in_dev_put(in_dev);
         }
         else
         {
            printk_fhgfs_debug(KERN_INFO, "found interface without in_dev: %s\n", dev->name);
         }
      }

      // push IPv6 addresses
      if (domain == AF_INET6)
      {
         struct inet6_dev* in_dev = in6_dev_get(dev);
         if (in_dev)
         {
            if (! list_empty(&in_dev->addr_list))
            {
               struct inet6_ifaddr *ifa;
               list_for_each_entry(ifa, &in_dev->addr_list, if_list)
               {
                  NicAddrType_t nicType = NICADDRTYPE_STANDARD;
                  struct in6_addr ipAddr = ifa->addr;
                  int type = ipv6_addr_type(&ipAddr);
                  char ipStr[SOCKET_IPADDRSTR_LEN];

                  SocketTk_ipaddrToStr(ipStr, sizeof ipStr, ipAddr);

                  if (!(type & IPV6_ADDR_UNICAST))
                  {
                     printk_fhgfs_debug(KERN_INFO, "ignoring IPv6 addr: %s on device %s: not a unicast address\n", ipStr, dev->name);
                     continue;
                  }

                  if (type & IPV6_ADDR_LOOPBACK)
                  {
                     printk_fhgfs_debug(KERN_INFO, "ignoring IPv6 addr: %s on device %s: is a loopback address\n", ipStr, dev->name);
                     continue;
                  }

                  if (type & IPV6_ADDR_LINKLOCAL)
                  {
                     printk_fhgfs_debug(KERN_INFO, "ignoring IPv6 addr: %s on device %s: is a link-local address\n", ipStr, dev->name);
                     continue;
                  }

                  if (type & IPV6_ADDR_MULTICAST)
                  {
                     printk_fhgfs_debug(KERN_INFO, "ignoring IPv6 addr: %s on device %s: is a multicast address\n", ipStr, dev->name);
                     continue;
                  }

                  if (ifa->flags & (IFA_F_TENTATIVE | IFA_F_TEMPORARY | IFA_F_DEPRECATED | IFA_F_OPTIMISTIC))
                  {
                     printk_fhgfs_debug(KERN_INFO, "ignoring IPv6 addr: %s on device %s: tentative, temporary, deprecated, or detached address\n", ipStr, dev->name);
                     continue;
                  }

                  push_nic_address(nicAddressFilter, outList, dev, nicType, ipAddr);
               }
            }
            else
            {
               printk_fhgfs_debug(KERN_INFO, "found in6_dev interface without inet6_ifaddr ifa_list: %s\n", dev->name);
            }

            in6_dev_put(in_dev);
         }
         else
         {
            printk_fhgfs_debug(KERN_INFO, "found interface without in6_dev: %s\n", dev->name);
         }
      }
   }
}

/**
 * @return static string (not alloc'ed, so don't free it).
 */
const char* NIC_nicTypeToString(NicAddrType_t nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA: return "RDMA";
      case NICADDRTYPE_STANDARD: return "TCP";

      default: return "<unknown>";
   }
}

/**
 * @return string will be kalloced and must be kfreed later
 */
char* NIC_nicAddrToString(NicAddress* nicAddr)
{
   char* nicAddrStr;
   char ipStr[SOCKET_IPADDRSTR_LEN];
   const char* typeStr;

   nicAddrStr = (char*)os_kmalloc(NIC_STRING_LEN);

   SocketTk_ipaddrToStr(ipStr, sizeof ipStr, nicAddr->ipAddr);

   if(nicAddr->nicType == NICADDRTYPE_RDMA)
      typeStr = "RDMA";
   else
   if(nicAddr->nicType == NICADDRTYPE_STANDARD)
      typeStr = "TCP";
   else
      typeStr = "Unknown";

   scnprintf(nicAddrStr, NIC_STRING_LEN, "%s[ip addr: %s; type: %s]", nicAddr->name, ipStr, typeStr);

   return nicAddrStr;
}

bool NIC_supportsRDMA(NicAddressList* nicList)
{
   bool rdmaSupported = false;

   NicAddressListIter iter;
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      if(NicAddressListIter_value(&iter)->nicType == NICADDRTYPE_RDMA)
      {
         rdmaSupported = true;
         break;
      }
   }

   return rdmaSupported;
}

void NIC_supportedCapabilities(NicAddressList* nicList, NicListCapabilities* outCapabilities)
{
   outCapabilities->supportsRDMA = NIC_supportsRDMA(nicList);
}


/**
 * Checks a list of TCP/IP interfaces for RDMA-capable interfaces.
 */
void __NIC_filterInterfacesForRDMA(NicAddressList* nicList, int domain, NicAddressList* outList)
{
   // Note: This works by binding an RDMASocket to each IP of the passed list.

   NicAddressListIter iter;
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      RDMASocket rdmaSock;
      Socket* sock = (Socket*)&rdmaSock;
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      bool bindRes;

      if(!RDMASocket_init(&rdmaSock, domain, nicAddr->ipAddr, NULL) )
         continue;

      bindRes = sock->ops->bindToAddr(sock, nicAddr->ipAddr, 0);

      if(bindRes)
      { // we've got an RDMA-capable interface => append it to outList
         NicAddress* nicAddrCopy = os_kmalloc(sizeof(NicAddress) );

         *nicAddrCopy = *nicAddr;

#ifdef BEEGFS_RDMA
         nicAddrCopy->ibdev = rdmaSock.ibvsock.cm_id->device;
#endif
         nicAddrCopy->nicType = NICADDRTYPE_RDMA;

         NicAddressList_append(outList, nicAddrCopy);
      }

      sock->ops->uninit(sock);
   }
}
