include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

META_CODEGEN_SOURCES = \
   ../codegen/GenerateConfigFields.cpp\

META_CODEGEN_OUTPUTS = \
   ../generated/MetaConfigFields.h \
   ../generated/MetaConfigFields.inc

$(call build-codegen,\
   ../build/MetaConfigFields_generate,\
   $(META_CODEGEN_SOURCES),\
   beegfs-base beegfs-codegen,\
   ../source,\
   $(META_CODEGEN_OUTPUTS))

$(call build-static-library,\
   Meta,\
   $(sources),\
   beegfs-base common dl blkid uuid nl3-route,\
   ../source,\
   $(META_CODEGEN_OUTPUTS))

$(call define-dep-lib,\
   Meta,\
   -I ../source,\
   $(build_dir)/libMeta.a)

$(call build-executable,\
   beegfs-meta,\
   $(main),\
   beegfs-base Meta common dl blkid uuid nl3-route,\
   ,\
   $(META_CODEGEN_OUTPUTS))

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   beegfs-base Meta common dl blkid uuid nl3-route,\
   ../tests,\
   $(META_CODEGEN_OUTPUTS))

# enable special reference DirInode debug code
ifneq ($(BEEGFS_DEBUG_RELEASE_DIR),)     # extra release dir debugging
   CXXFLAGS += -DBEEGFS_DEBUG_RELEASE_DIR
endif
