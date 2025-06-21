# This is the MXE control file for Windows cross-compilation.

# In brief:
# * Install the packages it depends on (see https://mxe.cc/#requirements)
# * git clone https://github.com/mxe/mxe.git
# * cd mxe
# * Put this file in src/
# * make sox_ng
# and `sox_ng.exe` will appear, after a long pause, as
#   usr/i686-w64-mingw32.static/bin/sox_ng.exe
#
# For the 64-bit version,
# * make MXE_TARGETS=x86_64-w64-mingw32.static sox_ng

PKG             := sox_ng
$(PKG)_WEBSITE  := https://codeberg.org/sox_ng/sox_ng
$(PKG)_DESCR    := SoX
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 14.6.0
$(PKG)_CHECKSUM := 6163d805372c3769b17a7bf858890fae6a1d21249c0404d35992b3d429f205e9
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $(PKG)-$($(PKG)_VERSION).tar.gz
$(PKG)_URL      := https://codeberg.org/sox_ng/$(PKG)/releases/download/$($(PKG)_SUBDIR)/$($(PKG)_FILE)
$(PKG)_DEPS     := cc fftw file flac lame libid3tag libltdl libmad libpng \
                   libsndfile opencore-amr opusfile speex speexdsp twolame \
		   vorbis wavpack

define $(PKG)_UPDATE
     echo 'TODO: write update script for $(PKG).' >&2
     echo $($(PKG)_VERSION)
endef

define $(PKG)_BUILD
    # set pkg-config cflags and libs
    $(SED) -i 's,^\(Cflags:.*\),\1 -fopenmp,' '$(1)/sox_ng.pc.in'
    $(SED) -i '/Libs.private/d'               '$(1)/sox_ng.pc.in'
    echo Libs.private: @MAGIC_LIBS@ \
        `grep sox_ng_LDADD '$(1)/src/optional-fmts.am' | \
         $(SED) 's, sox_ng_LDADD += ,,g' | tr -d '\n'` @LIBS@ >>'$(1)/sox_ng.pc.in'

    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
        --prefix='$(PREFIX)/$(TARGET)' \
        --build="`config.guess`" \
        --disable-shared \
        --enable-static \
        --disable-debug \
        --with-libltdl \
        --with-magic \
        --with-png \
        --with-ladspa \
        --with-amrwb \
        --with-amrnb \
        --with-flac \
        --with-oggvorbis \
        --with-sndfile \
        --with-wavpack \
        --with-mad \
        --with-id3tag \
        --with-lame \
        --with-twolame \
        --with-waveaudio \
        --with-ffmpeg \
        --without-alsa \
        --without-ao \
        --without-coreaudio \
        --without-oss \
        --without-pulseaudio \
        --without-sndio \
        --without-sunaudio \
	CFLAGS='-m32 -fno-pie' \
	LDFLAGS='-m32 -fno-pie' \
        LIBS='-lshlwapi -lgnurx'

    $(MAKE) -C '$(1)' -j '$(JOBS)' bin_PROGRAMS= EXTRA_PROGRAMS=
    $(MAKE) -C '$(1)' -j 1 install
endef

$(PKG)_BUILD_SHARED =
