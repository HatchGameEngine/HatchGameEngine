#!/usr/bin/make
# -*- coding: utf-8 -*-
##
## INBOUND, an agnostic software building system
##
## Written by Alexander Nicholi <//nich.fi/>
## Copyright (C) 2024-2025 Aquefir Consulting LLC <//aquefir.co/>
## Released under BSD-2-Clause.
##
## Epilogue file, to be included at the end of your Makefile
##
## This Makefile provides the bodies of a variety of recipes normally
## used in building native executables and libraries. These include:
## debug, release, and sanity check tunings. Using the conventional
## *FILES and *FLAGS Makefile variables, the toolchain program
## variables (like "$(CC)"), the $(PROJECT) variable, and some
## miscellaneous helpers, it will fill out all of the typical details
## for these targets automatically, just by including it in a project's
## main Makefile.
##
## This epilogue should have come with a companion prologue file. If it
## didn't, please obtain the latest version as a full package at
## <//kb.xion.mt/Inbound>. Thank you!
##

# INB_DEBUGLOG : debug.log is where all program output is redirected to
# instead of polluting the console window and potentially slowing down
# compilation. The default is simply "debug.log" in the current working
# directory, but this can be changed using this variable.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was
# customised. The ".O_" prefix denotes "origin" and is to prevent
# naming collisions.
ifeq ($(origin INB_DEBUGLOG),undefined)
.O_INB_DEBUGLOG := DEFAULT
else ifeq ($(origin INB_DEBUGLOG),default)
.O_INB_DEBUGLOG := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_INB_DEBUGLOG := CUSTOM
endif # $(origin INB_DEBUGLOG)

# Set the origin-dependent values of the new variable.
INB_DEBUGLOG.O_DEFAULT := debug.log
INB_DEBUGLOG.O_CUSTOM := $(INB_DEBUGLOG)

# Finally, set the variable.
override INB_DEBUGLOG := $(INB_DEBUGLOG.O_$(.O_INB_DEBUGLOG))

.FN_FILE.S     := \033[32mAssembling
.FN_FILE.C     := \033[34mCompiling
.FN_FILE.CXX   := \033[33mCompiling
.FN_FILE.OBJC  := \033[32mCompiling
.FN_FILE.AR    := \033[32mLinking
.FN_FILE.LD    := \033[31mLinking
.FN_FILE.OCPY  := \033[37mCopying binary of
.FN_FILE.STRIP := \033[31mStripping
.FN_FILE.FMT   := \033[37mFormatting
.FN_FILE.MID   := \033[35mProcessing
.FN_FILE.PCM   := \033[35mAssembling
.FN_FILE.IMG   := \033[35mTransmogrifying
.FN_FILE.BIN   := \033[35mProcessing

.FN_FILE = @$(ECHO) -e " $(.FN_FILE.$(1))\033[0m \033[1m$(2)\033[0m ..."
.FN_FILENOAT = $(ECHO) -e " $(.FN_FILE.$(1))\033[0m \033[1m$(2)\033[0m ..."

## Construct the *FLAGS variables normally.

# ASFLAGS
.L_ASFLAGS := $(ASFLAGS.$(TP))
ifeq ($(origin ASFLAGS),undefined)
# nop
else ifeq ($(origin ASFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_ASFLAGS += $(ASFLAGS)
endif

# CFLAGS
.L_CFLAGS := $(CFLAGS.ANY.$(TP))
ifeq ($(origin CFLAGS),undefined)
# nop
else ifeq ($(origin CFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_CFLAGS += $(CFLAGS)
endif

# CXXFLAGS
.L_CXXFLAGS := $(CXXFLAGS.ANY.$(TP))
ifeq ($(origin CXXFLAGS),undefined)
# nop
else ifeq ($(origin CXXFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_CXXFLAGS += $(CXXFLAGS)
endif

# OBJCFLAGS
.L_OBJCFLAGS := $(OBJCFLAGS.ANY.$(TP))
ifeq ($(origin OBJCFLAGS),undefined)
# nop
else ifeq ($(origin OBJCFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_OBJCFLAGS += $(OBJCFLAGS)
endif

# ARFLAGS
.L_ARFLAGS := $(ARFLAGS.ANY.$(TP))
ifeq ($(origin ARFLAGS),undefined)
# nop
else ifeq ($(origin ARFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_ARFLAGS += $(ARFLAGS)
endif

# LDFLAGS
.L_LDFLAGS := $(LDFLAGS.ANY.$(TP))
ifeq ($(origin LDFLAGS),undefined)
# nop
else ifeq ($(origin LDFLAGS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_LDFLAGS += $(LDFLAGS)
endif

# SYNDEFS
.L_SYNDEFS := $(SYNDEFS.$(TP))
ifeq ($(origin SYNDEFS),undefined)
# nop
else ifeq ($(origin SYNDEFS),default)
# nop
else
# environment [override], file, command line, override, automatic
.L_SYNDEFS += $(SYNDEFS)
endif

## Override the *FLAGS variables if requested and such *FLAGS are
## nonempty

ifeq ($(INB_OVERRIDE),1)

ifeq ($(origin ASFLAGS),undefined)
# nop
else ifeq ($(origin ASFLAGS),default)
# nop
else ifeq ($(origin ASFLAGS),command line)
# nop
else ifeq ($(strip $(ASFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_ASFLAGS := $(ASFLAGS)
endif

ifeq ($(origin CFLAGS),undefined)
# nop
else ifeq ($(origin CFLAGS),default)
# nop
else ifeq ($(origin CFLAGS),command line)
# nop
else ifeq ($(strip $(CFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_CFLAGS := $(CFLAGS)
endif

ifeq ($(origin CXXFLAGS),undefined)
# nop
else ifeq ($(origin CXXFLAGS),default)
# nop
else ifeq ($(origin CXXFLAGS),command line)
# nop
else ifeq ($(strip $(CXXFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_CXXFLAGS := $(CXXFLAGS)
endif

ifeq ($(origin OBJCFLAGS),undefined)
# nop
else ifeq ($(origin OBJCFLAGS),default)
# nop
else ifeq ($(origin OBJCFLAGS),command line)
# nop
else ifeq ($(strip $(OBJCFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_OBJCFLAGS := $(OBJCFLAGS)
endif

ifeq ($(origin ARFLAGS),undefined)
# nop
else ifeq ($(origin ARFLAGS),default)
# nop
else ifeq ($(origin ARFLAGS),command line)
# nop
else ifeq ($(strip $(ARFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_ARFLAGS := $(ARFLAGS)
endif

ifeq ($(origin LDFLAGS),undefined)
# nop
else ifeq ($(origin LDFLAGS),default)
# nop
else ifeq ($(origin LDFLAGS),command line)
# nop
else ifeq ($(strip $(LDFLAGS)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_LDFLAGS := $(LDFLAGS)
endif

endif # $(INB_OVERRIDE)

# Finally, set the variables.
ASFLAGS   := $(.L_ASFLAGS)
CFLAGS    := $(.L_CFLAGS)
CXXFLAGS  := $(.L_CXXFLAGS)
OBJCFLAGS := $(.L_OBJCFLAGS)
ARFLAGS   := $(.L_ARFLAGS)
LDFLAGS   := $(.L_LDFLAGS)
SYNDEFS   := $(.L_SYNDEFS)

## Add built-in libs and #includes

.K_LIBDIRS.DARWIN86.AGBHB    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.DARWIN86.AGBSP    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.DARWIN86.DARWIN86 := /usr/local/lib
.K_LIBDIRS.DARWIN86.DARWINM1 := /usr/local/lib
.K_LIBDIRS.DARWIN86.FREEBSD  :=
.K_LIBDIRS.DARWIN86.IBMPC    := /opt/watcom/lib286
.K_LIBDIRS.DARWIN86.ILLUMOS  :=
.K_LIBDIRS.DARWIN86.LINUX32  :=
.K_LIBDIRS.DARWIN86.LINUX64  :=
.K_LIBDIRS.DARWIN86.OPENBSD  :=
.K_LIBDIRS.DARWIN86.PCDOS    := /opt/watcom/lib286
.K_LIBDIRS.DARWIN86.WIN311   := /opt/watcom/lib286
.K_LIBDIRS.DARWIN86.WIN95    :=
.K_LIBDIRS.DARWIN86.WINNT32  :=
.K_LIBDIRS.DARWIN86.WINNT64  :=

.K_LIBS.DARWIN86.AGBHB    :=
.K_LIBS.DARWIN86.AGBSP    :=
.K_LIBS.DARWIN86.DARWIN86 :=
.K_LIBS.DARWIN86.DARWINM1 :=
.K_LIBS.DARWIN86.FREEBSD  :=
.K_LIBS.DARWIN86.IBMPC    :=
.K_LIBS.DARWIN86.ILLUMOS  :=
.K_LIBS.DARWIN86.LINUX32  :=
.K_LIBS.DARWIN86.LINUX64  :=
.K_LIBS.DARWIN86.OPENBSD  :=
.K_LIBS.DARWIN86.PCDOS    :=
.K_LIBS.DARWIN86.WIN311   :=
.K_LIBS.DARWIN86.WIN95    :=
.K_LIBS.DARWIN86.WINNT32  :=
.K_LIBS.DARWIN86.WINNT64  :=

.K_INCLUDES.DARWIN86.AGBHB    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.DARWIN86.AGBSP    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.DARWIN86.DARWIN86 := /usr/local/include
.K_INCLUDES.DARWIN86.DARWINM1 := /usr/local/include
.K_INCLUDES.DARWIN86.FREEBSD  :=
.K_INCLUDES.DARWIN86.IBMPC    := /opt/watcom/h
.K_INCLUDES.DARWIN86.ILLUMOS  :=
.K_INCLUDES.DARWIN86.LINUX32  :=
.K_INCLUDES.DARWIN86.LINUX64  :=
.K_INCLUDES.DARWIN86.OPENBSD  :=
.K_INCLUDES.DARWIN86.PCDOS    := /opt/watcom/h
.K_INCLUDES.DARWIN86.WIN311   := /opt/watcom/h
.K_INCLUDES.DARWIN86.WIN95    :=
.K_INCLUDES.DARWIN86.WINNT32  :=
.K_INCLUDES.DARWIN86.WINNT64  :=

.K_LIBDIRS.DARWINM1.AGBHB    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.DARWINM1.AGBSP    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.DARWINM1.DARWIN86 := /opt/homebrew/lib
.K_LIBDIRS.DARWINM1.DARWINM1 := /opt/homebrew/lib
.K_LIBDIRS.DARWINM1.FREEBSD  :=
.K_LIBDIRS.DARWINM1.IBMPC    := /opt/watcom/lib286
.K_LIBDIRS.DARWINM1.ILLUMOS  :=
.K_LIBDIRS.DARWINM1.LINUX32  :=
.K_LIBDIRS.DARWINM1.LINUX64  :=
.K_LIBDIRS.DARWINM1.OPENBSD  :=
.K_LIBDIRS.DARWINM1.PCDOS    := /opt/watcom/lib286
.K_LIBDIRS.DARWINM1.WIN311   := /opt/watcom/lib286
.K_LIBDIRS.DARWINM1.WIN95    :=
.K_LIBDIRS.DARWINM1.WINNT32  :=
.K_LIBDIRS.DARWINM1.WINNT64  :=

.K_LIBS.DARWINM1.AGBHB    :=
.K_LIBS.DARWINM1.AGBSP    :=
.K_LIBS.DARWINM1.DARWIN86 :=
.K_LIBS.DARWINM1.DARWINM1 :=
.K_LIBS.DARWINM1.FREEBSD  :=
.K_LIBS.DARWINM1.IBMPC    :=
.K_LIBS.DARWINM1.ILLUMOS  :=
.K_LIBS.DARWINM1.LINUX32  :=
.K_LIBS.DARWINM1.LINUX64  :=
.K_LIBS.DARWINM1.OPENBSD  :=
.K_LIBS.DARWINM1.PCDOS    :=
.K_LIBS.DARWINM1.WIN311   :=
.K_LIBS.DARWINM1.WIN95    :=
.K_LIBS.DARWINM1.WINNT32  :=
.K_LIBS.DARWINM1.WINNT64  :=

.K_INCLUDES.DARWINM1.AGBHB    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.DARWINM1.AGBSP    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.DARWINM1.DARWIN86 := /opt/homebrew/include
.K_INCLUDES.DARWINM1.DARWINM1 := /opt/homebrew/include
.K_INCLUDES.DARWINM1.FREEBSD  :=
.K_INCLUDES.DARWINM1.IBMPC    := /opt/watcom/h
.K_INCLUDES.DARWINM1.ILLUMOS  :=
.K_INCLUDES.DARWINM1.LINUX32  :=
.K_INCLUDES.DARWINM1.LINUX64  :=
.K_INCLUDES.DARWINM1.OPENBSD  :=
.K_INCLUDES.DARWINM1.PCDOS    := /opt/watcom/h
.K_INCLUDES.DARWINM1.WIN311   := /opt/watcom/h
.K_INCLUDES.DARWINM1.WIN95    :=
.K_INCLUDES.DARWINM1.WINNT32  :=
.K_INCLUDES.DARWINM1.WINNT64  :=

.K_LIBDIRS.LINUX64.AGBHB    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.LINUX64.AGBSP    := /opt/devkitpro/devkitARM/lib
.K_LIBDIRS.LINUX64.DARWIN86 :=
.K_LIBDIRS.LINUX64.DARWINM1 :=
.K_LIBDIRS.LINUX64.FREEBSD  :=
.K_LIBDIRS.LINUX64.IBMPC    := /opt/watcom/lib286
.K_LIBDIRS.LINUX64.ILLUMOS  :=
.K_LIBDIRS.LINUX64.LINUX32  :=
.K_LIBDIRS.LINUX64.LINUX64  :=
.K_LIBDIRS.LINUX64.OPENBSD  :=
.K_LIBDIRS.LINUX64.PCDOS    := /opt/watcom/lib286
.K_LIBDIRS.LINUX64.WIN311   := /opt/watcom/lib286
.K_LIBDIRS.LINUX64.WIN95    :=
.K_LIBDIRS.LINUX64.WINNT32  :=
.K_LIBDIRS.LINUX64.WINNT64  :=

.K_LIBS.LINUX64.AGBHB    :=
.K_LIBS.LINUX64.AGBSP    :=
.K_LIBS.LINUX64.DARWIN86 :=
.K_LIBS.LINUX64.DARWINM1 :=
.K_LIBS.LINUX64.FREEBSD  :=
.K_LIBS.LINUX64.IBMPC    :=
.K_LIBS.LINUX64.ILLUMOS  :=
.K_LIBS.LINUX64.LINUX32  :=
.K_LIBS.LINUX64.LINUX64  :=
.K_LIBS.LINUX64.OPENBSD  :=
.K_LIBS.LINUX64.PCDOS    :=
.K_LIBS.LINUX64.WIN311   :=
.K_LIBS.LINUX64.WIN95    :=
.K_LIBS.LINUX64.WINNT32  :=
.K_LIBS.LINUX64.WINNT64  :=

.K_INCLUDES.LINUX64.AGBHB    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.LINUX64.AGBSP    := /opt/devkitpro/devkitARM/include
.K_INCLUDES.LINUX64.DARWIN86 :=
.K_INCLUDES.LINUX64.DARWINM1 :=
.K_INCLUDES.LINUX64.FREEBSD  :=
.K_INCLUDES.LINUX64.IBMPC    := /opt/watcom/h
.K_INCLUDES.LINUX64.ILLUMOS  :=
.K_INCLUDES.LINUX64.LINUX32  :=
.K_INCLUDES.LINUX64.LINUX64  :=
.K_INCLUDES.LINUX64.OPENBSD  :=
.K_INCLUDES.LINUX64.PCDOS    := /opt/watcom/h
.K_INCLUDES.LINUX64.WIN311   := /opt/watcom/h
.K_INCLUDES.LINUX64.WIN95    :=
.K_INCLUDES.LINUX64.WINNT32  :=
.K_INCLUDES.LINUX64.WINNT64  :=

## Finalise flags and quasi-flags into their invocation-ready form.

.K_LIB := $(patsubst %,-L%,$(LIBDIRS) $(SYSROOT)/lib \
	$(.K_LIBDIRS.$(HP).$(TP))) $(patsubst %,-l%,$(LIBS) \
	$(.K_LIBS.$(HP).$(TP)))

.K_FWORK := $(patsubst %,-F%,$(FWORKDIRS)) $(patsubst \
	%,-framework %,$(FWORKS))

ifeq ($(TP),IBMPC)
.K_INCLUDE := -i=$(SYSROOT)/include $(patsubst %,-i=%,$(INCLUDES) \
	$(SYSROOT)/lib $(.K_INCLUDES.$(HP).$(TP)) $(INCLUDEL))
.K_ASINCLUDE := $(.K_INCLUDE)
.K_DEFINE := $(patsubst %,-d%,$(DEFINES) _CFGOPT_FOO) \
	$(patsubst %,-d_SYNDEF_%,$(SYNDEFS))
.K_ASDEFINE := $(.K_DEFINE)
else ifeq ($(TP),PCDOS)
.K_INCLUDE := -i=$(SYSROOT)/include $(patsubst %,-i=%,$(INCLUDES) \
	$(SYSROOT)/lib $(.K_INCLUDES.$(HP).$(TP)) $(INCLUDEL))
.K_ASINCLUDE := $(.K_INCLUDE)
.K_DEFINE := $(patsubst %,-d%,$(DEFINES) _CFGOPT_FOO) \
	$(patsubst %,-d_SYNDEF_%,$(SYNDEFS))
.K_ASDEFINE := $(.K_DEFINE)
else ifeq ($(TP),WIN311)
.K_INCLUDE := -i=$(SYSROOT)/include $(patsubst %,-i=%,$(INCLUDES) \
	$(SYSROOT)/lib $(.K_INCLUDES.$(HP).$(TP)) $(INCLUDEL))
.K_ASINCLUDE := $(.K_INCLUDE)
.K_DEFINE := $(patsubst %,-d%,$(DEFINES) _CFGOPT_FOO) \
	$(patsubst %,-d_SYNDEF_%,$(SYNDEFS))
.K_ASDEFINE := $(.K_DEFINE)
else
.K_INCLUDE := -isystem $(TROOT)/include $(patsubst %,-isystem \
	%,$(INCLUDES) $(SYSROOT)/lib $(.K_INCLUDES.$(HP).$(TP))) \
	$(patsubst %,-iquote %,$(INCLUDEL))
.K_ASINCLUDE := -I$(TROOT)/include $(patsubst %,-I%,$(INCLUDES) \
	$(SYSROOT)/lib $(.K_INCLUDES.$(HP).$(TP)) $(INCLUDEL))
.K_DEFINE := \
	$(patsubst %,-D%,$(DEFINES) _CFGOPT_FOO) \
	$(patsubst %,-D_SYNDEF_%,$(SYNDEFS) FOO)
.K_ASDEFINE := \
	$(patsubst %,--defsym %=1,$(DEFINES) _CFGOPT_FOO) \
	$(patsubst %,--defsym _SYNDEF_%=1,$(SYNDEFS) FOO)
endif

## Name the targets.

# Normalise the desired targets first.

.L_EXEFILE := 0
ifeq ($(origin EXEFILE),undefined)
# nop
else ifeq ($(origin EXEFILE),default)
# nop
else ifeq ($(origin EXEFILE),command line)
# nop
else ifeq ($(strip $(EXEFILE)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_EXEFILE := 1
endif

.L_SOFILE := 0
ifeq ($(origin SOFILE),undefined)
# nop
else ifeq ($(origin SOFILE),default)
# nop
else ifeq ($(origin SOFILE),command line)
# nop
else ifeq ($(strip $(SOFILE)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_SOFILE := 1
endif

.L_AFILE := 0
ifeq ($(origin AFILE),undefined)
# nop
else ifeq ($(origin AFILE),default)
# nop
else ifeq ($(origin AFILE),command line)
# nop
else ifeq ($(strip $(AFILE)),)
# nop
else
# environment [override], file, override, automatic
# not empty
.L_AFILE := 1
endif

# Populated below.
.L_TARGETS :=

# specify all target filenames
.L_EXETARGET  := $(PROJECT)$(EXE)
.L_SOTARGET   := lib$(PROJECT)$(SO)
.L_ATARGET    := lib$(PROJECT)$(A)

ifeq ($(TP),AGBHB)
ifeq ($(.L_EXEFILE),1)
.L_TARGETS += $(.L_EXETARGET)
endif
ifeq ($(.L_AFILE),1)
.L_TARGETS += $(.L_ATARGET)
endif
else ifeq ($(TP),AGBSP)
ifeq ($(.L_EXEFILE),1)
.L_TARGETS += $(.L_EXETARGET)
endif
ifeq ($(.L_AFILE),1)
.L_TARGETS += $(.L_ATARGET)
endif
else ifeq ($(TP),IBMPC)
ifeq ($(.L_EXEFILE),1)
.L_TARGETS += $(.L_EXETARGET)
endif
ifeq ($(.L_AFILE),1)
.L_TARGETS += $(.L_ATARGET)
endif
else ifeq ($(TP),PCDOS)
ifeq ($(.L_EXEFILE),1)
.L_TARGETS += $(.L_EXETARGET)
endif
ifeq ($(.L_AFILE),1)
.L_TARGETS += $(.L_ATARGET)
endif
else # platforms with shared library support
ifeq ($(.L_EXEFILE),1)
.L_TARGETS += $(.L_EXETARGET)
endif
ifeq ($(.L_SOFILE),1)
.L_TARGETS += $(.L_SOTARGET)
endif
ifeq ($(.L_AFILE),1)
.L_TARGETS += $(.L_ATARGET)
endif
endif

# always used by make clean
.L_ALLTARGETS := \
	lib$(PROJECT).so \
	lib$(PROJECT).dylib \
	lib$(PROJECT).dll \
	lib$(PROJECT).a \
	lib$(PROJECT).lib \
	$(PROJECT).elf \
	$(PROJECT).com \
	$(PROJECT).exe \
	$(PROJECT)

.L_OFILES.COMMON := \
	$(SFILES:.s=.s.o) \
	$(CFILES:.c=.c.o) \
	$(CPPFILES:.cpp=.cpp.o) \
	$(MFILES:.m=.m.o) \
	$(SNIPFILES:.snip=.snip.o) \
	$(MAPFILES:.map=.map.o) \
	$(MAPBFILES:.mapb=.mapb.o) \
	$(BSAFILES:.bsa=.bsa.o) \
	$(BSETFILES:.bset=.bset.o) \
	$(JASCFILES:.jasc=.jasc.o) \
	$(IMGFILES:.png=.png.o)

.L_OFILES.AGBHB := \
	$(SFILES.AGBHB:.s=.s.o) \
	$(CFILES.AGBHB:.c=.c.o) \
	$(CPPFILES.AGBHB:.cpp=.cpp.o) \
	$(SNIPFILES.AGBHB:.snip=.snip.o) \
	$(MAPFILES.AGBHB:.map=.map.o) \
	$(MAPBFILES.AGBHB:.mapb=.mapb.o) \
	$(BSAFILES.AGBHB:.bsa=.bsa.o) \
	$(BSETFILES.AGBHB:.bset=.bset.o) \
	$(JASCFILES.AGBHB:.jasc=.jasc.o) \
	$(IMGFILES.AGBHB:.png=.png.o)

.L_OFILES.AGBSP := \
	$(SFILES.AGBSP:.s=.s.o) \
	$(CFILES.AGBSP:.c=.c.o) \
	$(CPPFILES.AGBSP:.cpp=.cpp.o) \
	$(SNIPFILES.AGBSP:.snip=.snip.o) \
	$(MAPFILES.AGBSP:.map=.map.o) \
	$(MAPBFILES.AGBSP:.mapb=.mapb.o) \
	$(BSAFILES.AGBSP:.bsa=.bsa.o) \
	$(BSETFILES.AGBSP:.bset=.bset.o) \
	$(JASCFILES.AGBSP:.jasc=.jasc.o) \
	$(IMGFILES.AGBSP:.png=.png.o)

.L_OFILES.DARWIN86 := \
	$(SFILES.DARWIN86:.s=.s.o) \
	$(CFILES.DARWIN86:.c=.c.o) \
	$(CPPFILES.DARWIN86:.cpp=.cpp.o) \
	$(MFILES.DARWIN86:.m=.m.o)

.L_OFILES.DARWINM1 := \
	$(SFILES.DARWINM1:.s=.s.o) \
	$(CFILES.DARWINM1:.c=.c.o) \
	$(CPPFILES.DARWINM1:.cpp=.cpp.o) \
	$(MFILES.DARWINM1:.m=.m.o)

.L_OFILES.FREEBSD := \
	$(SFILES.FREEBSD:.s=.s.o) \
	$(CFILES.FREEBSD:.c=.c.o) \
	$(CPPFILES.FREEBSD:.cpp=.cpp.o)

.L_OFILES.IBMPC := \
	$(SFILES.IBMPC:.s=.s.o) \
	$(CFILES.IBMPC:.c=.c.o) \
	$(CPPFILES.IBMPC:.cpp=.cpp.o)

.L_OFILES.ILLUMOS := \
	$(SFILES.ILLUMOS:.s=.s.o) \
	$(CFILES.ILLUMOS:.c=.c.o) \
	$(CPPFILES.ILLUMOS:.cpp=.cpp.o)

.L_OFILES.LINUX32 := \
	$(SFILES.LINUX32:.s=.s.o) \
	$(CFILES.LINUX32:.c=.c.o) \
	$(CPPFILES.LINUX32:.cpp=.cpp.o)

.L_OFILES.LINUX64 := \
	$(SFILES.LINUX64:.s=.s.o) \
	$(CFILES.LINUX64:.c=.c.o) \
	$(CPPFILES.LINUX64:.cpp=.cpp.o)

.L_OFILES.OPENBSD := \
	$(SFILES.OPENBSD:.s=.s.o) \
	$(CFILES.OPENBSD:.c=.c.o) \
	$(CPPFILES.OPENBSD:.cpp=.cpp.o)

.L_OFILES.PCDOS := \
	$(SFILES.PCDOS:.s=.s.o) \
	$(CFILES.PCDOS:.c=.c.o) \
	$(CPPFILES.PCDOS:.cpp=.cpp.o)

.L_OFILES.WIN311 := \
	$(SFILES.WIN311:.s=.s.o) \
	$(CFILES.WIN311:.c=.c.o) \
	$(CPPFILES.WIN311:.cpp=.cpp.o)

.L_OFILES.WIN95 := \
	$(SFILES.WIN95:.s=.s.o) \
	$(CFILES.WIN95:.c=.c.o) \
	$(CPPFILES.WIN95:.cpp=.cpp.o)

.L_OFILES.WINNT32 := \
	$(SFILES.WINNT32:.s=.s.o) \
	$(CFILES.WINNT32:.c=.c.o) \
	$(CPPFILES.WINNT32:.cpp=.cpp.o)

.L_OFILES.WINNT64 := \
	$(SFILES.WINNT64:.s=.s.o) \
	$(CFILES.WINNT64:.c=.c.o) \
	$(CPPFILES.WINNT64:.cpp=.cpp.o)

.L_OFILES := $(.L_OFILES.COMMON) $(.L_OFILES.$(TP))

# Auto-formatter temporary artefacts.
FMTFILES := $(patsubst %,%.fmt,$(CFILES) $(CPPFILES) $(MFILES) \
	$(PUBHFILES) $(PRVHFILES) $(CFILES.AGBHB) $(CPPFILES.AGBHB) \
	$(CFILES.AGBSP) $(CPPFILES.AGBSP) \
	$(CFILES.DARWIN86) $(CPPFILES.DARWIN86) $(MFILES.DARWIN86) \
	$(CFILES.DARWINM1) $(CPPFILES.DARWINM1) $(MFILES.DARWINM1) \
	$(CFILES.FREEBSD) $(CPPFILES.FREEBSD) \
	$(CFILES.IBMPC) $(CPPFILES.IBMPC) \
	$(CFILES.ILLUMOS) $(CPPFILES.ILLUMOS) \
	$(CFILES.LINUX32) $(CPPFILES.LINUX32) \
	$(CFILES.LINUX64) $(CPPFILES.LINUX64) \
	$(CFILES.OPENBSD) $(CPPFILES.OPENBSD) \
	$(CFILES.PCDOS) $(CPPFILES.PCDOS) \
	$(CFILES.WIN311) $(CPPFILES.WIN311) \
	$(CFILES.WIN95) $(CPPFILES.WIN95) \
	$(CFILES.WINNT32) $(CPPFILES.WINNT32) \
	$(CFILES.WINNT64) $(CPPFILES.WINNT64))

ifneq ($(strip $(CPPFILES)),)
LD := $(CXX)
.K_LIB += -lstdc++ # le sigh
else
LD := $(CC)
endif

## Define the target recipes.

.PHONY: debug release check clean format
# Remove all default implicit rules by emptying the suffixes builtin
# This causes false circular dependencies with multi-dotted file
# extensionsif we don't do this
.SUFFIXES:

## Debug build
## useful for: normal testing, valgrind, LLDB
##
# ensure NDEBUG is undefined
ifeq ($(TP),IBMPC)
debug: .L_DEFINE += -u=NDEBUG
else ifeq ($(TP),PCDOS)
debug: .L_DEFINE += -u=NDEBUG
else ifeq ($(TP),WIN311)
debug: .L_DEFINE += -u=NDEBUG
else
debug: .L_DEFINE += -UNDEBUG
endif
debug: CFLAGS := $(CFLAGS.DEBUG.$(TP)) $(CFLAGS)
debug: CXXFLAGS := $(CXXFLAGS.DEBUG.$(TP)) $(CXXFLAGS)
debug: OBJCFLAGS := $(OBJCFLAGS.DEBUG.$(TP)) $(OBJCFLAGS)
debug: LDFLAGS := $(LDFLAGS.DEBUG.$(TP)) $(LDFLAGS)
# nop out when not used, as $(REALSTRIP) is called unconditionally
debug: REALSTRIP := ':' ; # : is a no-op
debug: $(.L_TARGETS)

## Release build
## useful for: deployment
##
# ensure NDEBUG is defined
ifeq ($(TP),IBMPC)
release: .L_DEFINE += -dNDEBUG=1
release: .L_ASDEFINE += -dNDEBUG=1
else ifeq ($(TP),PCDOS)
release: .L_DEFINE += -dNDEBUG=1
release: .L_ASDEFINE += -dNDEBUG=1
else ifeq ($(TP),WIN311)
release: .L_DEFINE += -dNDEBUG=1
release: .L_ASDEFINE += -dNDEBUG=1
else
release: .L_DEFINE += -DNDEBUG=1
release: .L_ASDEFINE += --defsym NDEBUG=1
endif
release: CFLAGS := $(CFLAGS.RELEASE.$(TP)) $(CFLAGS)
release: CXXFLAGS := $(CXXFLAGS.RELEASE.$(TP)) $(CXXFLAGS)
release: OBJCFLAGS := $(OBJCFLAGS.RELEASE.$(TP)) $(OBJCFLAGS)
release: LDFLAGS := $(LDFLAGS.RELEASE.$(TP)) $(LDFLAGS)
release: REALSTRIP := $(STRIP)
release: $(.L_TARGETS)

## Sanity check build
## useful for: pre-tool bug squashing
##
# ensure NDEBUG is undefined
ifeq ($(TP),IBMPC)
check: .L_DEFINE += -u=NDEBUG
else ifeq ($(TP),PCDOS)
check: .L_DEFINE += -u=NDEBUG
else ifeq ($(TP),WIN311)
check: .L_DEFINE += -u=NDEBUG
else
check: .L_DEFINE += -UNDEBUG
endif
check: CFLAGS := $(CFLAGS.CHECK.$(TP)) $(CFLAGS)
check: CXXFLAGS := $(CXXFLAGS.CHECK.$(TP)) $(CXXFLAGS)
check: OBJCFLAGS := $(OBJCFLAGS.CHECK.$(TP)) $(OBJCFLAGS)
check: LDFLAGS := $(LDFLAGS.CHECK.$(TP)) $(LDFLAGS)
# nop out when not used, as $(REALSTRIP) is called unconditionally
check: REALSTRIP := ':' ; # : is a no-op
check: $(.L_TARGETS)

## Define recipes.

# Fmtfile recipes.

# C
%.c.fmt: %.c
	$(call .FN_FILE,FMT,$<)
	@cat $< | $(FMT) $(FMTFLAGS) > $@
	@mv $@ $<

# C++
%.cpp.fmt: %.cpp
	$(call .FN_FILE,FMT,$<)
	@cat $< | $(FMT) $(FMTFLAGS) > $@
	@mv $@ $<

# Objective-C
%.m.fmt: %.m
	$(call .FN_FILE,FMT,$<)
	@cat $< | $(FMT) $(FMTFLAGS) > $@
	@mv $@ $<

%.h.fmt: %.h
	$(call .FN_FILE,FMT,$<)
	@cat $< | $(FMT) $(FMTFLAGS) > $@
	@mv $@ $<

# Ofile recipes.

# Assembly
%.s.o: %.s
	$(call .FN_FILE,S,$@)
	@$(AS) -o $@ $(ASFLAGS) $(.K_ASDEFINE) $(.K_ASINCLUDE) $<

# C
%.c.o: %.c
	$(call .FN_FILE,C,$@)
	@$(CC) -c -o $@ $(CFLAGS) $(.K_DEFINE) $(.K_INCLUDE) $<

# C++
%.cpp.o: %.cpp
	$(call .FN_FILE,CXX,$@)
	@$(CXX) -c -o $@ $(CXXFLAGS) $(.K_DEFINE) $(.K_INCLUDE) $<

# Objective-C
%.m.o: %.m
	$(call .FN_FILE,OBJC,$@)
	@$(CC) -c -o $@ $(OBJCFLAGS) $(.K_DEFINE) $(.K_INCLUDE) $<

# Static library recipe.

$(.L_ATARGET): $(.L_OFILES)
ifneq ($(strip $(.L_OFILES)),)
	$(call .FN_FILE,AR,$@)
	@$(AR) $(ARFLAGS) $@ $^
endif

# Shared library recipe.

$(.L_SOTARGET): $(.L_OFILES)
ifneq ($(strip $(.L_OFILES)),)
	$(call .FN_FILE,LD,$@)
	@$(LD) $(LDFLAGS) -shared -o $@ $^ $(.K_LIB) $(.K_FWORK)
	$(call .FN_FILE,STRIP,$@)
	@$(REALSTRIP) -s $@
endif

# Executable recipe.

$(.L_EXETARGET): $(.L_OFILES)
ifneq ($(strip $(.L_OFILES)),)
	$(call .FN_FILE,LD,$@)
#	@$(ECHO) $^
	@$(LD) $(LDFLAGS) -o $@ $^ $(.K_LIB) $(.K_FWORK)
	$(call .FN_FILE,STRIP,$@)
	@$(REALSTRIP) -s $@
endif

# Clean the repository.

clean:
	@$(ECHO) -e " -> \033[37mCleaning\033[0m the repository..."
	@$(RM) $(.L_ALLTARGETS)
	@$(RM) $(.L_OFILES.COMMON)
	@$(RM) $(.L_OFILES.AGBHB)
	@$(RM) $(.L_OFILES.AGBSP)
	@$(RM) $(.L_OFILES.DARWIN86)
	@$(RM) $(.L_OFILES.DARWINM1)
	@$(RM) $(.L_OFILES.FREEBSD)
	@$(RM) $(.L_OFILES.IBMPC)
	@$(RM) $(.L_OFILES.ILLUMOS)
	@$(RM) $(.L_OFILES.LINUX32)
	@$(RM) $(.L_OFILES.LINUX64)
	@$(RM) $(.L_OFILES.OPENBSD)
	@$(RM) $(.L_OFILES.PCDOS)
	@$(RM) $(.L_OFILES.WIN311)
	@$(RM) $(.L_OFILES.WIN95)
	@$(RM) $(.L_OFILES.WINNT32)
	@$(RM) $(.L_OFILES.WINNT64)
	@$(RM) $(FMTFILES)

# Auto-format the sources.

format: $(FMTFILES)

# EOF
