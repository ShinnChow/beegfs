#include <common/Common.h>
#include <common/system/System.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/StringTk.h>
#include <common/app/log/LogContext.h>

#include "RoutingTable.h"
#include "common/net/sock/IPAddress.h"
#include <netinet/in.h>
#include <set>
#include <arpa/inet.h>

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>
#include <netlink/route/route.h>
#include <netlink/route/addr.h>
#include <netlink/route/nexthop.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/socket.h>


typedef std::unordered_map<int, std::set<IPAddress>> InterfaceMap;

IPAddress makeIPAdress(const nl_addr* ra) {

   void* ba = ::nl_addr_get_binary_addr(ra);
   sa_family_t family = ::nl_addr_get_family(ra);

   if (family == AF_INET)
   {
      auto addr = reinterpret_cast<in_addr*>(ba);
      return IPAddress(*addr);
   } else if (family == AF_INET6) {
      auto addr = reinterpret_cast<in6_addr*>(ba);
      return IPAddress(*addr);
   }

   return IPAddress();
}

/**
 * Destnet indicates which local NIC IPs to use for a particular
 * destination network.
 */
class Destnet
{
   private:
      // net represents the destination network
      IPNetwork        net;
      // sources contains the local NIC IPs that have a route to "net"
      std::set<IPAddress> sources;

   public:

      Destnet() {}

      Destnet(const IPNetwork& net) : net(net) {}

      [[nodiscard]] const IPNetwork& getNet() const
      {
         return net;
      }

      void addSource(const IPAddress& a)
      {
         sources.insert(a);
      }

      [[nodiscard]] const std::set<IPAddress>& getSources() const
      {
         return sources;
      }

      // match() indicates if the passed address is in "net"
      [[nodiscard]] bool isInNetwork(const IPAddress& a) const
      {
         return net.containsAddress(a);
      }

      // hasSource() indicates if the passed address is in "sources"
      [[nodiscard]] bool hasSource(const IPAddress& addr) const
      {
         return sources.find(addr) != sources.end();
      }

      [[nodiscard]] bool operator==(const Destnet& o) const
      {
         return net == o.net &&
            sources == o.sources;
      }
};

/**
 * Abstract definition of classes that create a DestnetMap.
 */
class RoutingTableQuery
{
   public:
      virtual void init() = 0;
      /**
       * Perform the query.
       * @return the created DestnetMap. Ownership is released to the caller.
       */
      virtual DestnetMap* query() = 0;
      virtual ~RoutingTableQuery() {}
};

/**
 * Implementation of RoutingTableQuery that encapsulates use of libnl3-route
 * to access the OS routing tables.
 */
class Nl3RouteQuery : public RoutingTableQuery
{

   private:
      struct nl_sock*       sock;
      struct nl_cache_mngr* cacheMngr;
      struct nl_cache*      routeCache;
      struct nl_cache*      addrCache;

      void destroy()
      {
         if (cacheMngr)
            ::nl_cache_mngr_free(cacheMngr);
         if (sock)
            ::nl_socket_free(sock);
      }

      bool addrTo_in_addr(struct nl_addr* ra, struct in_addr& addr)
      {
         if (ra)
         {
            if (::nl_addr_get_family(ra) == AF_INET)
            {
               void* ba = ::nl_addr_get_binary_addr(ra);
               if (ba)
               {
                  addr = *reinterpret_cast<struct in_addr*>(ba);
                  return true;
               }
            }
         }
         return false;
      }

      bool addrTo_in_addr(struct rtnl_addr* ra, struct in_addr& addr)
      {
         if (ra)
         {
            struct nl_addr* na = ::rtnl_addr_get_local(ra);
            return this->addrTo_in_addr(na, addr);
         }
         return false;
      }

      void populateInterfaces(InterfaceMap& interfaces)
      {
         // build map of set of interface addresses keyed by interface index
         for (struct nl_object* no = ::nl_cache_get_first(addrCache); no != NULL;
              no = ::nl_cache_get_next(no))
         {
            struct rtnl_addr* raddr = reinterpret_cast<struct rtnl_addr*>(no);

            struct nl_addr* na = ::rtnl_addr_get_local(raddr);
            auto addr = makeIPAdress(na);
            if (!addr.isZero())
               interfaces[::rtnl_addr_get_ifindex(raddr)].insert(addr);
         }
      }

      void populateDestnetMap(InterfaceMap& interfaces, DestnetMap& dests)
      {
         // populate dests from NL3 data
         for (struct nl_object* no = ::nl_cache_get_first(routeCache); no != NULL;
              no = ::nl_cache_get_next(no))
         {
            struct rtnl_route* re = reinterpret_cast<struct rtnl_route*>(no);
            uint8_t family = rtnl_route_get_family(re);
            if ((family == AF_INET || family == AF_INET6) &&
               ::rtnl_route_get_type(re) == RTN_UNICAST)
            {
               struct nl_addr* dstaddr = ::rtnl_route_get_dst(re);
               auto key = makeIPAdress(dstaddr);
               if (key.isZero())
                  continue;
               int dstpref = ::nl_addr_get_prefixlen(dstaddr);

               auto s = dests.find(key);
               Destnet* t;
               if (s == dests.end())
               {
                  t = &(dests[key]);
                  // LOG(GENERAL, WARNING, "CCCC ", key.toString(), dstpref);
                  *t = Destnet(IPNetwork(key, dstpref));
               }
               else
               {
                  t = &s->second;
               }

               struct nl_addr* prefsrc = ::rtnl_route_get_pref_src(re);
               if (prefsrc)
               {
                  auto addr = makeIPAdress(prefsrc);
                  if (family == AF_INET) {
                     if (!addr.isZero() && addr.isIPv4()) {
                        t->addSource(addr);
                     }
                  } else if (family == AF_INET6) {
                     if (!addr.isZero() && addr.isIPv6()) {
                        t->addSource(addr);
                     }
                  }
               }
               else
               {
                  // this appears to happen for the default route
                  int nhs = ::rtnl_route_get_nnexthops(re);
                  for (int i = 0; i < nhs; i++)
                  {
                     struct rtnl_nexthop* nh = ::rtnl_route_nexthop_n(re, i);
                     if (nh)
                     {
                        int ifn = ::rtnl_route_nh_get_ifindex(nh);
                        for (auto& addr : interfaces[ifn]) {
                           if (family == AF_INET && addr.isIPv4()) {
                              t->addSource(addr);
                           } else if (family == AF_INET6 && addr.isIPv6()) {
                              t->addSource(addr);
                           }
                        }
                     }
                  }
               }
            }
         }
      }

public:

      Nl3RouteQuery() : sock(NULL), cacheMngr(NULL),
                        routeCache(NULL),
                        addrCache(NULL) {}

      ~Nl3RouteQuery() override
      {
         // the individual caches are owned by cacheMngr
         if (cacheMngr)
            ::nl_cache_mngr_free(cacheMngr);
         if (sock)
            ::nl_socket_free(sock);
      }

      void init() override
      {
         int rc;

         if (sock != NULL)
            throw RoutingTableException("Already initialized");

         if ((sock = ::nl_socket_alloc()) == NULL)
            throw RoutingTableException("Failed to allocate nl socket");

         ::nl_socket_set_nonblocking(sock);

         if ((rc = ::nl_cache_mngr_alloc(sock, NETLINK_ROUTE, NL_AUTO_PROVIDE, &cacheMngr)) != 0)
            throw RoutingTableException("nl_cache_mngr_alloc failed rc=" + StringTk::intToStr(rc));

         if ((rc = ::rtnl_route_alloc_cache(sock, AF_UNSPEC, 0, &routeCache) != 0))
            throw RoutingTableException("rtnl_route_alloc_cache failed, err=" + StringTk::intToStr(rc));

         if ((rc = ::nl_cache_mngr_add_cache(cacheMngr, routeCache, NULL, this) != 0))
         {
            ::nl_cache_free(routeCache);
            throw RoutingTableException("nl_cache_mngr_add_cache failed - route, err=" + StringTk::intToStr(rc));
         }

         if ((rc = ::rtnl_addr_alloc_cache(sock, &addrCache) != 0))
            throw RoutingTableException("rtnl_route_alloc_cache failed, err=" + StringTk::intToStr(rc));

         if ((rc = ::nl_cache_mngr_add_cache(cacheMngr, addrCache, NULL, this) != 0))
         {
            ::nl_cache_free(addrCache);
            throw RoutingTableException("nl_cache_mngr_add_cache failed - addr, err=" + StringTk::intToStr(rc));
         }
      }

      DestnetMap* query() override
      {
         std::unique_ptr<DestnetMap> dests = std::make_unique<DestnetMap>();
         InterfaceMap interfaces;
         populateInterfaces(interfaces);
         populateDestnetMap(interfaces, *dests);
         return dests.release();
      }
};

RoutingTable::RoutingTable(std::shared_ptr<const DestnetMap> destnets,
                           std::shared_ptr<const NetFilter> noDefaultRouteNets)
   : destnets(destnets),
     noDefaultRouteNets(noDefaultRouteNets)
{
}

bool RoutingTable::findSource(const Destnet* dn, const NicAddressList& nicList, IPAddress& addr) const
{
   LogContext log("RoutingTable (findSource)");
   //log.log(Log_DEBUG, dn->dump());
   for (auto& s : nicList)
   {
      //log.log(Log_ERR, std::string("try ") + Socket::ipaddrToStr(s.ipAddr));
      if (dn->hasSource(s.ipAddr))
      {
         addr = s.ipAddr;
         //log.log(Log_DEBUG, std::string("net=") + Socket::ipaddrToStr(dn->getNet().address.s_addr) + " use addr " + Socket::ipaddrToStr(addr));
         return true;
      }
   }
   return false;
}

bool RoutingTable::match(const IPAddress& addr, const NicAddressList& nicList, IPAddress& src) const
{
   LogContext log("RoutingTable (match)");
   const Destnet* defaultRoute = NULL;

   if (destnets == nullptr)
      throw RoutingTableException("RoutingTable not initialized");

   //log.log(Log_DEBUG, std::string("addr=") + Socket::ipaddrToStr(addr));

   for (auto ds = destnets->begin(); ds != destnets->end(); ds++)
   {
      auto* d = &ds->second;
      if (d->getNet().isDefaultNetwork())
         defaultRoute = d;
      else
      {
         // log.log(Log_WARNING, std::string("addr=") + addr.toString() + " net=" + d->getNet().data().toString() + " isInNet=" + (d->isInNetwork(addr) ? "Y" : "N") + " prefix=" + (int)d->getNet().getPrefix());
         if (d->isInNetwork(addr) && findSource(d, nicList, src))
            return true;
      }
   }

   if (defaultRoute != NULL)
   {
      if (noDefaultRouteNets != nullptr)
      {
         for (auto& n : *noDefaultRouteNets)
         {
            if (n.containsAddress(addr)) // XXX
               return false;
         }
      }
      //log.log(Log_DEBUG, std::string("trying default route, addr=") + Socket::ipaddrToStr(addr) + " net=" + Socket::ipaddrToStr(defaultRoute->getNet().address.s_addr));
      return findSource(defaultRoute, nicList, src);
   }

   return false;
}

void RoutingTableFactory::init()
{
   LogContext log("RoutingTableFactory (init)");
   log.log(Log_DEBUG, "");
   if (initialized)
      throw RoutingTableException("RoutingTableFactory already initialized");
   initialized = true;
}

bool RoutingTableFactory::load()
{
   LogContext log("RoutingTableFactory (load)");
   log.log(Log_DEBUG, "");

   if (!initialized)
      throw RoutingTableException("RoutingTableFactory not initialized");

   std::unique_ptr<RoutingTableQuery> q(new Nl3RouteQuery());
   q->init();
   std::unique_ptr<DestnetMap> dn(q->query());

   std::unique_lock<std::mutex> lock(destnetsMutex);
   bool changed = (destnets == nullptr) || !(*destnets == *dn);
   if (changed)
      destnets = std::move(dn);

   return changed;
}

RoutingTable RoutingTableFactory::create(std::shared_ptr<const NetFilter> noDefaultRouteNets)
{
   LogContext log("RoutingTableFactory (create)");
   log.log(Log_DEBUG, "");

   std::unique_lock<std::mutex> lock(destnetsMutex);

   if (destnets == nullptr)
      throw RoutingTableException("RoutingTableFactory not loaded");

   return RoutingTable(destnets, noDefaultRouteNets);
}

bool RoutingTable::loadIpSourceMap(const NicAddressList& nicList, const NicAddressList &localNicList,
   IpSourceMap& srcMap) const
{
   LogContext log("RoutingTable (loadIPSourceMap)");
   bool result = false;

   if (destnets == nullptr)
      throw RoutingTableException("RoutingTable not initialized");

   srcMap.clear();
   for (auto& s : nicList)
   {
      IPAddress src;
      if (match(s.ipAddr, localNicList, src))
         srcMap[s.ipAddr] = src;
   }
   if (srcMap.empty())
   {
      std::string nics;
      std::string ips;
      for (auto& s : nicList)
      {
         nics += std::string(" ") + s.name;
         ips += std::string(" ") + s.ipAddr.toString();
      }
      log.log(Log_ERR, std::string("No routes found ") + " nicList: [" + nics +
         " ] ipList: [" + ips + " ] localNicList.size: " +
         StringTk::intToStr((int)(localNicList.size())));
   }
   else
      result = true;

   return result;
}


