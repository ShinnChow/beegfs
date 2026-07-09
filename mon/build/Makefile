include ../../build/Makefile

main := ../source/app/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

MON_CODEGEN_SOURCES = \
	../codegen/GenerateConfigFields.cpp

MON_CODEGEN_OUTPUTS = \
	../generated/MonConfigFields.h \
	../generated/MonConfigFields.inc

$(call build-codegen,\
   ../build/MonConfigFields_generate,\
   $(MON_CODEGEN_SOURCES),\
   beegfs-base beegfs-codegen common,\
   ../source,\
   $(MON_CODEGEN_OUTPUTS))

$(call build-static-library,\
   Mon,\
   $(sources),\
   beegfs-base common dl curl cassandra nl3-route,\
   ../source,\
   $(MON_CODEGEN_OUTPUTS))

$(call define-dep-lib,\
   Mon,\
   -I ../source,\
   $(build_dir)/libMon.a)

$(call build-executable,\
   beegfs-mon,\
   $(main),\
   beegfs-base Mon common dl curl cassandra nl3-route,\
   ,\
   $(MON_CODEGEN_OUTPUTS))

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   beegfs-base Mon common dl curl cassandra nl3-route,\
   ../tests,\
   $(MON_CODEGEN_OUTPUTS))

