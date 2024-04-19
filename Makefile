# LTL2BA - Version 1.0 - October 2001

# Written by Denis Oddoux, LIAFA, France
# Copyright (c) 2001  Denis Oddoux
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
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# Based on the translation algorithm by Gastin and Oddoux,
# presented at the CAV Conference, held in 2001, Paris, France 2001.
# Send bug-reports and/or questions to: Denis.Oddoux@liafa.jussieu.fr
# or to Denis Oddoux
#       LIAFA, UMR 7089, case 7014
#       Universite Paris 7
#       2, place Jussieu
#       F-75251 Paris Cedex 05
#       FRANCE

# binaries
CC ?= gcc
AR ?= ar
RM ?= rm
LN ?= ln
INSTALL ?= install
REALPATH ?= realpath

# install paths
prefix ?= /usr/local
libdir ?= $(prefix)/lib64
bindir ?= $(prefix)/bin
includedir ?= $(prefix)/include

# compile flags
CPPFLAGS = -MD -Iinc
CFLAGS = $(WARNS)
WARNS  = -Wall -Wextra -Wno-unused

# objects
LTL2C = $(addprefix src/,\
	parse.o lex.o buchi.o set.o \
	mem.o rewrt.o cache.o alternating.o generalized.o \
)

DEPS = $(LTL2C:.o=.d) src/main.d

VERS := $(shell \
	printf '#include "inc/ltl2ba.h"\nLTL2BA_VERSION_MAJOR LTL2BA_VERSION_MINOR' | \
	$(CC) -x c -E - | tail -n1 | tr ' ' .)

# rules
all: ltl2c libltl2ba.a

debug: CFLAGS += -g
debug: all

release: CPPFLAGS += -DNDEBUG
release: CFLAGS += -O2
release: all

ltl2c: ltl2ba
	$(RM) -f ltl2c && $(LN) -s $< $@

libltl2ba.a: $(LTL2C)
	$(AR) rcs $@ $^

ltl2ba: src/main.o libltl2ba.a
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(LTL2C): Makefile

libltl2ba.pc:
	printf "\
# libltl2ba pkg-config source file\\n\
\\n\
prefix=%s\\n\
exec_prefix=\$${prefix}\\n\
libdir=%s\\n\
includedir=%s\\n\
\\n\
Name: libltl2ba\\n\
Description:\\n\
Version: %s\\n\
Libs: -L\$${libdir} -lltl2ba\\n\
Cflags: -I\$${includedir}\\n\
" \
	"$$($(REALPATH) -m $(prefix))" \
	"$$($(REALPATH) -m $(libdir))" \
	"$$($(REALPATH) -m $(includedir))" \
	"$(VERS)" > $@

install: ltl2c ltl2ba inc/ltl2ba.h libltl2ba.a
	$(INSTALL) -D -m 0644 -t $(DESTDIR)$(includedir) inc/ltl2ba.h
	$(INSTALL) -D -m 0644 -t $(DESTDIR)$(libdir) libltl2ba.a
	$(INSTALL) -D -m 0755 -t $(DESTDIR)$(bindir) ltl2ba
	$(RM) -f $(DESTDIR)$(bindir)/ltl2c && \
	$(LN) -s ltl2ba $(DESTDIR)$(bindir)/ltl2c
	$(RM) libltl2ba.pc && \
	$(MAKE) libltl2ba.pc && \
	$(INSTALL) -D -m 0644 -t $(DESTDIR)$(libdir)/pkgconfig libltl2ba.pc

uninstall:
	$(RM) \
		$(DESTDIR)$(includedir)/ltl2ba.h \
		$(DESTDIR)$(libdir)/libltl2ba.a \
		$(DESTDIR)$(bindir)/ltl2ba \
		$(DESTDIR)$(bindir)/ltl2c \
		$(DESTDIR)$(libdir)/pkgconfig/libltl2ba.pc \

clean:
	$(RM) -f ltl2c ltl2ba \
		libltl2ba.a libltl2ba.pc \
		src/main.o $(LTL2C) \
		$(DEPS) \

.PHONY: all clean install uninstall debug release

-include $(DEPS)
