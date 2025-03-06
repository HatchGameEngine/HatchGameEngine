#!/usr/bin/make
# -*- coding: utf-8 -*-
##
## INBOUND, an agnostic software building system
##
## Written by Alexander Nicholi <//nich.fi/>
## Copyright (C) 2024-2025 Aquefir Consulting LLC <//aquefir.co/>
## Released under BSD-2-Clause.
##
## Prologue file, to be included at the beginning of your Makefile
##
## This Makefile prologue provides multi-platform build normalisation.
## It explicates paths for toolchain executables, libraries, utility
## programs, and platform-specific errata for an exaustive matrix of
## compilation hosts and targets. It also provides 'synthetic
## definitions', or syndefs for short, that expose platform-specific
## details in a portable way to C code in projects using Inbound. It
## also provides a similar mechanism for 'configuration options', or
## cfgopts, which allow the user building the project to modify the
## compilation in a fully declarative way.
##
## Inbound was originally the v2.0 rewrite of the Slick Makefiles, a
## proof-of-concept created to show that meta-build tools such as CMake,
## SCons and Autotools were not necessary to target the full plethora of
## platforms out there in the world today. Like Slick, so too does the
## Inbound fulfill this promise. It's just plain Make.
##
## For more information, visit <//kb.xion.mt/Inbound>.
##

# Check Make version; we need at least GNU Make 3.82. Fortunately,
# 'undefine' directive has been introduced exactly in GNU Make 3.82.
ifeq ($(filter undefine,$(value .FEATURES)),)
$(error Unsupported Make version. \
The build system does not work properly with GNU Make $(MAKE_VERSION). \
Please use GNU Make 3.82 or later)
endif

# INB_OVERRIDE : If set, epilogue.mk will take user-set *FLAGS
# variables as the entirety of the flags, instead of appending them to
# the default *FLAGS, which is the default behaviour.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was
# customised. The ".O_" prefix denotes "origin" and is to prevent
# naming collisions.
ifeq ($(origin INB_OVERRIDE),undefined)
.O_INB_OVERRIDE := DEFAULT
else ifeq ($(origin INB_OVERRIDE),default)
.O_INB_OVERRIDE := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_INB_OVERRIDE := CUSTOM
endif # $(origin INB_OVERRIDE)

# Set the origin-dependent values of the new variable.
INB_OVERRIDE.O_DEFAULT := 0
INB_OVERRIDE.O_CUSTOM := 1

# Finally, set the variable.
override INB_OVERRIDE := $(INB_OVERRIDE.O_$(.O_INB_OVERRIDE))

## Host platform.

# The ".K_" prefix denotes "[k]onstant" and is to prevent naming
# collisions. Capitalise the result text for use in variable
# interpolation later.
.K_UNAMES := $(shell uname -s | tr 'a-z' 'A-Z')
.K_UNAMEM := $(shell uname -m | tr 'a-z' 'A-Z')

ifeq ($(.K_UNAMES),DARWIN)
ifeq ($(.K_UNAMEM),ARM64)
HP := DARWINM1
else
HP := DARWIN86
endif
else ifeq ($(.K_UNAMES),LINUX)
ifneq ($(.K_UNAMEM),X86_64)
$(error Sorry, Inbound currently only supports Linux on x86_64.)
endif
HP := LINUX64
endif

## Homebrew prefix.

BREW := $(shell command -v brew >/dev/null && { brew --prefix; } || { \
	/bin/echo ///nohomebrew; })
ifeq ($(BREW),///nohomebrew)
ifneq ($(HP),LINUX64)
$(error Homebrew is needed on macOS. Please install it.)
endif
endif

## devkitPro prefix.

DKP := $(shell test -d /opt/devkitpro && { /bin/echo /opt/devkitpro; \
	} || { /bin/echo ///nodevkitpro; })

## OpenWatcom 2 prefix.

WAT := $(shell test -d /opt/watcom && { /bin/echo /opt/watcom; } || { \
	/bin/echo ///nowatcom; })

## Target platform.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was
# customised. The ".O_" prefix denotes "origin" and is to prevent
# naming collisions.
ifeq ($(origin TP),undefined)
.O_TP := DEFAULT
else ifeq ($(origin TP),default)
.O_TP := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_TP := CUSTOM
endif # $(origin TP)

# Set the origin-dependent values of the new variable.
TP.O_DEFAULT := $(HP)
TP.O_CUSTOM := $(TP)

# Finally, set the variable.
override TP := $(TP.O_$(.O_TP))

## Target sysroot.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was
# customised. The ".O_" prefix denotes "origin" and is to prevent
# naming collisions.
ifeq ($(origin SYSROOT),undefined)
.O_SYSROOT := DEFAULT
else ifeq ($(origin SYSROOT),default)
.O_SYSROOT := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_SYSROOT := CUSTOM
endif # $(origin SYSROOT)

# Set the host-target dependent values of the new variable.
# Follows the form $(SYSROOT.$(HP).$(TP)).
SYSROOT.DARWIN86.AGBHB    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.DARWIN86.AGBSP    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.DARWIN86.DARWIN86 := $(BREW)/opt/llvm
SYSROOT.DARWIN86.DARWINM1 := $(BREW)/opt/llvm
SYSROOT.DARWIN86.FREEBSD  := $(BREW)/opt/llvm
SYSROOT.DARWIN86.IBMPC    := $(WAT)
SYSROOT.DARWIN86.ILLUMOS  := $(BREW)/opt/llvm
SYSROOT.DARWIN86.LINUX32  := $(BREW)/opt/llvm
SYSROOT.DARWIN86.LINUX64  := $(BREW)/opt/llvm
SYSROOT.DARWIN86.OPENBSD  := $(BREW)/opt/llvm
SYSROOT.DARWIN86.PCDOS    := $(WAT)
SYSROOT.DARWIN86.WIN311   := $(WAT)
SYSROOT.DARWIN86.WIN95    := $(BREW)/opt/mingw-w64/toolchain-i686/mingw
SYSROOT.DARWIN86.WINNT32  := $(BREW)/opt/mingw-w64/toolchain-i686/mingw
SYSROOT.DARWIN86.WINNT64  := $(BREW)/opt/mingw-w64/toolchain-x86_64/mingw

SYSROOT.DARWINM1.AGBHB    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.DARWINM1.AGBSP    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.DARWINM1.DARWIN86 := $(BREW)/opt/llvm
SYSROOT.DARWINM1.DARWINM1 := $(BREW)/opt/llvm
SYSROOT.DARWINM1.FREEBSD  := $(BREW)/opt/llvm
SYSROOT.DARWINM1.IBMPC    := $(WAT)
SYSROOT.DARWINM1.ILLUMOS  := $(BREW)/opt/llvm
SYSROOT.DARWINM1.LINUX32  := $(BREW)/opt/llvm
SYSROOT.DARWINM1.LINUX64  := $(BREW)/opt/llvm
SYSROOT.DARWINM1.OPENBSD  := $(BREW)/opt/llvm
SYSROOT.DARWINM1.PCDOS    := $(WAT)
SYSROOT.DARWINM1.WIN311   := $(WAT)
SYSROOT.DARWINM1.WIN95    := $(BREW)/opt/mingw-w64/toolchain-i686/mingw
SYSROOT.DARWINM1.WINNT32  := $(BREW)/opt/mingw-w64/toolchain-i686/mingw
SYSROOT.DARWINM1.WINNT64  := $(BREW)/opt/mingw-w64/toolchain-x86_64/mingw

SYSROOT.LINUX64.AGBHB    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.LINUX64.AGBSP    := $(DKP)/devkitARM/arm-none-eabi
SYSROOT.LINUX64.DARWIN86 := "Darwin cannot be targeted from GNU/Linux."
SYSROOT.LINUX64.DARWINM1 := "Darwin cannot be targeted from GNU/Linux."
SYSROOT.LINUX64.FREEBSD  := /usr
SYSROOT.LINUX64.IBMPC    := $(WAT)
SYSROOT.LINUX64.ILLUMOS  := /usr
SYSROOT.LINUX64.LINUX32  := /usr
SYSROOT.LINUX64.LINUX64  := /usr
SYSROOT.LINUX64.OPENBSD  := /usr
SYSROOT.LINUX64.PCDOS    := $(WAT)
SYSROOT.LINUX64.WIN311   := $(WAT)
SYSROOT.LINUX64.WIN95    := /usr/x86_64-w64-mingw32
SYSROOT.LINUX64.WINNT32  := /usr/x86_64-w64-mingw32
SYSROOT.LINUX64.WINNT64  := /usr/x86_64-w64-mingw32

# Set the origin-dependent values of the new variable.
SYSROOT.O_DEFAULT := $(SYSROOT.$(HP).$(TP))
SYSROOT.O_CUSTOM := $(SYSROOT)

# Finally, set the variable.
override SYSROOT := $(SYSROOT.O_$(.O_SYSROOT))

## Toolchain programs.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was
# customised. The ".O_" prefix denotes "origin" and is to prevent
# naming collisions.
ifeq ($(origin AS),undefined)
.O_AS := DEFAULT
else ifeq ($(origin AS),default)
.O_AS := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_AS := CUSTOM
endif # $(origin AS)

ifeq ($(origin CC),undefined)
.O_CC := DEFAULT
else ifeq ($(origin CC),default)
.O_CC := DEFAULT
else
.O_CC := CUSTOM
endif # $(origin CC)

ifeq ($(origin CXX),undefined)
.O_CXX := DEFAULT
else ifeq ($(origin CXX),default)
.O_CXX := DEFAULT
else
.O_CXX := CUSTOM
endif # $(origin CXX)

ifeq ($(origin AR),undefined)
.O_AR := DEFAULT
else ifeq ($(origin AR),default)
.O_AR := DEFAULT
else
.O_AR := CUSTOM
endif # $(origin AR)

ifeq ($(origin LD),undefined)
.O_LD := DEFAULT
else ifeq ($(origin LD),default)
.O_LD := DEFAULT
else
.O_LD := CUSTOM
endif # $(origin LD)

ifeq ($(origin OCPY),undefined)
.O_OCPY := DEFAULT
else ifeq ($(origin OCPY),default)
.O_OCPY := DEFAULT
else
.O_OCPY := CUSTOM
endif # $(origin OCPY)

ifeq ($(origin STRIP),undefined)
.O_STRIP := DEFAULT
else ifeq ($(origin STRIP),default)
.O_STRIP := DEFAULT
else
.O_STRIP := CUSTOM
endif # $(origin STRIP)

ifeq ($(origin PL),undefined)
.O_PL := DEFAULT
else ifeq ($(origin PL),default)
.O_PL := DEFAULT
else
.O_PL := CUSTOM
endif # $(origin PL)

ifeq ($(origin PY),undefined)
.O_PY := DEFAULT
else ifeq ($(origin PY),default)
.O_PY := DEFAULT
else
.O_PY := CUSTOM
endif # $(origin PY)

ifeq ($(origin FMT),undefined)
.O_FMT := DEFAULT
else ifeq ($(origin FMT),default)
.O_FMT := DEFAULT
else
.O_FMT := CUSTOM
endif # $(origin FMT)

ifeq ($(origin LINT),undefined)
.O_LINT := DEFAULT
else ifeq ($(origin LINT),default)
.O_LINT := DEFAULT
else
.O_LINT := CUSTOM
endif # $(origin LINT)

ifeq ($(origin INSTALL),undefined)
.O_INSTALL := DEFAULT
else ifeq ($(origin INSTALL),default)
.O_INSTALL := DEFAULT
else
.O_INSTALL := CUSTOM
endif # $(origin INSTALL)

ifeq ($(origin ECHO),undefined)
.O_ECHO := DEFAULT
else ifeq ($(origin ECHO),default)
.O_ECHO := DEFAULT
else
.O_ECHO := CUSTOM
endif # $(origin ECHO)

ifeq ($(origin CP),undefined)
.O_CP := DEFAULT
else ifeq ($(origin CP),default)
.O_CP := DEFAULT
else
.O_CP := CUSTOM
endif # $(origin CP)

# Darwin Intel host

# GBA homebrew
AS.DARWIN86.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.DARWIN86.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.DARWIN86.AGBHB   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.DARWIN86.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.DARWIN86.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.DARWIN86.AGBHB  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.DARWIN86.AGBHB := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# GBA sourcepatching
AS.DARWIN86.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.DARWIN86.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.DARWIN86.AGBSP   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.DARWIN86.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.DARWIN86.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.DARWIN86.AGBSP  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.DARWIN86.AGBSP := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# Native
AS.DARWIN86.DARWIN86    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.DARWIN86    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.DARWIN86   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.DARWIN86    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.DARWIN86    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.DARWIN86  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.DARWIN86 := $(BREW)/opt/llvm/bin/llvm-strip

# macOS on AArch64
AS.DARWIN86.DARWINM1    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.DARWINM1    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.DARWINM1   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.DARWINM1    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.DARWINM1    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.DARWINM1  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.DARWINM1 := $(BREW)/opt/llvm/bin/llvm-strip

# FreeBSD
AS.DARWIN86.FREEBSD    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.FREEBSD    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.FREEBSD   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.FREEBSD    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.FREEBSD    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.FREEBSD  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.FREEBSD := $(BREW)/opt/llvm/bin/llvm-strip

# IBM-PC direct (not DOS-specific)
AS.DARWIN86.IBMPC    := $(WAT)/bino64/wasm
CC.DARWIN86.IBMPC    := $(WAT)/bino64/wcc
CXX.DARWIN86.IBMPC   := $(WAT)/bino64/wpp
AR.DARWIN86.IBMPC    := $(WAT)/bino64/wlib
LD.DARWIN86.IBMPC    := $(WAT)/bino64/wlink
STRIP.DARWIN86.IBMPC := $(WAT)/bino64/wstrip

# illumos/OpenIndiana
AS.DARWIN86.ILLUMOS    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.ILLUMOS    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.ILLUMOS   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.ILLUMOS    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.ILLUMOS    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.ILLUMOS  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.ILLUMOS := $(BREW)/opt/llvm/bin/llvm-strip

# Linux on i686
AS.DARWIN86.LINUX32    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.LINUX32    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.LINUX32   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.LINUX32    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.LINUX32    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.LINUX32  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.LINUX32 := $(BREW)/opt/llvm/bin/llvm-strip

# Linux on x86_64
AS.DARWIN86.LINUX64    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.LINUX64    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.LINUX64   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.LINUX64    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.LINUX64    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.LINUX64  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.LINUX64 := $(BREW)/opt/llvm/bin/llvm-strip

# OpenBSD
AS.DARWIN86.OPENBSD    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWIN86.OPENBSD    := $(BREW)/opt/llvm/bin/clang
CXX.DARWIN86.OPENBSD   := $(BREW)/opt/llvm/bin/clang++
AR.DARWIN86.OPENBSD    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWIN86.OPENBSD    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWIN86.OPENBSD  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWIN86.OPENBSD := $(BREW)/opt/llvm/bin/llvm-strip

# PC-DOS and compatibles on IBM-PC
AS.DARWIN86.PCDOS    := $(WAT)/bino64/wasm
CC.DARWIN86.PCDOS    := $(WAT)/bino64/wcc
CXX.DARWIN86.PCDOS   := $(WAT)/bino64/wpp
AR.DARWIN86.PCDOS    := $(WAT)/bino64/wlib
LD.DARWIN86.PCDOS    := $(WAT)/bino64/wlink
STRIP.DARWIN86.PCDOS := $(WAT)/bino64/wstrip

# 16-bit Microsoft Windows (3.11)
AS.DARWIN86.WIN311    := $(WAT)/bino64/wasm
CC.DARWIN86.WIN311    := $(WAT)/bino64/wcc
CXX.DARWIN86.WIN311   := $(WAT)/bino64/wpp
AR.DARWIN86.WIN311    := $(WAT)/bino64/wlib
LD.DARWIN86.WIN311    := $(WAT)/bino64/wlink
STRIP.DARWIN86.WIN311 := $(WAT)/bino64/wstrip

# 32-bit Microsoft Windows (95/98/Me)
AS.DARWIN86.WIN95    := $(BREW)/bin/i686-w64-mingw32-as
CC.DARWIN86.WIN95    := $(BREW)/bin/i686-w64-mingw32-gcc
CXX.DARWIN86.WIN95   := $(BREW)/bin/i686-w64-mingw32-g++
AR.DARWIN86.WIN95    := $(BREW)/bin/i686-w64-mingw32-ar
LD.DARWIN86.WIN95    := $(BREW)/bin/i686-w64-mingw32-gcc
OCPY.DARWIN86.WIN95  := $(BREW)/bin/i686-w64-mingw32-objcopy
STRIP.DARWIN86.WIN95 := $(BREW)/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (4.0+)
AS.DARWIN86.WINNT32    := $(BREW)/bin/i686-w64-mingw32-as
CC.DARWIN86.WINNT32    := $(BREW)/bin/i686-w64-mingw32-gcc
CXX.DARWIN86.WINNT32   := $(BREW)/bin/i686-w64-mingw32-g++
AR.DARWIN86.WINNT32    := $(BREW)/bin/i686-w64-mingw32-ar
LD.DARWIN86.WINNT32    := $(BREW)/bin/i686-w64-mingw32-gcc
OCPY.DARWIN86.WINNT32  := $(BREW)/bin/i686-w64-mingw32-objcopy
STRIP.DARWIN86.WINNT32 := $(BREW)/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (5.2+)
AS.DARWIN86.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-as
CC.DARWIN86.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-gcc
CXX.DARWIN86.WINNT64   := $(BREW)/bin/x86_64-w64-mingw32-g++
AR.DARWIN86.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-ar
LD.DARWIN86.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-gcc
OCPY.DARWIN86.WINNT64  := $(BREW)/bin/x86_64-w64-mingw32-objcopy
STRIP.DARWIN86.WINNT64 := $(BREW)/bin/x86_64-w64-mingw32-strip

# Utilities
PL.DARWIN86      := $(BREW)/bin/perl # brew perl
PY.DARWIN86      := $(BREW)/bin/python3
FMT.DARWIN86     := $(BREW)/bin/clang-format
LINT.DARWIN86    := $(BREW)/bin/cppcheck
INSTALL.DARWIN86 := $(BREW)/opt/coreutils/bin/ginstall # GNU coreutils
ECHO.DARWIN86    := $(BREW)/opt/coreutils/bin/gecho
CP.DARWIN86      := $(BREW)/opt/coreutils/bin/gcp

# Darwin AArch64 host

# GBA homebrew
AS.DARWINM1.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.DARWINM1.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.DARWINM1.AGBHB   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.DARWINM1.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.DARWINM1.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.DARWINM1.AGBHB  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.DARWINM1.AGBHB := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# GBA sourcepatching
AS.DARWINM1.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.DARWINM1.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.DARWINM1.AGBSP   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.DARWINM1.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.DARWINM1.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.DARWINM1.AGBSP  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.DARWINM1.AGBSP := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# macOS on Intel 64
AS.DARWINM1.DARWIN86    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.DARWIN86    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.DARWIN86   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.DARWIN86    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.DARWIN86    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.DARWIN86  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.DARWIN86 := $(BREW)/opt/llvm/bin/llvm-strip

# Native
AS.DARWINM1.DARWINM1    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.DARWINM1    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.DARWINM1   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.DARWINM1    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.DARWINM1    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.DARWINM1  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.DARWINM1 := $(BREW)/opt/llvm/bin/llvm-strip

# FreeBSD
AS.DARWINM1.FREEBSD    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.FREEBSD    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.FREEBSD   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.FREEBSD    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.FREEBSD    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.FREEBSD  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.FREEBSD := $(BREW)/opt/llvm/bin/llvm-strip

# IBM-PC direct (not DOS-specific)
AS.DARWINM1.IBMPC    := $(WAT)/armo64/wasm
CC.DARWINM1.IBMPC    := $(WAT)/armo64/wcc
CXX.DARWINM1.IBMPC   := $(WAT)/armo64/wpp
AR.DARWINM1.IBMPC    := $(WAT)/armo64/wlib
LD.DARWINM1.IBMPC    := $(WAT)/armo64/wlink
STRIP.DARWINM1.IBMPC := $(WAT)/armo64/wstrip

# illumos/OpenIndiana
AS.DARWINM1.ILLUMOS    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.ILLUMOS    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.ILLUMOS   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.ILLUMOS    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.ILLUMOS    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.ILLUMOS  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.ILLUMOS := $(BREW)/opt/llvm/bin/llvm-strip

# Linux on i686
AS.DARWINM1.LINUX32    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.LINUX32    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.LINUX32   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.LINUX32    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.LINUX32    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.LINUX32  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.LINUX32 := $(BREW)/opt/llvm/bin/llvm-strip

# Linux on x86_64
AS.DARWINM1.LINUX64    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.LINUX64    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.LINUX64   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.LINUX64    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.LINUX64    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.LINUX64  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.LINUX64 := $(BREW)/opt/llvm/bin/llvm-strip

# OpenBSD
AS.DARWINM1.OPENBSD    := $(BREW)/opt/llvm/bin/llvm-as
CC.DARWINM1.OPENBSD    := $(BREW)/opt/llvm/bin/clang
CXX.DARWINM1.OPENBSD   := $(BREW)/opt/llvm/bin/clang++
AR.DARWINM1.OPENBSD    := $(BREW)/opt/llvm/bin/llvm-ar
LD.DARWINM1.OPENBSD    := $(BREW)/opt/llvm/bin/clang
OCPY.DARWINM1.OPENBSD  := $(BREW)/opt/llvm/bin/llvm-objcopy
STRIP.DARWINM1.OPENBSD := $(BREW)/opt/llvm/bin/llvm-strip

# PC-DOS and compatibles on IBM-PC
AS.DARWINM1.PCDOS    := $(WAT)/armo64/wasm
CC.DARWINM1.PCDOS    := $(WAT)/armo64/wcc
CXX.DARWINM1.PCDOS   := $(WAT)/armo64/wpp
AR.DARWINM1.PCDOS    := $(WAT)/armo64/wlib
LD.DARWINM1.PCDOS    := $(WAT)/armo64/wlink
STRIP.DARWINM1.PCDOS := $(WAT)/armo64/wstrip

# 16-bit Microsoft Windows (3.11)
AS.DARWINM1.WIN311    := $(WAT)/armo64/wasm
CC.DARWINM1.WIN311    := $(WAT)/armo64/wcc
CXX.DARWINM1.WIN311   := $(WAT)/armo64/wpp
AR.DARWINM1.WIN311    := $(WAT)/armo64/wlib
LD.DARWINM1.WIN311    := $(WAT)/armo64/wlink
STRIP.DARWINM1.WIN311 := $(WAT)/armo64/wstrip

# 32-bit Microsoft Windows (95/98/Me)
AS.DARWINM1.WIN95    := $(BREW)/bin/i686-w64-mingw32-as
CC.DARWINM1.WIN95    := $(BREW)/bin/i686-w64-mingw32-gcc
CXX.DARWINM1.WIN95   := $(BREW)/bin/i686-w64-mingw32-g++
AR.DARWINM1.WIN95    := $(BREW)/bin/i686-w64-mingw32-ar
LD.DARWINM1.WIN95    := $(BREW)/bin/i686-w64-mingw32-gcc
OCPY.DARWINM1.WIN95  := $(BREW)/bin/i686-w64-mingw32-objcopy
STRIP.DARWINM1.WIN95 := $(BREW)/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (4.0+)
AS.DARWINM1.WINNT32    := $(BREW)/bin/i686-w64-mingw32-as
CC.DARWINM1.WINNT32    := $(BREW)/bin/i686-w64-mingw32-gcc
CXX.DARWINM1.WINNT32   := $(BREW)/bin/i686-w64-mingw32-g++
AR.DARWINM1.WINNT32    := $(BREW)/bin/i686-w64-mingw32-ar
LD.DARWINM1.WINNT32    := $(BREW)/bin/i686-w64-mingw32-gcc
OCPY.DARWINM1.WINNT32  := $(BREW)/bin/i686-w64-mingw32-objcopy
STRIP.DARWINM1.WINNT32 := $(BREW)/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (5.2+)
AS.DARWINM1.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-as
CC.DARWINM1.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-gcc
CXX.DARWINM1.WINNT64   := $(BREW)/bin/x86_64-w64-mingw32-g++
AR.DARWINM1.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-ar
LD.DARWINM1.WINNT64    := $(BREW)/bin/x86_64-w64-mingw32-gcc
OCPY.DARWINM1.WINNT64  := $(BREW)/bin/x86_64-w64-mingw32-objcopy
STRIP.DARWINM1.WINNT64 := $(BREW)/bin/x86_64-w64-mingw32-strip

# Utilities
PL.DARWINM1      := $(BREW)/bin/perl # brew perl
PY.DARWINM1      := $(BREW)/bin/python3
FMT.DARWINM1     := $(BREW)/bin/clang-format
LINT.DARWINM1    := $(BREW)/bin/cppcheck
INSTALL.DARWINM1 := $(BREW)/opt/coreutils/bin/ginstall # GNU coreutils
ECHO.DARWINM1    := $(BREW)/opt/coreutils/bin/gecho
CP.DARWINM1      := $(BREW)/opt/coreutils/bin/gcp

# Linux x86-64 host

# GBA homebrew
AS.LINUX64.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.LINUX64.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.LINUX64.AGBHB   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.LINUX64.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.LINUX64.AGBHB    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.LINUX64.AGBHB  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.LINUX64.AGBHB := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# GBA sourcepatching
AS.LINUX64.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-as
CC.LINUX64.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
CXX.LINUX64.AGBSP   := $(DKP)/devkitARM/bin/arm-none-eabi-g++
AR.LINUX64.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-ar
LD.LINUX64.AGBSP    := $(DKP)/devkitARM/bin/arm-none-eabi-gcc
OCPY.LINUX64.AGBSP  := $(DKP)/devkitARM/bin/arm-none-eabi-objcopy
STRIP.LINUX64.AGBSP := $(DKP)/devkitARM/bin/arm-none-eabi-strip

# macOS on Intel 64
AS.LINUX64.DARWIN86    := "Darwin cannot be targeted from GNU/Linux."
CC.LINUX64.DARWIN86    := "Darwin cannot be targeted from GNU/Linux."
CXX.LINUX64.DARWIN86   := "Darwin cannot be targeted from GNU/Linux."
AR.LINUX64.DARWIN86    := "Darwin cannot be targeted from GNU/Linux."
LD.LINUX64.DARWIN86    := "Darwin cannot be targeted from GNU/Linux."
OCPY.LINUX64.DARWIN86  := "Darwin cannot be targeted from GNU/Linux."
STRIP.LINUX64.DARWIN86 := "Darwin cannot be targeted from GNU/Linux."

# macOS on AArch64
AS.LINUX64.DARWINM1    := "Darwin cannot be targeted from GNU/Linux."
CC.LINUX64.DARWINM1    := "Darwin cannot be targeted from GNU/Linux."
CXX.LINUX64.DARWINM1   := "Darwin cannot be targeted from GNU/Linux."
AR.LINUX64.DARWINM1    := "Darwin cannot be targeted from GNU/Linux."
LD.LINUX64.DARWINM1    := "Darwin cannot be targeted from GNU/Linux."
OCPY.LINUX64.DARWINM1  := "Darwin cannot be targeted from GNU/Linux."
STRIP.LINUX64.DARWINM1 := "Darwin cannot be targeted from GNU/Linux."

# FreeBSD
AS.LINUX64.FREEBSD    := /usr/bin/llvm-as
CC.LINUX64.FREEBSD    := /usr/bin/clang
CXX.LINUX64.FREEBSD   := /usr/bin/clang++
AR.LINUX64.FREEBSD    := /usr/bin/llvm-ar
LD.LINUX64.FREEBSD    := /usr/bin/clang
OCPY.LINUX64.FREEBSD  := /usr/bin/llvm-objcopy
STRIP.LINUX64.FREEBSD := /usr/bin/llvm-strip

# IBM-PC direct (not DOS-specific)
AS.LINUX64.IBMPC    := $(WAT)/binl64/wasm
CC.LINUX64.IBMPC    := $(WAT)/binl64/wcc
CXX.LINUX64.IBMPC   := $(WAT)/binl64/wpp
AR.LINUX64.IBMPC    := $(WAT)/binl64/wlib
LD.LINUX64.IBMPC    := $(WAT)/binl64/wlink
STRIP.LINUX64.IBMPC := $(WAT)/binl64/wstrip

# illumos/OpenIndiana
AS.LINUX64.ILLUMOS    := /usr/bin/llvm-as
CC.LINUX64.ILLUMOS    := /usr/bin/clang
CXX.LINUX64.ILLUMOS   := /usr/bin/clang++
AR.LINUX64.ILLUMOS    := /usr/bin/llvm-ar
LD.LINUX64.ILLUMOS    := /usr/bin/clang
OCPY.LINUX64.ILLUMOS  := /usr/bin/llvm-objcopy
STRIP.LINUX64.ILLUMOS := /usr/bin/llvm-strip

# Linux on i686
AS.LINUX64.LINUX32    := /usr/bin/llvm-as
CC.LINUX64.LINUX32    := /usr/bin/clang
CXX.LINUX64.LINUX32   := /usr/bin/clang++
AR.LINUX64.LINUX32    := /usr/bin/llvm-ar
LD.LINUX64.LINUX32    := /usr/bin/clang
OCPY.LINUX64.LINUX32  := /usr/bin/llvm-objcopy
STRIP.LINUX64.LINUX32 := /usr/bin/llvm-strip

# Native
AS.LINUX64.LINUX64    := /usr/bin/llvm-as
CC.LINUX64.LINUX64    := /usr/bin/clang
CXX.LINUX64.LINUX64   := /usr/bin/clang++
AR.LINUX64.LINUX64    := /usr/bin/llvm-ar
LD.LINUX64.LINUX64    := /usr/bin/clang
OCPY.LINUX64.LINUX64  := /usr/bin/llvm-objcopy
STRIP.LINUX64.LINUX64 := /usr/bin/llvm-strip

# OpenBSD
AS.LINUX64.OPENBSD    := /usr/bin/llvm-as
CC.LINUX64.OPENBSD    := /usr/bin/clang
CXX.LINUX64.OPENBSD   := /usr/bin/clang++
AR.LINUX64.OPENBSD    := /usr/bin/llvm-ar
LD.LINUX64.OPENBSD    := /usr/bin/clang
OCPY.LINUX64.OPENBSD  := /usr/bin/llvm-objcopy
STRIP.LINUX64.OPENBSD := /usr/bin/llvm-strip

# PC-DOS and compatibles on IBM-PC
AS.LINUX64.PCDOS    := $(WAT)/binl64/wasm
CC.LINUX64.PCDOS    := $(WAT)/binl64/wcc
CXX.LINUX64.PCDOS   := $(WAT)/binl64/wpp
AR.LINUX64.PCDOS    := $(WAT)/binl64/wlib
LD.LINUX64.PCDOS    := $(WAT)/binl64/wlink
STRIP.LINUX64.PCDOS := $(WAT)/binl64/wstrip

# 16-bit Microsoft Windows (3.11)
AS.LINUX64.WIN311    := $(WAT)/binl64/wasm
CC.LINUX64.WIN311    := $(WAT)/binl64/wcc
CXX.LINUX64.WIN311   := $(WAT)/binl64/wpp
AR.LINUX64.WIN311    := $(WAT)/binl64/wlib
LD.LINUX64.WIN311    := $(WAT)/binl64/wlink
STRIP.LINUX64.WIN311 := $(WAT)/binl64/wstrip

# 32-bit Microsoft Windows (95/98/Me)
AS.LINUX64.WIN95    := /usr/bin/i686-w64-mingw32-as
CC.LINUX64.WIN95    := /usr/bin/i686-w64-mingw32-gcc
CXX.LINUX64.WIN95   := /usr/bin/i686-w64-mingw32-g++
AR.LINUX64.WIN95    := /usr/bin/i686-w64-mingw32-ar
LD.LINUX64.WIN95    := /usr/bin/i686-w64-mingw32-gcc
OCPY.LINUX64.WIN95  := /usr/bin/i686-w64-mingw32-objcopy
STRIP.LINUX64.WIN95 := /usr/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (4.0+)
AS.LINUX64.WINNT32    := /usr/bin/i686-w64-mingw32-as
CC.LINUX64.WINNT32    := /usr/bin/i686-w64-mingw32-gcc
CXX.LINUX64.WINNT32   := /usr/bin/i686-w64-mingw32-g++
AR.LINUX64.WINNT32    := /usr/bin/i686-w64-mingw32-ar
LD.LINUX64.WINNT32    := /usr/bin/i686-w64-mingw32-gcc
OCPY.LINUX64.WINNT32  := /usr/bin/i686-w64-mingw32-objcopy
STRIP.LINUX64.WINNT32 := /usr/bin/i686-w64-mingw32-strip

# 32-bit Windows NT (5.2+)
AS.LINUX64.WINNT64    := /usr/bin/x86_64-w64-mingw32-as
CC.LINUX64.WINNT64    := /usr/bin/x86_64-w64-mingw32-gcc
CXX.LINUX64.WINNT64   := /usr/bin/x86_64-w64-mingw32-g++
AR.LINUX64.WINNT64    := /usr/bin/x86_64-w64-mingw32-ar
LD.LINUX64.WINNT64    := /usr/bin/x86_64-w64-mingw32-gcc
OCPY.LINUX64.WINNT64  := /usr/bin/x86_64-w64-mingw32-objcopy
STRIP.LINUX64.WINNT64 := /usr/bin/x86_64-w64-mingw32-strip

# Utilities
PL.LINUX64      := /usr/bin/perl
PY.LINUX64      := /usr/bin/python3
FMT.LINUX64     := /usr/bin/clang-format
LINT.LINUX64    := /usr/bin/cppcheck
INSTALL.LINUX64 := /usr/bin/install
ECHO.LINUX64    := /bin/echo # not a bashism
CP.LINUX64      := /bin/cp

# Set the origin-dependent values of the new variables.
AS.O_DEFAULT      := $(AS.$(HP).$(TP))
CC.O_DEFAULT      := $(CC.$(HP).$(TP))
CXX.O_DEFAULT     := $(CXX.$(HP).$(TP))
AR.O_DEFAULT      := $(AR.$(HP).$(TP))
LD.O_DEFAULT      := $(LD.$(HP).$(TP))
OCPY.O_DEFAULT    := $(OCPY.$(HP).$(TP))
STRIP.O_DEFAULT   := $(STRIP.$(HP).$(TP))
PL.O_DEFAULT      := $(PL.$(HP))
PY.O_DEFAULT      := $(PY.$(HP))
FMT.O_DEFAULT     := $(FMT.$(HP))
LINT.O_DEFAULT    := $(LINT.$(HP))
INSTALL.O_DEFAULT := $(INSTALL.$(HP))
ECHO.O_DEFAULT    := $(ECHO.$(HP))
CP.O_DEFAULT      := $(CP.$(HP))

AS.O_CUSTOM      := $(AS)
CC.O_CUSTOM      := $(CC)
CXX.O_CUSTOM     := $(CXX)
AR.O_CUSTOM      := $(AR)
LD.O_CUSTOM      := $(LD)
OCPY.O_CUSTOM    := $(OCPY)
STRIP.O_CUSTOM   := $(STRIP)
PL.O_CUSTOM      := $(PL)
PY.O_CUSTOM      := $(PY)
FMT.O_CUSTOM     := $(FMT)
LINT.O_CUSTOM    := $(LINT)
INSTALL.O_CUSTOM := $(INSTALL)
ECHO.O_CUSTOM    := $(ECHO)
CP.O_CUSTOM      := $(CP)

# Finally, set the variables.
override AS      := $(AS.O_$(.O_AS))
override CC      := $(CC.O_$(.O_CC))
override CXX     := $(CXX.O_$(.O_CXX))
override AR      := $(AR.O_$(.O_AR))
override LD      := $(LD.O_$(.O_LD))
override OCPY    := $(OCPY.O_$(.O_OCPY))
override STRIP   := $(STRIP.O_$(.O_STRIP))
override PL      := $(PL.O_$(.O_PL))
override PY      := $(PY.O_$(.O_PY))
override FMT     := $(FMT.O_$(.O_FMT))
override LINT    := $(LINT.O_$(.O_LINT))
override INSTALL := $(INSTALL.O_$(.O_INSTALL))
override ECHO    := $(ECHO.O_$(.O_ECHO))
override CP      := $(CP.O_$(.O_CP))

## Suffixes.

# Shared libraries.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was customised.
# The ".O_" prefix denotes "origin" and is to prevent naming collisions.
ifeq ($(origin SO),undefined)
.O_SO := DEFAULT
else ifeq ($(origin SO),default)
.O_SO := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_SO := CUSTOM
endif # $(origin SO)

SO.AGBHB    := "GBA does not have shared libraries."
SO.AGBSP    := "GBA does not have shared libraries."
SO.DARWIN86 := .dylib
SO.DARWINM1 := .dylib
SO.FREEBSD  := .so
SO.IBMPC    := "IBM-PC does not have shared libraries."
SO.ILLUMOS  := .so
SO.LINUX32  := .so
SO.LINUX64  := .so
SO.OPENBSD  := .so
SO.PCDOS    := "PC-DOS does not have shared libraries."
SO.WIN311   := .dll
SO.WIN95    := .dll
SO.WINNT32  := .dll
SO.WINNT64  := .dll

# Set the origin-dependent values of the new variable.
SO.O_DEFAULT := $(SO.$(TP))
SO.O_CUSTOM := $(SO)

# Finally, set the variable.
override SO := $(SO.O_$(.O_SO))

# Static libraries.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was customised.
# The ".O_" prefix denotes "origin" and is to prevent naming collisions.
ifeq ($(origin A),undefined)
.O_A := DEFAULT
else ifeq ($(origin A),default)
.O_A := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_A := CUSTOM
endif # $(origin A)

A.AGBHB    := .a
A.AGBSP    := .a
A.DARWIN86 := .a
A.DARWINM1 := .a
A.FREEBSD  := .a
A.IBMPC    := .lib
A.ILLUMOS  := .a
A.LINUX32  := .a
A.LINUX64  := .a
A.OPENBSD  := .a
A.PCDOS    := .lib
A.WIN311   := .lib
A.WIN95    := .lib
A.WINNT32  := .lib
A.WINNT64  := .lib

# Set the origin-dependent values of the new variable.
A.O_DEFAULT := $(A.$(TP))
A.O_CUSTOM := $(A)

# Finally, set the variable.
override A := $(A.O_$(.O_A))

# Executables.

# Inspect the origin of the new variable.
# If it is undefined or set by default, say so. Otherwise it was customised.
# The ".O_" prefix denotes "origin" and is to prevent naming collisions.
ifeq ($(origin EXE),undefined)
.O_EXE := DEFAULT
else ifeq ($(origin EXE),default)
.O_EXE := DEFAULT
else
# environment [override], file, command line, override, automatic
.O_EXE := CUSTOM
endif # $(origin EXE)

EXE.AGBHB    := .elf
EXE.AGBSP    :=
EXE.DARWIN86 :=
EXE.DARWINM1 :=
EXE.FREEBSD  :=
EXE.IBMPC    :=
EXE.ILLUMOS  :=
EXE.LINUX32  :=
EXE.LINUX64  :=
EXE.OPENBSD  :=
EXE.PCDOS    := .com
EXE.WIN311   := .exe
EXE.WIN95    := .exe
EXE.WINNT32  := .exe
EXE.WINNT64  := .exe

# Set the origin-dependent values of the new variable.
EXE.O_DEFAULT := $(EXE.$(TP))
EXE.O_CUSTOM := $(EXE)

# Finally, set the variable.
override EXE := $(EXE.O_$(.O_EXE))

## Flags.

# Assembler flags.
# Form: ASFLAGS.<TP>
# Only GNU toolchain is supported. Darwin cannot be targeted.

ASFLAGS.AGBHB    := -EL -march=armv4t -mcpu=arm7tdmi -mthumb-interwork
ASFLAGS.AGBSP    := -EL -march=armv4t -mcpu=arm7tdmi
ASFLAGS.DARWIN86 := -march=x86-64 -mtune=x86-64
ASFLAGS.DARWINM1 := -march=armv8.4-a -mcpu=apple-m1
ASFLAGS.FREEBSD  := -march=x86-64 -mtune=x86-64
ASFLAGS.IBMPC    := -2
ASFLAGS.ILLUMOS  := -mcpu=v9 -mtune=niagara
ASFLAGS.LINUX32  := -march=i686 -mtune=x86-64
ASFLAGS.LINUX64  := -march=x86-64 -mtune=x86-64
ASFLAGS.OPENBSD  := -march=x86-64 -mtune=x86-64
ASFLAGS.PCDOS    := -2 -bt=dos
ASFLAGS.WIN311   := -2p -bt=windows
ASFLAGS.WIN95    := -march=i386 -mtune=i486
ASFLAGS.WINNT32  := -march=i386 -mtune=i686
ASFLAGS.WINNT64  := -march=x86-64 -mtune=x86-64

# C compiler flags.
# Form: CFLAGS.<RECIPE>.<TP>

CFLAGS.ANY.AGBHB    := -ansi -frandom-seed=69420 -march=armv4t \
	-mcpu=arm7tdmi -mthumb-interwork -pipe -Wpedantic \
	-Wno-builtin-declaration-mismatch -Wno-long-long -x c
CFLAGS.ANY.AGBSP    := -ansi -ffreestanding -fno-pie -fPIC \
	-frandom-seed=69420 -march=armv4t -mcpu=arm7tdmi -nostdinc -pipe \
	-Wpedantic -Wno-long-long -x c
CFLAGS.ANY.DARWIN86 := -ansi -fPIC -frandom-seed=69420 -march=x86-64 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.DARWINM1 := -ansi -fPIC -frandom-seed=69420 \
	-march=armv8.4-a -mcpu=apple-m1 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.FREEBSD  := -ansi -fPIC -frandom-seed=69420 -march=x86-64 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.IBMPC    := -2 -aa -ecw -ml -zA -zastd=c89 -zku8 -zl -zld \
	-zls -zp2 -zu
CFLAGS.ANY.ILLUMOS  := -ansi -fPIC -frandom-seed=69420 -mcpu=v9 \
	-mtune=niagara -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.LINUX32  := -ansi -fPIC -frandom-seed=69420 -march=i686 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.LINUX64  := -ansi -fPIC -frandom-seed=69420 -march=x86-64 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.OPENBSD  := -ansi -fPIC -frandom-seed=69420 -march=x86-64 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.PCDOS    := -2 -aa -bt=dos -ecp -ml -zastd=c89 -zku8 -zl \
	-zld -zls -zp2 -zu
CFLAGS.ANY.WIN311   := -2 -aa -bt=windows -ecp -ml -zastd=c89 -zku8 \
	-zl -zld -zls -zp2 -zu -zW -zw -zws
CFLAGS.ANY.WIN95    := -ansi -fPIC -frandom-seed=69420 -march=i386 \
	-mtune=i486 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.WINNT32  := -ansi -fPIC -frandom-seed=69420 -march=i386 \
	-mtune=i686 -pipe -Wpedantic -Wno-long-long -x c
CFLAGS.ANY.WINNT64  := -ansi -fPIC -frandom-seed=69420 -march=x86-64 \
	-mtune=x86-64 -pipe -Wpedantic -Wno-long-long -x c

CFLAGS.DEBUG.AGBHB    := -O0 -g3 -Wall
CFLAGS.DEBUG.AGBSP    := -O0 -g3 -Wall
CFLAGS.DEBUG.DARWIN86 := -O0 -g3 -Wall
CFLAGS.DEBUG.DARWINM1 := -O0 -g3 -Wall
CFLAGS.DEBUG.FREEBSD  := -O0 -g3 -Wall
CFLAGS.DEBUG.IBMPC    := -d2 -hd -wx
CFLAGS.DEBUG.ILLUMOS  := -O0 -g3 -Wall
CFLAGS.DEBUG.LINUX32  := -O0 -g3 -Wall
CFLAGS.DEBUG.LINUX64  := -O0 -g3 -Wall
CFLAGS.DEBUG.OPENBSD  := -O0 -g3 -Wall
CFLAGS.DEBUG.PCDOS    := -d2 -hd -wx
CFLAGS.DEBUG.WIN311   := -d2 -hd -wx
CFLAGS.DEBUG.WIN95    := -O0 -g3 -Wall
CFLAGS.DEBUG.WINNT32  := -O0 -g3 -Wall
CFLAGS.DEBUG.WINNT64  := -O0 -g3 -Wall

CFLAGS.RELEASE.AGBHB    := -O3 -w
CFLAGS.RELEASE.AGBSP    := -O3 -w
CFLAGS.RELEASE.DARWIN86 := -O3 -w
CFLAGS.RELEASE.DARWINM1 := -O3 -w
CFLAGS.RELEASE.FREEBSD  := -O3 -w
CFLAGS.RELEASE.IBMPC    := -d0 -ox -w=0
CFLAGS.RELEASE.ILLUMOS  := -O3 -w
CFLAGS.RELEASE.LINUX32  := -O3 -w
CFLAGS.RELEASE.LINUX64  := -O3 -w
CFLAGS.RELEASE.OPENBSD  := -O3 -w
CFLAGS.RELEASE.PCDOS    := -d0 -ox -w=0
CFLAGS.RELEASE.WIN311   := -d0 -ox -w=0
CFLAGS.RELEASE.WIN95    := -O3 -w
CFLAGS.RELEASE.WINNT32  := -O3 -w
CFLAGS.RELEASE.WINNT64  := -O3 -w

CFLAGS.CHECK.AGBHB    := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.AGBSP    := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.DARWIN86 := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.DARWINM1 := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.FREEBSD  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.IBMPC    := -wx -zs
CFLAGS.CHECK.ILLUMOS  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.LINUX32  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.LINUX64  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.OPENBSD  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.PCDOS    := -wx -zs
CFLAGS.CHECK.WIN311   := -wx -zs
CFLAGS.CHECK.WIN95    := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.WINNT32  := -E -Wextra -Werror -Wno-unused-variable
CFLAGS.CHECK.WINNT64  := -E -Wextra -Werror -Wno-unused-variable

# C++ compiler flags.
# Form: CXXFLAGS.<RECIPE>.<TP>

CXXFLAGS.ANY.AGBHB    := -std=c++11 -frandom-seed=69420 -march=armv4t \
	-mcpu=arm7tdmi -mthumb-interwork -pipe -Wpedantic -x c++
CXXFLAGS.ANY.AGBSP    := -std=c++11 -ffreestanding -fno-pie -fPIC \
	-frandom-seed=69420 -march=armv4t -mcpu=arm7tdmi -nostdinc -pipe \
	-Wpedantic -x c++
CXXFLAGS.ANY.DARWIN86 := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.DARWINM1 := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=armv8.4-a -mcpu=apple-m1 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.FREEBSD  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.IBMPC    := -2 -aa -ecw -ml -zA -zastd=c89 -zku8 -zl \
	-zld -zls -zp2 -zu
CXXFLAGS.ANY.ILLUMOS  := -std=c++11 -fPIC -frandom-seed=69420 \
	-mcpu=v9 -mtune=niagara -pipe -Wpedantic -x c++
CXXFLAGS.ANY.LINUX32  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=i686 -mtune=x86-64 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.LINUX64  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.OPENBSD  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.PCDOS    := -2 -aa -bt=dos -ecp -ml -zastd=c89 -zku8 -zl \
	-zld -zls -zp2 -zu
CXXFLAGS.ANY.WIN311   := -2 -aa -bt=windows -ecp -ml -zastd=c89 -zku8 \
	-zl -zld -zls -zp2 -zu -zW -zw -zws
CXXFLAGS.ANY.WIN95    := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=i386 -mtune=i486 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.WINNT32  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=i386 -mtune=i686 -pipe -Wpedantic -x c++
CXXFLAGS.ANY.WINNT64  := -std=c++11 -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x c++

CXXFLAGS.DEBUG.AGBHB    := -O0 -g3 -Wall
CXXFLAGS.DEBUG.AGBSP    := -O0 -g3 -Wall
CXXFLAGS.DEBUG.DARWIN86 := -O0 -g3 -Wall
CXXFLAGS.DEBUG.DARWINM1 := -O0 -g3 -Wall
CXXFLAGS.DEBUG.FREEBSD  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.IBMPC    := -d2 -hd -wx
CXXFLAGS.DEBUG.ILLUMOS  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.LINUX32  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.LINUX64  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.OPENBSD  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.PCDOS    := -d2 -hd -wx
CXXFLAGS.DEBUG.WIN311   := -d2 -hd -wx
CXXFLAGS.DEBUG.WIN95    := -O0 -g3 -Wall
CXXFLAGS.DEBUG.WINNT32  := -O0 -g3 -Wall
CXXFLAGS.DEBUG.WINNT64  := -O0 -g3 -Wall

CXXFLAGS.RELEASE.AGBHB    := -O3 -w
CXXFLAGS.RELEASE.AGBSP    := -O3 -w
CXXFLAGS.RELEASE.DARWIN86 := -O3 -w
CXXFLAGS.RELEASE.DARWINM1 := -O3 -w
CXXFLAGS.RELEASE.FREEBSD  := -O3 -w
CXXFLAGS.RELEASE.IBMPC    := -d0 -ox -w=0
CXXFLAGS.RELEASE.ILLUMOS  := -O3 -w
CXXFLAGS.RELEASE.LINUX32  := -O3 -w
CXXFLAGS.RELEASE.LINUX64  := -O3 -w
CXXFLAGS.RELEASE.OPENBSD  := -O3 -w
CXXFLAGS.RELEASE.PCDOS    := -d0 -ox -w=0
CXXFLAGS.RELEASE.WIN311   := -d0 -ox -w=0
CXXFLAGS.RELEASE.WIN95    := -O3 -w
CXXFLAGS.RELEASE.WINNT32  := -O3 -w
CXXFLAGS.RELEASE.WINNT64  := -O3 -w

CXXFLAGS.CHECK.AGBHB    := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.AGBSP    := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.DARWIN86 := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.DARWINM1 := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.FREEBSD  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.IBMPC    := -wx -zs
CXXFLAGS.CHECK.ILLUMOS  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.LINUX32  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.LINUX64  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.OPENBSD  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.PCDOS    := -wx -zs
CXXFLAGS.CHECK.WIN311   := -wx -zs
CXXFLAGS.CHECK.WIN95    := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.WINNT32  := -E -Wextra -Werror -Wno-unused-variable
CXXFLAGS.CHECK.WINNT64  := -E -Wextra -Werror -Wno-unused-variable

# Objective-C compiler flags.
# Form: OBJCFLAGS.<RECIPE>.<TP>

OBJCFLAGS.ANY.AGBHB    := -ansi -frandom-seed=69420 -march=armv4t \
	-mcpu=arm7tdmi -mthumb-interwork -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.AGBSP    := -ansi -ffreestanding -fno-pie -fPIC \
	-frandom-seed=69420 -march=armv4t -mcpu=arm7tdmi -nostdinc -pipe \
	-Wpedantic -x objective-c
OBJCFLAGS.ANY.DARWIN86 := -ansi -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.DARWINM1 := -ansi -fPIC -frandom-seed=69420 \
	-march=armv8.4-a -mcpu=apple-m1 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.FREEBSD  := -ansi -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.ILLUMOS  := -ansi -fPIC -frandom-seed=69420 -mcpu=v9 \
	-mtune=niagara -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.LINUX32  := -ansi -fPIC -frandom-seed=69420 -march=i686 \
	-mtune=x86-64 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.LINUX64  := -ansi -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.OPENBSD  := -ansi -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.WIN95    := -ansi -fPIC -frandom-seed=69420 -march=i386 \
	-mtune=i486 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.WINNT32  := -ansi -fPIC -frandom-seed=69420 -march=i386 \
	-mtune=i686 -pipe -Wpedantic -x objective-c
OBJCFLAGS.ANY.WINNT64  := -ansi -fPIC -frandom-seed=69420 \
	-march=x86-64 -mtune=x86-64 -pipe -Wpedantic -x objective-c

OBJCFLAGS.DEBUG.AGBHB    := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.AGBSP    := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.DARWIN86 := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.DARWINM1 := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.FREEBSD  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.ILLUMOS  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.LINUX32  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.LINUX64  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.OPENBSD  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.WIN95    := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.WINNT32  := -O0 -g3 -Wall
OBJCFLAGS.DEBUG.WINNT64  := -O0 -g3 -Wall

OBJCFLAGS.RELEASE.AGBHB    := -O3 -w
OBJCFLAGS.RELEASE.AGBSP    := -O3 -w
OBJCFLAGS.RELEASE.DARWIN86 := -O3 -w
OBJCFLAGS.RELEASE.DARWINM1 := -O3 -w
OBJCFLAGS.RELEASE.FREEBSD  := -O3 -w
OBJCFLAGS.RELEASE.ILLUMOS  := -O3 -w
OBJCFLAGS.RELEASE.LINUX32  := -O3 -w
OBJCFLAGS.RELEASE.LINUX64  := -O3 -w
OBJCFLAGS.RELEASE.OPENBSD  := -O3 -w
OBJCFLAGS.RELEASE.WIN95    := -O3 -w
OBJCFLAGS.RELEASE.WINNT32  := -O3 -w
OBJCFLAGS.RELEASE.WINNT64  := -O3 -w

OBJCFLAGS.CHECK.AGBHB    := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.AGBSP    := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.DARWIN86 := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.DARWINM1 := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.FREEBSD  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.ILLUMOS  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.LINUX32  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.LINUX64  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.OPENBSD  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.WIN95    := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.WINNT32  := -E -Wextra -Werror -Wno-unused-variable
OBJCFLAGS.CHECK.WINNT64  := -E -Wextra -Werror -Wno-unused-variable

# Archiver flags.
# Form: ARFLAGS.<RECIPE>.<TP>

ARFLAGS.ANY.AGBHB    := -rcs
ARFLAGS.ANY.AGBSP    := -rcs
ARFLAGS.ANY.DARWIN86 := -rcs
ARFLAGS.ANY.DARWINM1 := -rcs
ARFLAGS.ANY.FREEBSD  := -rcs
ARFLAGS.ANY.IBMPC    := -b -c -fa
ARFLAGS.ANY.ILLUMOS  := -rcs
ARFLAGS.ANY.LINUX32  := -rcs
ARFLAGS.ANY.LINUX64  := -rcs
ARFLAGS.ANY.OPENBSD  := -rcs
ARFLAGS.ANY.PCDOS    := -b -c -fa
ARFLAGS.ANY.WIN311   := -b -c -fa
ARFLAGS.ANY.WIN95    := -rcs
ARFLAGS.ANY.WINNT32  := -rcs
ARFLAGS.ANY.WINNT64  := -rcs

ARFLAGS.DEBUG.AGBHB    :=
ARFLAGS.DEBUG.AGBSP    :=
ARFLAGS.DEBUG.DARWIN86 :=
ARFLAGS.DEBUG.DARWINM1 :=
ARFLAGS.DEBUG.FREEBSD  :=
ARFLAGS.DEBUG.IBMPC    :=
ARFLAGS.DEBUG.ILLUMOS  :=
ARFLAGS.DEBUG.LINUX32  :=
ARFLAGS.DEBUG.LINUX64  :=
ARFLAGS.DEBUG.OPENBSD  :=
ARFLAGS.DEBUG.PCDOS    :=
ARFLAGS.DEBUG.WIN311   :=
ARFLAGS.DEBUG.WIN95    :=
ARFLAGS.DEBUG.WINNT32  :=
ARFLAGS.DEBUG.WINNT64  :=

ARFLAGS.RELEASE.AGBHB    :=
ARFLAGS.RELEASE.AGBSP    :=
ARFLAGS.RELEASE.DARWIN86 :=
ARFLAGS.RELEASE.DARWINM1 :=
ARFLAGS.RELEASE.FREEBSD  :=
ARFLAGS.RELEASE.IBMPC    := -s -tl -zld -zll
ARFLAGS.RELEASE.ILLUMOS  :=
ARFLAGS.RELEASE.LINUX32  :=
ARFLAGS.RELEASE.LINUX64  :=
ARFLAGS.RELEASE.OPENBSD  :=
ARFLAGS.RELEASE.PCDOS    := -s -tl -zld -zll
ARFLAGS.RELEASE.WIN311   := -s -tl -zld -zll
ARFLAGS.RELEASE.WIN95    :=
ARFLAGS.RELEASE.WINNT32  :=
ARFLAGS.RELEASE.WINNT64  :=

# Linker flags.
# Form: LDFLAGS.<RECIPE>.<TP>

LDFLAGS.ANY.AGBHB    := -fPIE
LDFLAGS.ANY.AGBSP    := -fPIE
LDFLAGS.ANY.DARWIN86 := -fPIE -mlinker-version=305
LDFLAGS.ANY.DARWINM1 := -fPIE -mlinker-version=305
LDFLAGS.ANY.FREEBSD  := -fPIE
LDFLAGS.ANY.IBMPC    :=
LDFLAGS.ANY.ILLUMOS  := -fPIE
LDFLAGS.ANY.LINUX32  := -fPIE
LDFLAGS.ANY.LINUX64  := -fPIE
LDFLAGS.ANY.OPENBSD  := -fPIE
LDFLAGS.ANY.PCDOS    :=
LDFLAGS.ANY.WIN311   :=
LDFLAGS.ANY.WIN95    := -fPIE
LDFLAGS.ANY.WINNT32  := -fPIE
LDFLAGS.ANY.WINNT64  := -fPIE

LDFLAGS.DEBUG.AGBHB    :=
LDFLAGS.DEBUG.AGBSP    :=
LDFLAGS.DEBUG.DARWIN86 :=
LDFLAGS.DEBUG.DARWINM1 :=
LDFLAGS.DEBUG.FREEBSD  :=
LDFLAGS.DEBUG.IBMPC    :=
LDFLAGS.DEBUG.ILLUMOS  :=
LDFLAGS.DEBUG.LINUX32  :=
LDFLAGS.DEBUG.LINUX64  :=
LDFLAGS.DEBUG.OPENBSD  :=
LDFLAGS.DEBUG.PCDOS    :=
LDFLAGS.DEBUG.WIN311   :=
LDFLAGS.DEBUG.WIN95    :=
LDFLAGS.DEBUG.WINNT32  :=
LDFLAGS.DEBUG.WINNT64  :=

LDFLAGS.RELEASE.AGBHB    := -Wl,--build-id=none
LDFLAGS.RELEASE.AGBSP    := -Wl,--build-id=none
LDFLAGS.RELEASE.DARWIN86 :=
LDFLAGS.RELEASE.DARWINM1 :=
LDFLAGS.RELEASE.FREEBSD  :=
LDFLAGS.RELEASE.IBMPC    :=
LDFLAGS.RELEASE.ILLUMOS  :=
LDFLAGS.RELEASE.LINUX32  :=
LDFLAGS.RELEASE.LINUX64  :=
LDFLAGS.RELEASE.OPENBSD  :=
LDFLAGS.RELEASE.PCDOS    :=
LDFLAGS.RELEASE.WIN311   :=
LDFLAGS.RELEASE.WIN95    := -Wl,--build-id=none
LDFLAGS.RELEASE.WINNT32  := -Wl,--build-id=none
LDFLAGS.RELEASE.WINNT64  := -Wl,--build-id=none

# Synthetic definitions.
# Form: SYNDEFS.<TP>

SYNDEFS.AGBHB    := GBA ARMV4T LILENDIAN PTRSZ_32 HAVE_I32 HAVE_FP \
	FP_SOFT LONGSZ_32
SYNDEFS.AGBSP    := GBA ARMV4T SOURCEPATCH LILENDIAN PTRSZ_32 \
	HAVE_I32 HAVE_FP FP_SOFT LONGSZ_32
SYNDEFS.DARWIN86 := DARWIN AMD64 LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 \
	HAVE_FP FP_SOFT LONGSZ_64
SYNDEFS.DARWINM1 := DARWIN ARMV8 LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 \
	HAVE_FP FP_SOFT LONGSZ_64
SYNDEFS.FREEBSD  := BSD LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 HAVE_FP \
	FP_SOFT LONGSZ_64
SYNDEFS.IBMPC    := I286 LILENDIAN PTRSZ_16 HAVE_FP FP_SOFT LONGSZ_16
SYNDEFS.ILLUMOS  := SOLARIS LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 \
	HAVE_FP FP_SOFT LONGSZ_64
SYNDEFS.LINUX32  := LINUX LILENDIAN PTRSZ_32 HAVE_I64 HAVE_I32 \
	HAVE_FP FP_SOFT LONGSZ_32
SYNDEFS.LINUX64  := LINUX LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 \
	HAVE_FP FP_SOFT LONGSZ_64
SYNDEFS.OPENBSD  := BSD LILENDIAN PTRSZ_64 HAVE_I64 HAVE_I32 HAVE_FP \
	FP_SOFT LONGSZ_64
SYNDEFS.PCDOS    := DOS I286 LILENDIAN PTRSZ_16 HAVE_FP FP_SOFT \
	LONGSZ_16
SYNDEFS.WIN311   := WINDOWS I286 WIN16 LILENDIAN PTRSZ_16 HAVE_FP \
	FP_SOFT LONGSZ_16
SYNDEFS.WIN95    := WINDOWS IA32 WIN32 LILENDIAN PTRSZ_32 HAVE_I64 \
	HAVE_I32 HAVE_FP FP_SOFT LONGSZ_32
SYNDEFS.WINNT32  := WINDOWS IA32 WINNT LILENDIAN PTRSZ_32 HAVE_I64 \
	HAVE_I32 HAVE_FP FP_SOFT LONGSZ_32
SYNDEFS.WINNT64  := WINDOWS AMD64 WINNT LILENDIAN PTRSZ_64 HAVE_I64 \
	HAVE_I32 HAVE_FP FP_SOFT LONGSZ_32

FMTFLAGS := -style=file:meta/clang-format.yml

# Make builds deterministic when using LLVM or GNU C/C++ compilers.
# These are the environment variables necessary; see CFLAGS and
# CXXFLAGS for  the command-line options that deterministic builds
# depend on.
override SOURCE_DATE_EPOCH := 0
override ZERO_AR_DATE      := 1

# EOF
