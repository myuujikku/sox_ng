EXTRA_DIST = samples.c

CLEANFILES = samples$(EXE) samples.o

samples$(EXE): samples.c
	$(CC) -o samples$(EXE) samples.c

check-am: samples$(EXE)
	sox=../../src/sox_ng sh $(srcdir)/check.sh -n

dist-hook:
	rm -f samples$(EXE) samples.o
	rm -rf $(distdir)/.deps
