# ##########################################################################
# MMC - Morphing Match Chain library
# Copyright (C) Yann Collet 2010-2015
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - ZSTD source repository : http://code.google.com/p/zstd/
#  - Public forum : https://groups.google.com/forum/#!forum/lz4c
# ##########################################################################

LIBVER_MAJOR = 0
LIBVER_MINOR = 2
LIBVER_PATCH = 0
LIBVER  = $(LIBVER_MAJOR).$(LIBVER_MINOR).$(LIBVER_PATCH)
VERSION?= $(LIBVER)

VERSION?= 0.2.0

DESTDIR?=
PREFIX ?= /usr/local
CFLAGS ?= -O3  # -falign-loops=32   # not always positive
CFLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-qual -Wcast-align -Wstrict-prototypes
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(MOREFLAGS)

BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
VOID = nul
else
EXT =
VOID = /dev/null
endif

# OS X linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(shell uname), Darwin)
	SHARED_EXT = dylib
	SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
	SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
	SONAME_FLAGS = -install_name $(PREFIX)/lib/libmmc.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
	SONAME_FLAGS = -Wl,-soname=libmmc.$(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT = so
	SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif

default: libmmc

all: libmmc

libmmc: mmc.c
	@echo compiling static library
	@$(CC) $(FLAGS) -c $^
	@$(AR) rcs $@.a mmc.o
	@echo compiling dynamic library $(LIBVER)
	@$(CC) $(FLAGS) -shared $^ -fPIC $(SONAME_FLAGS) -o $@.$(SHARED_EXT_VER)
	@echo creating versioned links
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT_MAJOR)
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT)

example: example.o mmc.o
	$(CC) $(FLAGS) $^ -o $@

clean:
	@rm -f core *.o *.a *.$(SHARED_EXT) *.$(SHARED_EXT).* libmmc.pc
	@echo Cleaning completed
