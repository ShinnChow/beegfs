include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

FSCK_CODEGEN_SOURCES = \
   ../codegen/GenerateConfigFields.cpp\

FSCK_CODEGEN_OUTPUTS = \
   ../generated/FsckConfigFields.h \
   ../generated/FsckConfigFields.inc

$(call build-codegen,\
   ../build/FsckConfigFields_generate,\
   $(FSCK_CODEGEN_SOURCES),\
   beegfs-base beegfs-codegen common,\
   ../source,\
   $(FSCK_CODEGEN_OUTPUTS))

$(call build-static-library,\
   Fsck,\
   $(sources),\
   beegfs-base common dl nl3-route,\
   ../source,\
   $(FSCK_CODEGEN_OUTPUTS))

$(call define-dep-lib,\
   Fsck,\
   -I ../source,\
   $(build_dir)/libFsck.a)

$(call build-executable,\
   beegfs-fsck,\
   $(main),\
   beegfs-base Fsck common dl nl3-route,\
   ,\
   $(FSCK_CODEGEN_OUTPUTS))

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   beegfs-base Fsck common dl nl3-route,\
   ../tests,\
   $(FSCK_CODEGEN_OUTPUTS))

CXXFLAGS += -DBOOST_RESULT_OF_USE_DECLTYPE
