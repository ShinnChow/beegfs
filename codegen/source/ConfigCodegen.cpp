#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <cstdio>
#include <exception>
#include <filesystem> // std::filesystem::path. Can't use common/storage/Path.h here
#include <beegfs-codegen/ConfigCodegen.h>

static inline char uppercase(char c)
{
   if ('a' <= c && c <= 'z')
      c = 'A' + (c - 'a');
   assert('A' <= c && c <= 'Z');
   return c;
}

static bool obsolete_or_override(CfgDef const *def)
{
   return (def->cfgflags & (CFGFLAG_OBSOLETE_FIELD | CFGFLAG_OVERRIDE)) != 0;
}

__attribute__((format(printf, 1, 2)))
static void codegen_log_printf(const char *fmt, ...) // NOLINT
{
   fprintf(stderr, "[CODEGEN] ");
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

__attribute__((format(printf, 1, 2)))
static std::string string_printf(const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_list copy;
   va_start(ap, fmt);
   va_copy(copy, ap);
   int len = vsnprintf(NULL, 0, fmt, copy);
   std::string result;
   result.resize(len);
   vsnprintf(result.data(), result.size() + 1, fmt, ap);
   va_end(copy);
   va_end(ap);
   return result;
}

static inline std::string config_type_string(CfgDef const *def)
{
   uint32_t tuple_size = cfgflags_get_tuplesize(def->cfgflags);
   std::string basic;
   switch (def->cfgtype)
   {
      case CFGTYPE_BOOL: basic = "bool"; break;
      case CFGTYPE_STRING: basic = "std::string"; break;
      case CFGTYPE_INT: basic = "int"; break;
      case CFGTYPE_INT64: basic = "int64_t"; break;
      case CFGTYPE_UINT: basic = "unsigned"; break;
      case CFGTYPE_UINT8: basic = "uint8_t"; break;
      case CFGTYPE_UINT64: basic = "uint64_t"; break;
      case CFGTYPE_LOGTYPE: basic = "LogType"; break;
      default: return "ERROR";
   }
   if (def->cfgflags & CFGFLAG_IS_LIST)
   {
      return string_printf("std::vector<%s>", basic.c_str());
   }
   else if (def->cfgflags & CFGFLAG_IS_TUPLE)
   {
      std::string size_string = std::to_string(tuple_size);
      return string_printf("std::array<%s, %s>", basic.c_str(), size_string.c_str());
   }
   else
   {
      return basic;
   }
}

static std::string stringlit(std::string const& value)
{
   std::string ans;
   ans.push_back('"');
   for (char cc : value)
   {
      int c = (unsigned char) cc;
      if (c < 32 || c >= 127)
      {
         // Octal not hex: \x has no fixed width in C/C++ and would consume a following hex digit.
         // Three-digit octal always terminates after exactly 3 digits.
         ans += { '\\', (char)('0' + (c >> 6)), (char)('0' + ((c >> 3) & 7)), (char)('0' + (c & 7)) };
      }
      else if (c == '"' || c == '\\')
      {
         ans += { '\\', (char) c };
      }
      else
      {
         ans.push_back((char) c);
      }
   }
   ans.push_back('"');
   return ans;
}

enum FileCurrentVerdict
{
   FileCurrentVerdict_Uptodate,
   FileCurrentVerdict_MustReplace,
   FileCurrentVerdict_Error,
};

static FileCurrentVerdict check_file_current(const char *dest_filepath, const char *source_filepath)
{
   FileCurrentVerdict result = FileCurrentVerdict_Uptodate;
   int dest_fd = open(dest_filepath, O_RDONLY);
   int source_fd = open(source_filepath, O_RDONLY);
   size_t buffer_size = 16 * 1024;
   void *dest_buffer = malloc(buffer_size);
   void *source_buffer = malloc(buffer_size);

   if (! dest_buffer || ! source_buffer)
   {
      perror("malloc()");
      abort();
   }

   if (dest_fd == -1)
   {
      result = FileCurrentVerdict_MustReplace;
      goto out;
   }

   if (source_fd == -1)
   {
      codegen_log_printf("ERROR: open(\"%s\"): %s\n", source_filepath, strerror(errno));
      result = FileCurrentVerdict_Error;
      goto out;
   }

   for (;;)
   {
      ssize_t ndest = read(dest_fd, dest_buffer, buffer_size);
      if (ndest == -1)
      {
         codegen_log_printf("ERROR: read() from \"%s\": %s\n", dest_filepath, strerror(errno));
         result = FileCurrentVerdict_Error;
         goto out;
      }

      ssize_t nsource = read(source_fd, source_buffer, buffer_size);
      if (nsource == -1)
      {
         codegen_log_printf("ERROR: read() from \"%s\": %s\n", source_filepath, strerror(errno));
         result = FileCurrentVerdict_Error;
         goto out;
      }

      if (ndest != nsource || memcmp(dest_buffer, source_buffer, ndest) != 0)
      {
         result = FileCurrentVerdict_MustReplace;
         goto out;
      }

      if (ndest == 0)
      {
         // reached end of both files
         goto out;
      }
   }

out:
   if (dest_fd != -1) close(dest_fd);
   if (source_fd != -1) close(source_fd);
   free(dest_buffer);
   free(source_buffer);
   return result;
}

struct FileUpdater
{
   std::string dest_filepath;
   std::string temp_filepath;
   FILE *temp_file = NULL;

   FileUpdater(std::string filepath);
   ~FileUpdater();
};

void fu_init(FileUpdater *u, std::string filepath)
{
   u->dest_filepath = filepath;
   u->temp_filepath = u->dest_filepath + ".tmp";
   u->temp_file = fopen(u->temp_filepath.c_str(), "wb");
   if (! u->temp_file)
      throw std::runtime_error(string_printf("Failed to open file %s", filepath.c_str()));
}

void fu_exit(FileUpdater *u)
{
   if (u->temp_file)
   {
      codegen_log_printf("WARNING: Did not commit %s => %s\n",
            u->temp_filepath.c_str(), u->dest_filepath.c_str());
      fclose(u->temp_file);
      u->temp_file = NULL;
   }
}

// Compares contents of dest file and temp file, and replaces file by renaming
// only if different. Returns false when opening, writing, or renaming files failed.
bool fu_commit(FileUpdater *u)
{
   const char *temp_filepath = u->temp_filepath.c_str();
   const char *dest_filepath = u->dest_filepath.c_str();
   int e = 0;

   assert(u->temp_file);
   if (fclose(u->temp_file) == EOF)
      e = errno;
   u->temp_file = NULL;  // invalidated regardless of error

   if (e)
   {
      codegen_log_printf("ERROR: fclose(%s): %s\n", temp_filepath, strerror(e));
      return false;
   }

   FileCurrentVerdict verdict = check_file_current(dest_filepath, temp_filepath);

   if (verdict == FileCurrentVerdict_Error)
   {
      codegen_log_printf("ERROR: update-check failed with I/O error. NOT REPLACING %s => %s\n",
            temp_filepath, dest_filepath);
      return false;
   }
   else if (verdict == FileCurrentVerdict_MustReplace)
   {
      codegen_log_printf("INFO: recreating %s\n", dest_filepath);
      if (rename(temp_filepath, dest_filepath) == -1)
      {
         e = errno;
         codegen_log_printf("ERROR: Failed to commit: rename(%s, %s) failed: %s\n",
               temp_filepath, dest_filepath, strerror(e));
         return false;
      }
   }
   else
   {
      codegen_log_printf("INFO: %s unchanged\n", u->dest_filepath.c_str());
      if (unlink(temp_filepath) == -1)
      {
         e = errno;
         codegen_log_printf("ERROR: Failed to unlink temp file %s: %s\n",
               temp_filepath, strerror(e));
         return false; // could swallow, but error was already logged
      }
   }
   return true;
}

void fu_printfv(FileUpdater *u, const char *fmt, va_list ap)
{
   FILE *f = u->temp_file;
   if (f)
   {
      vfprintf(f, fmt, ap);
   }
}

__attribute__((format(printf, 2, 3)))
void fu_printf(FileUpdater *u, const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   fu_printfv(u, fmt, ap);
   va_end(ap);
}

__attribute__((format(printf, 2, 3)))
void fu_printf_line(FileUpdater *u, const char *fmt, ...) // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   fu_printfv(u, fmt, ap);
   va_end(ap);
   fu_printf(u, "\n");
}

FileUpdater::FileUpdater(std::string filepath)
{
   fu_init(this, filepath);
}

FileUpdater::~FileUpdater()
{
   fu_exit(this);
}

static void write_config_struct(FileUpdater *fu, const char *classname, CfgDef const *defs, int count)
{
   fu_printf_line(fu, "struct %s {", classname);

   // write fields
   for (CfgDef const *def = defs, *last = defs + count;
         def != last; def++)
   {
      if (obsolete_or_override(def))
         continue;
      std::string name = def->name;
      std::string type = config_type_string(def);
      fu_printf_line(fu, "%s %s;", type.c_str(), name.c_str());
   }

   // write getters
   for (CfgDef const *def = defs, *last = defs + count;
         def != last; def++)
   {
      if (obsolete_or_override(def))
         continue;
      if (def->cfgflags & CFGFLAG_NO_GETTER)
         continue;
      std::string name = def->name;
      assert(name.size() > 0);  // NOLINT
      std::string getterName = std::string("get") + std::string { uppercase(name[0]) } + std::string(name.c_str() + 1, name.size() - 1);
      std::string type = config_type_string(def);
      fu_printf_line(fu, "%s %s() const { return this->%s; }", type.c_str(), getterName.c_str(), name.c_str());
   }

   // write setters
   for (CfgDef const *def = defs, *last = defs + count;
         def != last; def++)
   {
      if (obsolete_or_override(def))
         continue;
      if (def->cfgflags & CFGFLAG_NO_SETTER)
         continue;
      std::string name = def->name;
      assert(name.size() > 0);  // NOLINT
      std::string setterName = std::string("set") + std::string { uppercase(name[0]) } + std::string(name.c_str() + 1, name.size() - 1);
      std::string type = config_type_string(def);
      fu_printf_line(fu, "void %s(%s value) { this->%s = value; };", setterName.c_str(), type.c_str(), name.c_str());
   }

   fu_printf_line(fu, "};");
}

static void write_config_table(FileUpdater *fu, const char *classname,
      CfgDef const *defs, size_t count)
{
   fu_printf_line(fu, "extern CfgInfo const %s_infos[%zu] = {", classname, count);

   for (CfgDef const *def = defs, *last = defs + count;
         def != last; def++)
   {
      std::string name = def->name;
      std::string name_lit = stringlit(def->name);
      std::string defaultval_lit = stringlit(def->defaultval);
      unsigned long cfgtype = def->cfgtype;
      unsigned long cfgflags = def->cfgflags;

      std::string offset_string;
      if (obsolete_or_override(def))
         offset_string = std::string("0");
      else
         offset_string = string_printf("offsetof(%s, %s)", classname, name.c_str());

      fu_printf_line(fu, "{ %s, %s, %s, (CfgType) %lu, %lu },",
            offset_string.data(), name_lit.c_str(), defaultval_lit.c_str(),
            cfgtype, cfgflags);
   }

   fu_printf_line(fu, "};");
}

void generate_config_sources(ConfigFieldsGenerator *fields,
      const char *dirpath, const char *classname)
{
   CfgDef const *defs = fields->cfgDefs.data();
   size_t count = fields->cfgDefs.size();

   std::string header_path = std::string(dirpath) + "/" + classname + ".h";
   std::string def_path = std::string(dirpath) + "/" + classname + ".inc";

   std::filesystem::create_directories(dirpath);

   {
      FileUpdater file_updater(header_path);
      FileUpdater *fu = &file_updater;

      fu_printf_line(fu, "#pragma once");
      fu_printf_line(fu, "#include <cstddef>");
      fu_printf_line(fu, "#include <cstdint>");
      fu_printf_line(fu, "#include <string>");
      fu_printf_line(fu, "#include <vector>");
      fu_printf_line(fu, "#include <array>");
      fu_printf_line(fu, "#include <common/Common.h>");

      write_config_struct(fu, classname, defs, count);

      fu_printf_line(fu, "extern CfgInfo const %s_infos[%zu];", classname, count);

      if (! fu_commit(fu))
         throw std::runtime_error("Codegen failed (declaration header).");
   }

   {
      FileUpdater file_updater(def_path);
      FileUpdater *fu = &file_updater;

      // quoted: the header is this file's sibling, so it is found without -I
      fu_printf_line(fu, "#include \"%s.h\"", classname);

      write_config_table(fu, classname, defs, count);

      if (! fu_commit(fu))
         throw std::runtime_error("Codegen failed (definition include).");
   }
}
