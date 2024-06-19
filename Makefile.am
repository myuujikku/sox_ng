## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lpc10 libgsm src
DIST_SUBDIRS = lpc10 libgsm src msvc9 msvc10

RM = rm -f

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox_ng.1 soxi_ng.1 soxformat_ng.7 libsox_ng.3
EXTRA_DIST = sox_ng.txt soxi_ng.txt soxformat_ng.txt libsox_ng.txt \
	     CMakeLists.txt sox_ng.pc.in LICENSE.GPL LICENSE.LGPL \
	     cygbuild README.win32 README.osx scripts/batch-example.bat \
	     README.sh FEATURES.in

if HAVE_PKGCONFIG
pkgconfigdir = @PKGCONFIGDIR@
pkgconfig_DATA = sox_ng.pc
endif

play_ng.1 rec_ng.1: sox_ng.1
	$(RM) $@ && $(LN_S) $< $@

README: README.sh FEATURES.in
	./README.sh

# Rule for making text man pages.  Uses GNU nroff's -c so not portable.
.1.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@
.3.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@
.7.txt:
	tbl $(srcdir)/$< | nroff -man -c | sed "s/.//g" > $@

DOCTXT = sox_ng.txt soxi_ng.txt soxformat_ng.txt libsox_ng.txt README
txt: $(DOCTXT)

# Rule for making PDF man pages
.1.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@
.3.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@
.7.pdf:
	pdfroff -t -man -Tps $(srcdir)/$< > $@

DOCPDF = sox_ng.pdf soxi_ng.pdf soxformat_ng.pdf libsox_ng.pdf
pdf: $(DOCPDF)

install-pdf: pdf
	@$(NORMAL_INSTALL)
	test -z "$(pdfdir)" || $(MKDIR_P) "$(DESTDIR)$(pdfdir)"
	@list='$(DOCPDF)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=$(am__strip_dir) \
	  echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

uninstall-pdf:
	@$(NORMAL_UNINSTALL)
	@list='$(DOCPDF)'; for p in $$list; do \
	  f=$(am__strip_dir) \
	  echo " $(RM) '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(RM) "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

distclean-local:
	$(RM) mingw32-config.cache
	$(RM) -r -f soxpng

# Rule for making HTML man pages
#
# Get rid on unneeded Content-Type, man directory and manpage index in extension.
# e.g. ../man1/soxi_ng.1.html becomes ../soxi_ng.html
# Return to Main Contents and link to man2html are also modified.
FIXHTML = sed 's,<head>,&<link rel="stylesheet" href="http://sox.sourceforge.net/pub/skins/sox/sox.css" type="text/css"></link>,' 

.1.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.3.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.7.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@

DOCHTML = sox_ng.html soxi_ng.html soxformat_ng.html libsox_ng.html
html: $(DOCHTML)

install-html: html 	 
	@$(NORMAL_INSTALL) 	 
	test -z "$(htmldir)/soxpng" || $(MKDIR_P) "$(DESTDIR)$(htmldir)/soxpng"
	@list='$(DOCHTML)'; for p in $$list; do \
	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)//$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	@for p in soxpng/*; do \
	if test -f "$$p"; then d=; else d="$(srcdir)"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)/soxpng/$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/soxpng/$$f"; \
	done

uninstall-html: 	 
	@$(NORMAL_UNINSTALL)
	@list='$(DOCHTML)'; for p in $$list; do \
	f=$(am__strip_dir) \
	echo " $(RM) '$(DESTDIR)$(htmldir)/$$f'"; \
	$(RM) "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	$(RM) -fr "$(DESTDIR)$(htmldir)/soxpng";

DISTCLEANFILES = $(DOCHTML) $(DOCPDF)
MAINTAINERCLEANFILES = $(DOCTXT)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && $(RM) play_ng.1 && $(LN_S) sox_ng.1 play_ng.1
	cd $(DESTDIR)$(mandir)/man1 && $(RM) rec_ng.1 && $(LN_S) sox_ng.1 rec_ng.1
	cd $(DESTDIR)$(mandir)/man7 && $(RM) soxeffect_ng.7 && $(LN_S) ../man1/sox_ng.1 soxeffect_ng.7

uninstall-hook:
	$(RM) $(DESTDIR)$(mandir)/man1/play_ng.1
	$(RM) $(DESTDIR)$(mandir)/man1/rec_ng.1
	$(RM) $(DESTDIR)$(mandir)/man7/soxeffect_ng.7

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
