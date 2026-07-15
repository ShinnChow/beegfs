#pragma once

/* This file gets included during codegen as well as the regular build. */

#include <string>
#include <cstdint>

enum CfgType
{
   CFGTYPE_NONE,
   CFGTYPE_STRING,
   CFGTYPE_INT,
   CFGTYPE_UINT,
   CFGTYPE_UINT8,
   CFGTYPE_INT64,
   CFGTYPE_UINT64,
   CFGTYPE_BOOL,
   CFGTYPE_LOGTYPE,
};

static inline const char *cfgtype_string(CfgType cfgtype)
{
   switch (cfgtype)
   {
      case CFGTYPE_NONE: return "CFGTYPE_NONE";
      case CFGTYPE_STRING: return "CFGTYPE_STRING";
      case CFGTYPE_INT: return "CFGTYPE_INT";
      case CFGTYPE_UINT: return "CFGTYPE_UINT";
      case CFGTYPE_UINT8: return "CFGTYPE_UINT8";
      case CFGTYPE_INT64: return "CFGTYPE_INT64";
      case CFGTYPE_UINT64: return "CFGTYPE_UINT64";
      case CFGTYPE_BOOL: return "CFGTYPE_BOOL";
      case CFGTYPE_LOGTYPE: return "CFGTYPE_LOGTYPE";
      default: return "(invalid-cfgtype)";
   }
}


static constexpr uint32_t cfg_get_bits(uint32_t value, uint32_t shift, uint32_t bitcount)
{
   return (value >> shift) & ((1u << bitcount) - 1);
}

static constexpr uint32_t cfg_make_bits(uint32_t value, uint32_t shift, uint32_t bitcount)
{
   if (value >= (1u << bitcount))
   {
      throw "Invalid value for bitmask";
   }
   return value << shift;
}

enum CfgFlag
{
   CFGFLAG_EMPTY_IS_MINUS1 = (1 << 0),  // allow empty strings (value is -1) when parsing integers.
   CFGFLAG_HUMAN_READABLE_SIZE = (1 << 2),  // allow suffixes like K, M, KB, MB, etc.
   CFGFLAG_IS_LIST = (1 << 3),  // std::vector container

   CFGFLAG_IS_TUPLE = (1 << 4),
   CFGFLAG_OBSOLETE_FIELD = (1 << 5),  // Accepted in config files but value ignored.
   // The field exists in an inherited class already; only override the default
   // value but don't generate a new member or getters/setters
   // Ideally there should be no need for this flag. But to remove it, we would
   // need to stop using inheritance for config classes, or at least for the
   // fields that currently use the flag.
   CFGFLAG_OVERRIDE = (1 << 6),

   CFGFLAG_NO_GETTER = (1 << 7),
   CFGFLAG_NO_SETTER = (1 << 8),
};

// 4 bits to indicate the tuple size (number of elements).
// To use a tuple size, CFGFLAG_IS_TUPLE needs to be set.
// If the flag is set, the next 4 bits encode the size of the tuple (allowing
// up to 16 elements).
static constexpr uint32_t CFGFLAG_TUPLESIZE_SHIFT = 16;
static constexpr uint32_t CFGFLAG_TUPLESIZE_BITS = 4;

static constexpr uint32_t cfgflags_get_tuplesize(uint32_t cfgflags)
{
   if ((cfgflags & CFGFLAG_IS_TUPLE) == 0)
      return 1;  // i.e. a unit-tuple
   return cfg_get_bits(cfgflags, CFGFLAG_TUPLESIZE_SHIFT, CFGFLAG_TUPLESIZE_BITS);
}

static inline constexpr uint32_t CFG_TUPLE_TYPE(uint32_t size)
{
   return CFGFLAG_IS_TUPLE | cfg_make_bits(size, CFGFLAG_TUPLESIZE_SHIFT, CFGFLAG_TUPLESIZE_BITS);
}

////////////////////////////////

struct CfgInfo
{
   size_t offset;  // offset of field from start of container struct
   std::string name;
   std::string defaultval;
   CfgType cfgtype;
   uint32_t cfgflags;
};
