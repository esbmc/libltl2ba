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

CC ?= gcc
AR ?= ar

CPPFLAGS = -MD
CFLAGS = $(WARNS)
WARNS  = -Wall -Wextra -Wno-unused

LTL2C =	parse.o lex.o buchi.o set.o \
	mem.o rewrt.o cache.o alternating.o generalized.o

all: ltl2c libltl2ba.a

ltl2c: ltl2ba
	rm -f ltl2c && ln -s $< $@

libltl2ba.a: $(LTL2C)
	$(AR) rcs $@ $^

ltl2ba: main.o libltl2ba.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(LTL2C): Makefile

clean:
	rm -f ltl2c ltl2ba libltl2ba.a main.o main.d $(LTL2C) $(LTL2C:.o=.d)

.PHONY: all clean

-include $(wildcard *.d)
