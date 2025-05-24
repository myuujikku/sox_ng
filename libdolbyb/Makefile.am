# Makefile.am Copyright (C) 2025 Martin Guy <martinwguy@gmail.com>
# Based on Sox's Makefile.am Copyright (C) 2006 Reuben Thomas <rrt@sc3d.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS=-I m4
AM_CFLAGS = @WARN_CFLAGS@

#bin_PROGRAMS = dolbyb
#dolbyb_SOURCES = dolbyb.c
#dolbyb_LDADD = libdolbyb.la -lsndfile
include_HEADERS = dolbyb.h
lib_LTLIBRARIES = libdolbyb.la
#dist_man_MANS = dolbyb.1 libdolbyb.3
dist_man_MANS = libdolbyb.3
EXTRA_DIST = COPYING INSTALL

libdolbyb_la_SOURCES = \
     Calibrate.c DCfilter.c DiodeClip.c FindOutSmp.c HPF1.c HPF2.c \
     HPF2SetVals.c HPFAdj.c LPFAdj.c Param.c SetGate.c SidePath.c \
     libdolbyb.c \
     Calibrate.h DCfilter.h DiodeClip.h FindOutSmp.h HPF1.h HPF2.h \
     HPF2SetVals.h HPFAdj.h LPFAdj.h Mixers.h Param.h SetGate.h SidePath.h \
     dolbyb.h

libdolbyb_la_CFLAGS = @WARN_CFLAGS@
libdolbyb_la_LDFLAGS = @APP_LDFLAGS@ -version-info @SHLIB_VERSION@ \
  -export-symbols-regex '^dolbyb_(init|start|(en|de)code|free|dumpparam)$$'

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

DolbyBi64: DolbyBi64.PP LibI64/*.Lib
	fpc DolbyBi64.PP

tags:
	ctags *.[ch]

clean-local:
	rm -rf .deps autom4te.cache
	rm -f config.log configure~ \
	      stamp-h1 libdolbyb-*.tar.gz \
	      tags dolbyb DolbyBi64
