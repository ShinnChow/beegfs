#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/ArrayTypeTraits.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include "AbstractConfig.h"
#include "common/toolkit/HashTk.h"

#include <../generated/CommonConfigFields.inc>  // bake in data definitions


#define ABSTRACTCONF_AUTHFILE_READSIZE 1024 // max amount of data that we read from auth file
#define ABSTRACTCONF_AUTHFILE_MINSIZE  4 // at least 2, because we compute two 32bit hashes


AbstractConfig::AbstractConfig(int argc, char** argv) :
      argc(argc), argv(argv)
{
   // initConfig(..) must be called by derived classes
   // because of the virtual init method loadDefaults()
}

/**
 * Note: This must not be called from the AbstractConfig (or base class) constructor, because it
 * calls virtual methods of the sub classes.
 *
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend dashes to defaults and config file keys (used e.g. by fhgfs-ctl)
 */
void AbstractConfig::initConfig(int argc, char** argv, bool enableException, bool addDashes)
{
   // load and apply args to see whether we have a cfgFile
   loadDefaults(addDashes);
   loadFromArgs(argc, argv);

   applyConfigMap(enableException, addDashes);

   if(this->cfgFile.length() )
   { // there is a config file specified
      // start over again and include the config file this time
      this->configMap.clear();

      const std::string tmpCfgFile = this->cfgFile; /* need tmp variable here to make sure we don't
         override the value when we use call loadDefaults() again below, because subclasses have
         direct access to this->cfgFile */

      loadDefaults(addDashes);
      loadFromFile(tmpCfgFile.c_str(), addDashes);
      loadFromArgs(argc, argv);

      applyConfigMap(enableException, addDashes);
   }

   initImplicitVals();
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void AbstractConfig::loadDefaults(bool addDashes)
{
   commonLoadConfigDefaults(&this->configMap, ArraySlice(CommonConfigFields_infos), addDashes);
}

/**
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
void AbstractConfig::applyConfigMap(bool enableException, bool addDashes)
{
   commonApplyConfigMap(static_cast<CommonConfigFields *>(this), &this->configMap, ArraySlice(CommonConfigFields_infos), enableException, addDashes);

   auto processPortSettings = [&](const std::string& name, int& setting, const int& tcp, const int& udp, const int def) {
      if(setting == -1) {
         if(tcp != -1 && udp != -1 && tcp != udp) {
            throw InvalidConfigException("Deprecated config arguments '" + name + "UDP' and \
'" + name + "TCP' set to different values, which is no longer allowed. Please use the new '" + name + "' \
setting instead.");
         }

         // Set the new setting using the old values
         if(tcp != -1) {
            setting = tcp;
            LOG_CTX(GENERAL, WARNING, "Config", "Using deprecated config argument '" + name + "TCP'");
         } else if(udp != -1) {
            setting = udp;
            LOG_CTX(GENERAL, WARNING, "Config", "Using deprecated config argument '" + name + "UDP'");
         } else {
            setting = def;
         }
      } else {
         if(tcp != -1 || udp != -1) {
            throw InvalidConfigException("Deprecated config arguments '" + name + "UDP/TCP' set along with the new \
'" + name + "' setting. Please use only the new setting.");
         }
      }
   };

   processPortSettings("connClientPort", this->connClientPort, this->connClientPortTCP, this->connClientPortUDP, 8004);
   processPortSettings("connStoragePort", this->connStoragePort, this->connStoragePortTCP, this->connStoragePortUDP, 8003);
   processPortSettings("connMetaPort", this->connMetaPort, this->connMetaPortTCP, this->connMetaPortUDP, 8005);
   processPortSettings("connMonPort", this->connMonPort, this->connMonPortTCP, this->connMonPortUDP, 8007);
   processPortSettings("connMgmtdPort", this->connMgmtdPort, this->connMgmtdPortTCP, this->connMgmtdPortUDP, 8008);

   connMessagingTimeouts[0] = connMessagingTimeouts[0] > 0 ? connMessagingTimeouts[0] : CONN_LONG_TIMEOUT;
   connMessagingTimeouts[1] = connMessagingTimeouts[1] > 0 ? connMessagingTimeouts[1] : CONN_MEDIUM_TIMEOUT;
   connMessagingTimeouts[2] = connMessagingTimeouts[2] > 0 ? connMessagingTimeouts[2] : CONN_SHORT_TIMEOUT;

   {
      size_t numRDMATimeouts = this->connRDMATimeouts.size();
      // YUCK. beegfs-ctl and beegfs-fsck use beegfs-client.conf. That file defines
      // connRDMATimeouts as 5 comma-separated values, but user space only has 3
      // values. The utils are supposed to ignore the configuration. cfgUtilValCount
      // is a hack to prevent the configuration from causing a runtime error.
      if (numRDMATimeouts != 3 && numRDMATimeouts != 5)
      {
         throw InvalidConfigException(
            "Wrong number of connRDMATimeouts configured. Expected: 3 or 5, got: "
            + std::to_string(numRDMATimeouts));
      }
   }
}

void AbstractConfig::initImplicitVals()
{
   // nothing to be done here (just a dummy so that derived classes don't have to impl this)
}

/**
 * Initialize interfaces list from interfaces file if the list is currently empty and the filename
 * is not empty.
 *
 * @param inoutConnInterfacesList will be initialized from file (comma-separated) if it was empty
 *
 * @throw InvalidConfigException if interfaces filename and interface list are both not empty
 */
void AbstractConfig::initInterfacesList(const std::string& connInterfacesFile,
                                        std::string& inoutConnInterfacesList)
{
   // make sure not both (list and file) were specified
   if(!inoutConnInterfacesList.empty() && !connInterfacesFile.empty() )
   {
      throw InvalidConfigException(
         "connInterfacesFile and connInterfacesList cannot be used together");
   }
   
   if(!inoutConnInterfacesList.empty() )
      return; // interfaces already given as list => nothing to do

   if(connInterfacesFile.empty() )
      return; // no interfaces file given => nothing to do


   // load interfaces from file...

   StringList loadedInterfacesList;

   loadStringListFile(connInterfacesFile.c_str(), loadedInterfacesList);

   inoutConnInterfacesList = StringTk::implode(',', loadedInterfacesList, true);
}

/**
 * Generate connection authentication hash based on contents of given authentication file.
 *
 * @param outConnAuthHash will be set to 0 if file is not defined
 *
 * @throw InvalidConfigException if connAuthFile is defined, but cannot be read.
 */
void AbstractConfig::initConnAuthHash(const std::string& connAuthFile, uint64_t* outConnAuthHash)
{
   if (connDisableAuthentication) {
      *outConnAuthHash = 0;
      return; // connAuthFile explicitly disabled => no hash to be generated
   }

   // Connection authentication not explicitly disabled, so bail if connAuthFile is not configured
   if(connAuthFile.empty())
      throw ConnAuthFileException("No connAuthFile configured. Using BeeGFS without connection authentication is considered insecure and is not recommended. If you really want or need to run BeeGFS without connection authentication, please set connDisableAuthentication to true.");

   // open file...

   /* note: we don't reuse something like loadStringListFile() here, because:
       1) the auth file might not contain a string, but can be any binary data (including zeros).
       2) we want to react on EACCES. */

   int fd = open(connAuthFile.c_str(), O_RDONLY);

   int errCode = errno;

   if( (fd == -1) && (errCode == EACCES) )
   { // current effective user/group ID not allowed to read file => try it with saved IDs
      unsigned previousUID;
      unsigned previousGID;

      // switch to saved IDs
      System::elevateUserAndGroupFsID(&previousUID, &previousGID);

      fd = open(connAuthFile.c_str(), O_RDONLY);

      errCode = errno; // because ID dropping might change errno

      // restore previous IDs
      System::setFsIDs(previousUID, previousGID, &previousUID, &previousGID);
   }

   if(fd == -1)
   {
      throw ConnAuthFileException("Unable to open auth file: " + connAuthFile + " "
            "(SysErr: " + System::getErrString(errCode) + ")");
   }

   // load file contents...

   unsigned char buf[ABSTRACTCONF_AUTHFILE_READSIZE];

   const ssize_t readRes = read(fd, buf, ABSTRACTCONF_AUTHFILE_READSIZE);

   errCode = errno; // because close() might change errno

   close(fd);

   if(readRes < 0)
   {
      throw InvalidConfigException("Unable to read auth file: " + connAuthFile + " "
         "(SysErr: " + System::getErrString(errCode) + ")");
   }

   // empty authFile is probably unintended, so treat it as error
   if(!readRes || (readRes < ABSTRACTCONF_AUTHFILE_MINSIZE) )
      throw InvalidConfigException("Auth file is empty: " + connAuthFile);


   // hash file contents
   *outConnAuthHash = HashTk::authHash(buf, readRes);
}

/**
 * Sets the value of connTCPRcvBufSize and connUDPRcvBufSize according to the configuration.
 * 0 indicates legacy behavior that uses RDMA bufsizes. Otherwise leave the values as
 * configured.
 */
void AbstractConfig::initSocketBufferSizes()
{
   int legacy = connRDMABufSize * connRDMABufNum;

   if (connTCPRcvBufSize == 0)
      connTCPRcvBufSize = legacy;

   if (connUDPRcvBufSize == 0)
      connUDPRcvBufSize = legacy;
}

/**
 * @param addDashes true to prepend "--" to every config key.
 */
void AbstractConfig::loadFromFile(const char* filename, bool addDashes)
{
   if(!addDashes)
   { // no dashes needed => load directly into configMap
      MapTk::loadStringMapFromFile(filename, &configMap);
      return;
   }

   /* we need to add dashes to keys => use temporary map with real keys and then copy to actual
      config map with prepended dashes. */

   StringMap tmpMap;

   MapTk::loadStringMapFromFile(filename, &tmpMap);

   for(StringMapCIter iter = tmpMap.begin(); iter != tmpMap.end(); iter++)
      configMapRedefine(&this->configMap, iter->first, iter->second, addDashes);
}

/**
 * Note: No addDashes param here, because the user should specify dashes himself on the command line
 * if they are needed.
 */
void AbstractConfig::loadFromArgs(int argc, char** argv)
{
   for(int i=1; i < argc; i++)
      MapTk::addLineToStringMap(argv[i], &configMap);
}


// Load a list of IPNetworks from the given file if the file name is not empty
NetFilter loadNetworkList(const std::string& file) {
   if (file.empty())
      return {};

   StringList filterList;
   CommonConfig::loadStringListFile(file.c_str(), filterList);

   NetFilter res;
   for (const auto& f : filterList) {
      try {
         auto net = IPNetwork::fromCidr(f);
         res.push_back(net);
      } catch (std::exception &e) {
         LOG_CTX(GENERAL, WARNING, "Config", e.what());
      }
   }

   return res;
}

