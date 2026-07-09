include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

STORAGE_CODEGEN_SOURCES = \
	../codegen/GenerateConfigFields.cpp

STORAGE_CODEGEN_OUTPUTS = \
	../generated/StorageConfigFields.h \
	../generated/StorageConfigFields.inc

$(call build-codegen,\
   ../build/StorageConfigFields_generate,\
   $(STORAGE_CODEGEN_SOURCES),\
   beegfs-base beegfs-codegen common,\
   ../source,\
   $(STORAGE_CODEGEN_OUTPUTS))

$(call build-static-library,\
   Storage,\
   $(sources),\
   beegfs-base common dl blkid uuid nl3-route,\
   ../source,\
   $(STORAGE_CODEGEN_OUTPUTS))

$(call define-dep-lib,\
   Storage,\
   -I ../source,\
   $(build_dir)/libStorage.a)

$(call build-executable,\
   beegfs-storage,\
   $(main),\
   beegfs-base Storage common dl blkid uuid nl3-route,\
   ../source,\
   $(STORAGE_CODEGEN_OUTPUTS))

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   beegfs-base Storage common dl blkid uuid nl3-route,\
   ../tests,\
   $(STORAGE_CODEGEN_OUTPUTS))
