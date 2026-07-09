#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/MapTk.h>
#include "CommonConfig.h"
#include <stdarg.h> // NOLINT

static std::string string_printfv(const char *fmt, va_list ap)
{
   va_list copy;
   va_copy(copy, ap);
   int len = vsnprintf(NULL, 0, fmt, copy);
   va_end(copy);
   std::string result;
   result.resize(len);
   vsnprintf(result.data(), result.size() + 1, fmt, ap);
   return result;
}

__attribute__((format(printf, 1, 2)))
static std::string string_printf(const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   std::string str = string_printfv(fmt, ap);
   va_end(ap);
   return str;
}

// TODO: It seems nothing is logged. Probably because config parsing happens
// before logger initialization
__attribute__((format(printf, 2, 3)))
static void log_config(LogLevel logLevel, const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   std::string str = string_printfv(fmt, ap);
   va_end(ap);
   LogContext("Reading config").log(LogTopic_GENERAL, logLevel, str.c_str());
}

__attribute__((format(printf, 2, 3)))
static InvalidConfigException config_exception(CfgInfo const& cfginfo, const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   std::string msg = string_printfv(fmt, ap);
   va_end(ap);
   std::string what = string_printf("%s: %s", cfginfo.name.c_str(), msg.c_str());
   return InvalidConfigException(what);
}


template<typename T>
static inline void set_value(void *first_elem_ptr, size_t elem_index, T value)
{
   ((T *) first_elem_ptr)[elem_index] = value;
}

static void check_range_int64(CfgInfo cfginfo, int64_t val, int64_t minval, int64_t maxval)
{
   if (val < minval)
      throw config_exception(cfginfo, "value %" PRIi64 " < lower bound of %" PRIi64, val, minval);
   if (val > maxval)
      throw config_exception(cfginfo, "value %" PRIi64 " > upper bound of %" PRIi64, val, maxval);
}

static void check_range_uint64(CfgInfo const& cfginfo, uint64_t val, uint64_t maxval)
{
   if (val > maxval)
      throw config_exception(cfginfo, "value %" PRIu64 " > upper bound of %" PRIu64, val, maxval);
}

/**
 * Note: Read the addDashesToTestKey comment on case-sensitivity.
 *
 * @param addDashesToTestKey true to prepend "--" to testKey before testing for match; if this
 * is specified, the check will also be case-insensitive (otherwise it is case-sensitive).
 * @return true if iter->first equals testKey.
 */
bool testConfigMapKeyMatch(const std::string& name, const std::string& testKey,
   bool addDashesToTestKey)
{
   if(addDashesToTestKey)
   {
      std::string testKeyDashed = "--" + testKey;
      return (!strcasecmp(name.c_str(), testKeyDashed.c_str() ) );
   }
   else
      return (name == testKey);
}


/**
 * @param addDashes true to prepend "--" to the keyStr.
 */
void configMapRedefine(StringMap *configMap, std::string keyStr, std::string valueStr, bool addDashes)
{
   std::string keyStrInternal;

   if(addDashes)
      keyStrInternal = "--" + keyStr;
   else
      keyStrInternal = keyStr;

   MapTk::stringMapRedefine(keyStrInternal, valueStr, configMap);
}

void commonLoadConfigDefaults(StringMap *configMap, ArraySlice<const CfgInfo> cfgInfos, bool addDashes)
{
   for (CfgInfo const& info : cfgInfos.iterateAsConstRefs())
   {
      if (! (info.cfgflags & CFGFLAG_OBSOLETE_FIELD))
      {
         configMapRedefine(configMap, info.name, info.defaultval, addDashes);
      }
   }
}

static bool is_integer_type(CfgType elem_type)
{
   switch (elem_type)
   {
      case CFGTYPE_INT:
      case CFGTYPE_INT64:
      case CFGTYPE_UINT:
      case CFGTYPE_UINT64:
      case CFGTYPE_UINT8:
         return true;
      default:
         return false;
   }
}

static bool is_signed_integer_type(CfgType elem_type)
{
   return (elem_type == CFGTYPE_INT) || (elem_type == CFGTYPE_INT64);
}

static bool read_human(const char *p, const char **endp, int *out)
{
   while (*p && *p <= ' ')
      p++;
   int exp = 0;
   switch (*p|32)
   {
      case 'p': exp = 50; break; // peta
      case 't': exp = 40; break; // tera
      case 'g': exp = 30; break; // giga
      case 'm': exp = 20; break; // mega
      case 'k': exp = 10; break; // kilo
   }
   if (exp)
      p++;
   if ((*p|32)=='b')  // byte
      p++;
   while (*p && *p <= ' ')
      p++;
   *endp = p;
   *out = exp;
   return true;
}

static bool read_uint64(std::string const& value, bool ishuman, uint64_t *out)
{
   const char *str = value.c_str();
   const char *p = NULL;
   errno = 0;
   unsigned long long x = strtoull(str, (char **) &p, 10);
   if (str == p)
      return false;  // no conversion performed (errno may not be set)
   if (errno != 0)
      return false;
   if (ishuman)
   {
      int exp = 0;
      if (! read_human(p, &p, &exp))
         return false;
      if (__builtin_mul_overflow(x, 1ULL << exp, &x))
         return false;
   }
   if (*p)
      return false;  // garbage
   *out = (uint64_t) x;
   return true;
}

static bool read_int64(std::string const& value, bool ishuman, int64_t *out)
{
   const char *str = value.c_str();
   const char *p = NULL;
   errno = 0;
   long long x = strtoll(str, (char **) &p, 10);
   if (str == p)
      return false;  // no conversion performed (errno may not be set)
   if (errno != 0)
      return false;
   if (ishuman)
   {
      int exp = 0;
      if (! read_human(p, &p, &exp))
         return false;
      if (__builtin_mul_overflow(x, 1LL << exp, &x))
         return false;
   }
   if (*p)
      return false;  // garbage
   *out = (int64_t) x;
   return true;
}

static bool commonApplyConfigValue(void *containerStruct, std::string const& name, std::string const& value, ArraySlice<const CfgInfo> cfgInfos, bool addDashes)
{
   // Look up static config description. Note that we don't consider the
   // defaults there, because due to historical reasons the defaults have
   // already been set in the configMap.

   CfgInfo info;
   bool knownElement = false;

   for (const auto& it : cfgInfos.iterateAsConstRefs())
   {
      if (testConfigMapKeyMatch(name, it.name, addDashes))
      {
         knownElement = true;
         info = it;
         break;
      }
   }

   if (knownElement)
   {
      if (info.cfgflags & CFGFLAG_OBSOLETE_FIELD)
      {
         log_config(Log_WARNING, "Config field '%s' is obsolete and will be ignored", name.c_str());
      }
      else
      {
         CfgType elem_type = info.cfgtype;
         bool isminus1 = (info.cfgflags & CFGFLAG_EMPTY_IS_MINUS1) != 0;
         bool istuple = (info.cfgflags & CFGFLAG_IS_TUPLE);
         bool islist = (info.cfgflags & CFGFLAG_IS_LIST);
         bool ishuman = (info.cfgflags & CFGFLAG_HUMAN_READABLE_SIZE) != 0;
         bool isinteger = is_integer_type(elem_type);
         bool issigned = is_signed_integer_type(elem_type);

         if (elem_type == CFGTYPE_NONE)
         {
            throw config_exception(info, "CFGTYPE_NONE is only allowed if CFGFLAG_OBSOLETE_FIELD is set");
         }

         if (istuple && islist)
         {
            throw config_exception(info, "CFGFLAG_IS_LIST and CFGFLAG_IS_TUPLE are mutually incompatible");
         }

         if (isminus1 && ! issigned)
         {
            throw config_exception(info, "CFGFLAG_EMPTY_IS_MINUS1 is only valid with (signed) integer configs, got: %s", cfgtype_string(elem_type));
         }

         if (ishuman && (! isinteger))
         {
            throw config_exception(info, "CFGFLAG_HUMAN_READABLE_SIZE flag is only valid with unsigned integer configs, got: %s", cfgtype_string(elem_type));
         }

         std::vector<std::string> elems;
         if (islist || istuple)
         {
            std::list<std::string> elemlist;
            StringTk::explode(value, ',', &elemlist);
            if (istuple)
            {
               size_t tuplesize = cfgflags_get_tuplesize(info.cfgflags);
               if (elemlist.size() != tuplesize)
               {
                  throw config_exception(info,
                        "Wrong number of config elements. Got: \"%s\" (%zu elems), required: %zu elems",
                        value.c_str(), elemlist.size(), tuplesize);
               }
            }
            for (auto const& elem : elemlist)
            {
               elems.push_back(StringTk::trim(elem));
            }
         }
         else
         {
            std::string elem = value;
            elems.push_back(elem);
         }

         void *first_elem_ptr = NULL;
         if (islist)
         {
            void *untyped_vector = (char *) containerStruct + info.offset;
            switch (elem_type)
            {
#define DOLIST(type) { auto vec = (std::vector<type> *) untyped_vector; vec->resize(elems.size()); first_elem_ptr = vec->data(); }
               case CFGTYPE_STRING: DOLIST(std::string); break;
               case CFGTYPE_INT: DOLIST(int); break;
               case CFGTYPE_UINT: DOLIST(unsigned int); break;
               case CFGTYPE_UINT8: DOLIST(uint8_t); break;
               case CFGTYPE_INT64: DOLIST(int64_t); break;
               case CFGTYPE_UINT64: DOLIST(uint64_t); break;
               // can't support list of bools because of stupid std::vector<bool> specialization. For now, we don't need it.
               //case CFGTYPE_BOOL: DOLIST(bool); break;
               case CFGTYPE_LOGTYPE: DOLIST(LogType); break;
               default:
               case CFGTYPE_NONE: assert(0); break;
            }
         }
         else
         {
            first_elem_ptr = (char *) containerStruct + info.offset;
         }

         for (size_t elem_index = 0; elem_index < elems.size(); elem_index++)
         {
            std::string value = elems.at(elem_index);
            if (isinteger)
            {
               if (issigned)
               {
                  int64_t x;
                  if (value.empty() && isminus1)
                     x = -1;
                  else if (! read_int64(value, ishuman, &x))
                     throw config_exception(info, "Failed to read integer value %s", value.c_str());
                  log_config(Log_DEBUG, "Config field '%s=%s' parsed as %" PRIi64, name.c_str(), value.c_str(), x);
                  if (elem_type == CFGTYPE_INT)
                  {
                     check_range_int64(info, x, INT_MIN, INT_MAX);
                     set_value<int>(first_elem_ptr, elem_index, (int) x);
                  }
                  else if (elem_type == CFGTYPE_INT64)
                  {
                     set_value<int64_t>(first_elem_ptr, elem_index, x);
                  }
                  else
                  {
                     assert(false);
                  }
               }
               else
               {
                  uint64_t x;
                  if (! read_uint64(value, ishuman, &x))
                     throw config_exception(info, "Failed to read config '%s' as integer", value.c_str());
                  log_config(Log_DEBUG, "Config field '%s' parsed as %" PRIu64, name.c_str(), x);
                  if (elem_type == CFGTYPE_UINT)
                  {
                     check_range_uint64(info, x, UINT_MAX);
                     set_value<unsigned int>(first_elem_ptr, elem_index, (unsigned int) x);
                  }
                  else if (elem_type == CFGTYPE_UINT8)
                  {
                     check_range_uint64(info, x, UINT8_MAX);
                     set_value<uint8_t>(first_elem_ptr, elem_index, (uint8_t) x);
                  }
                  else if (elem_type == CFGTYPE_UINT64)
                  {
                     check_range_uint64(info, x, UINT64_MAX);
                     set_value<uint64_t>(first_elem_ptr, elem_index, x);
                  }
                  else
                  {
                     assert(false);
                  }

               }
            }
            else if (elem_type == CFGTYPE_BOOL)
            {
               set_value<bool>(first_elem_ptr, elem_index, StringTk::strToBool(value));
            }
            else if (elem_type == CFGTYPE_STRING)
            {
               set_value<std::string>(first_elem_ptr, elem_index, value);
            }
            else if (elem_type == CFGTYPE_LOGTYPE)
            {
               if (value == "syslog")
                  set_value<LogType>(first_elem_ptr, elem_index, LogType_SYSLOG);
               else if (value == "logfile")
                  set_value<LogType>(first_elem_ptr, elem_index, LogType_LOGFILE);
               else
                  throw InvalidConfigException("Possible values for logType config option: syslog, logfile. Got: '" + value + "'");
            }
            else
            {
               assert(false && "Switch case missing");
            }
         }
      }
   }
   return knownElement;
}

void commonApplyConfigMap(void *containerStruct, StringMap *configMap, ArraySlice<const CfgInfo> cfgInfos, bool enableException, bool addDashes)
{
   for (StringMapIter iter = configMap->begin(); iter != configMap->end();)
   {
      std::string name = iter->first;
      std::string value = iter->second;

      if (name == "tuneDefaultNumStripeNodes")
         // old name, kept for compat, make it alias the new name
         name = "tuneDefaultNumStripeTargets";

      bool knownElement = commonApplyConfigValue(containerStruct, name, value, cfgInfos, addDashes);

      StringMapIter oldIter = iter;
      iter++;

      if (knownElement)
      {
         configMap->erase(oldIter);
      }
      else if (enableException)
      {
         throw InvalidConfigException("The config argument '" + name + "' is invalid");
      }
   }
}

/**
 * Loads a file into a string list (line by line).
 *
 * Loaded strings are trimmed before they are added to the list. Empty lines  and lines starting
 * with STORAGETK_FILE_COMMENT_CHAR are not added to the list.
 */
void CommonConfig::loadStringListFile(const char* filename, StringList& outList)
{
   std::ifstream fis(filename);
   if(!fis.is_open() || fis.fail() )
   {
      throw InvalidConfigException(
         std::string("Failed to open file: ") + filename);
   }

   while(!fis.eof() && !fis.fail() )
   {
      std::string line;

      std::getline(fis, line);
      std::string trimmedLine = StringTk::trim(line);
      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
         outList.push_back(trimmedLine);
   }

   fis.close();
}





