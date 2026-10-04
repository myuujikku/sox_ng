# README-sox_ng

Apart from bug fixes, what's in `sox_ng-14.8` compared to old `sox-14.4.2`?

## New file formats

* `-t ffmpeg` reads `3g2` `3gp` `aa` `aac` `ac3` `act` `adts` `adx` `ape`
  `apm` `aptx` `argo_asf, asf` `ast` `avi` `dfpwm` `dts` `dvd` `ea` `eac3`
  `f4v` `flv` `gxf` `ism` `kvag` `m4a` `m4v, mkv` `mlp` `mp4` `mpeg` `mpegts`
  `mxf` `nut` `oga (Ogg with FLAC data)` `ra` `rm, rso` `sbc` `smjpeg` `spdif`
  `speex` `svcd` `tta` `vag` `vcd` `vob` `webm` `wma` `wsaud` `wtv` and more
* Reads and writes `dsf` and reads `dff` and `wsd`
* `hip`: LAME's built-in MP3 decoder
* `mod`: Reads (Amiga modules)
* `mpc`/`mpc2k`: Reads (Akai samplers)
* `mul`: Writes (TombRaider Multiplex Streams)
* `nsp`: Reads (Computerized Speech Lab)
* `pcm`: Reads and writes (Loop file for SNES add-on MSU-1)

and `main`, which will be `14.9`, can encode `opus` files.

## New file subformats

* `aifc`: Reads and writes a-law and u-law encodings
* `aifc`: Supports 24/32-bit and big/little-endian compression
* `aifc`/`aiff`: Copies MARK and INST chunks
* `coreaudio` (Mac): Also allow audio device selection by number
* `mp2`: Enables VBR encoding with `-C` from -10 to 10
* `mp3`: Mixes 3 or more channels down to stereo
* `oga`: Reads Ogg FLAC files (with ffmpeg)
* `pulseaudio`: Filename can set server and application names
* `sd2`: Supports resource forks
* `sndfile`: Also handle `avr` format
* `sph`: Supports  ALAW encoding
* `wavpack`: Supports 64-bit file lengths
* The ID3 `TCOM` (Composer) tag is now supported

## New effects

* `centercut`: A sophisticated version of `oops`
* `chorus` and `echos` have never worked but now do
* `dolbyb`: Decodes and encodes Dolby B noise reduction
* `dop` and `sdm`, related to DSD formats
* `saturation`: Soft clipping from subtle warmth to crunchy fuzz
* `softvol`: An automatic volume control that avoids clipping
* `speexdsp`: Automatic gain control and noise reduction

## New effect options/improvements

* `chorus`: All parameters are now optional
* `chorus`: An initial `-s` or `-t` sets the default wave shape
* `chorus`: `-l` and `-q` give linear or quadratic interpolation
* `dither`: [New Shibata filters from SSRC](wiki/ShaperCoefs)
* `fade`: `s` for square-law fade type
* `flanger`: `-n` for no interpolation
* `flanger`: An initial `-s` or `-t` sets the default wave shape
* `ladspa`: Multi-channel effects now work
* `ladspa`: Effects that take no input now work
* `rate`: Extrapolates by Linear Predictive Coding to improve end effects
* `sinc -d`: Copy through when the lowpass frequency is above Nyquist
* `spectrogram`: Make `-Y` give exactly the requested height
* `spectrogram -g`: Show times on the axis as HH:MM:SS
* `spectrogram -i`: Interpolate between frequency bins for smooth shading
* `spectrogram -n`: Normalizes the brightness
* `spectrogram -L`: Gives a logarithmic frequency axis
* `spectrogram -R`: Specifies the frequency range
* `stat -e`: Give EBU-R-128 loudness measurements
* `stat -j`: Output in JSON format
* `stretch`: `sqrt`, `quarter-cos` and `half-cos` fade types
* `synth`: `vdelay` combine method phase-modulates by the synthed wave
* `synth -p`: Selects Pythagorean tuning

## New global options

* `-A`: Retune A4 to something other than 440Hz
* `-h foo`: Gives help for format or effect `foo`
* `--keymap`: Let key presses change effect parameters while playing
* In `--interactive` mode (e.g. when playing)
** `<` and `>` seek back and forward by 30 seconds
** `n` skips to the next input file
** `R` restarts the effects chain
** `q` and `Esc` make it quit

## Changes to limits

* `bend`: Maximum FFT size from 8192 to 2^30
* `chorus`: Number of stages from 7 to 256
* `echo/echos`: Number of stages from 7 to unlimited
* `flanger`: Minimum speed from 0.1 to 0.01Hz
* `flanger`: Fix range of `width` from -100..100 to 0..infinity
* `hilbert/sinc`: Maximum number of taps to 1073741823
* `phaser`: Number of channels from 4 to unlimited
* `phaser`: Fix range of `regen` to -1..+1
* `riaa`: Add 192kHz sample rate
* `spectrogram`: Total height from 8193 to 200000 pixels
* `stretch`: Impose minimum window size of 1ms

## Documentation

* The manual for the effects has moved from sox_ng(1) to soxeffects_ng(7)
* The manual for `libsox_ng(3)` is now complete and accurate
