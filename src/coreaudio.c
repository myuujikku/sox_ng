/* AudioCore sound handler
 *
 * Copyright (C) 2008 Chris Bagwell (cbagwell@sprynet.com)
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

#include <CoreAudio/CoreAudio.h>
#include <pthread.h>

/*
 * Portability garply to use deprecated name on systems without its new alias
 */
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 150000 || __WATCH_OS_VERSION_MIN_REQUIRED >= 80000 || __TV_OS_VERSION_MIN_REQUIRED >= 150000
  /* Use the new constant on newer OS versions */
# define kAudioObjectPropertyElementMaster kAudioObjectPropertyElementMain
#else
  /* Use the deprecated constant on older OS versions */
# define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
# define kAudioObjectPropertyScopeInput  kAudioDevicePropertyScopeInput
# define kAudioObjectPropertyScopeOutput kAudioDevicePropertyScopeOutput
#endif

#define Buffactor 4

typedef struct {
  AudioDeviceID adid;
  AudioDeviceIOProcID adiopid;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int device_started;
  size_t bufsize;
  size_t bufrd;
  size_t bufwr;
  size_t bufrdavail;
  float *buf;
} priv_t;

static OSStatus PlaybackIOProc(AudioDeviceID inDevice UNUSED,
                               const AudioTimeStamp *inNow UNUSED,
                               const AudioBufferList *inInputData UNUSED,
                               const AudioTimeStamp *inInputTime UNUSED,
                               AudioBufferList *outOutputData,
                               const AudioTimeStamp *inOutputTime UNUSED,
                               void *inClientData)
{
    priv_t *ac = (priv_t*)((sox_format_t*)inClientData)->priv;
    AudioBuffer *buf;
    size_t copylen, avail;

    pthread_mutex_lock(&ac->mutex);

    /* CoreAudio output may be split across multiple buffers (often
     * non-interleaved). De-interleave frame-by-frame from SoX's interleaved
     * ring buffer to preserve channel order. */
    if (outOutputData->mNumberBuffers > 1) {
        size_t total_channels = 0;
        size_t frames = (size_t)-1;
        size_t bi;

        for (bi = 0; bi < outOutputData->mNumberBuffers; ++bi) {
            AudioBuffer *b = &outOutputData->mBuffers[bi];
            size_t bchannels;
            size_t bframes;

            if (b->mNumberChannels == 0)
                continue;

            bchannels = b->mNumberChannels;
            bframes = b->mDataByteSize / (sizeof(float) * bchannels);
            if (bframes < frames)
                frames = bframes;
            total_channels += bchannels;
        }

        if (total_channels > 0 && frames != (size_t)-1) {
            size_t frames_available = ac->bufrdavail / total_channels;

            if (frames > frames_available)
                frames = frames_available;

            for (bi = 0; bi < outOutputData->mNumberBuffers; ++bi) {
                AudioBuffer *b = &outOutputData->mBuffers[bi];
                size_t bchannels;

                if (b->mNumberChannels == 0)
                    continue;

                bchannels = b->mNumberChannels;
                b->mDataByteSize = frames * bchannels * sizeof(float);
            }

            for (size_t frame = 0; frame < frames; ++frame) {
                for (bi = 0; bi < outOutputData->mNumberBuffers; ++bi) {
                    AudioBuffer *b = &outOutputData->mBuffers[bi];
                    size_t ch;
                    size_t bchannels;
                    float *dst;

                    if (b->mNumberChannels == 0)
                        continue;

                    bchannels = b->mNumberChannels;
                    dst = b->mData ? ((float*)b->mData + frame * bchannels) : NULL;

                    for (ch = 0; ch < bchannels; ++ch) {
                        float sample = ac->buf[ac->bufrd];
                        if (dst)
                            dst[ch] = sample;

                        ac->bufrd++;
                        if (ac->bufrd == ac->bufsize)
                            ac->bufrd = 0;
                        ac->bufrdavail--;
                    }
                }
            }
        } else {
            for (bi = 0; bi < outOutputData->mNumberBuffers; ++bi)
                outOutputData->mBuffers[bi].mDataByteSize = 0;
        }
    }
    else

    for(buf = outOutputData->mBuffers;
        buf != outOutputData->mBuffers + outOutputData->mNumberBuffers;
        buf++){

        copylen = buf->mDataByteSize / sizeof(float);
        if(copylen > ac->bufrdavail)
            copylen = ac->bufrdavail;

        avail = ac->bufsize - ac->bufrd;
        if(buf->mData == NULL){
            /*do nothing-hardware can't play audio*/
        }else if(copylen > avail){
            memcpy(buf->mData, ac->buf + ac->bufrd, avail * sizeof(float));
            memcpy((float*)buf->mData + avail, ac->buf, (copylen - avail) * sizeof(float));
        }else{
            memcpy(buf->mData, ac->buf + ac->bufrd, copylen * sizeof(float));
        }

        buf->mDataByteSize = copylen * sizeof(float);
        ac->bufrd += copylen;
        if(ac->bufrd >= ac->bufsize)
            ac->bufrd -= ac->bufsize;
        ac->bufrdavail -= copylen;
    }

    pthread_cond_signal(&ac->cond);
    pthread_mutex_unlock(&ac->mutex);

    return kAudioHardwareNoError;
}

static OSStatus RecIOProc(AudioDeviceID inDevice UNUSED,
                          const AudioTimeStamp *inNow UNUSED,
                          const AudioBufferList *inInputData,
                          const AudioTimeStamp *inInputTime UNUSED,
                          AudioBufferList *outOutputData UNUSED,
                          const AudioTimeStamp *inOutputTime UNUSED,
                          void *inClientData)
{
    priv_t *ac = (priv_t *)((sox_format_t*)inClientData)->priv;
    size_t nfree, copylen;
    size_t total_channels = 0;
    size_t frames = (size_t)-1;
    size_t bi;

    pthread_mutex_lock(&ac->mutex);

    /* Input may arrive split across multiple buffers (often non-interleaved).
     * Interleave frame-by-frame into the internal ring buffer. */
    for (bi = 0; bi < inInputData->mNumberBuffers; ++bi) {
        AudioBuffer const *b = &inInputData->mBuffers[bi];
        size_t bchannels;
        size_t bframes;

        if (b->mData == NULL || b->mNumberChannels == 0)
            continue;

        bchannels = b->mNumberChannels;
        bframes = b->mDataByteSize / (sizeof(float) * bchannels);
        if (bframes < frames)
            frames = bframes;
        total_channels += bchannels;
    }

    if (total_channels > 0 && frames != (size_t)-1) {
        nfree = ac->bufsize - ac->bufrdavail - 1;
        copylen = frames * total_channels;

        if (nfree == 0)
            lsx_warn("unhandled buffer overrun. Data discarded.");

        if (copylen > nfree)
            frames = nfree / total_channels;

        for (size_t frame = 0; frame < frames; ++frame) {
            for (bi = 0; bi < inInputData->mNumberBuffers; ++bi) {
                AudioBuffer const *b = &inInputData->mBuffers[bi];
                size_t ch;
                size_t bchannels;
                float const *src;

                if (b->mData == NULL || b->mNumberChannels == 0)
                    continue;

                bchannels = b->mNumberChannels;
                src = (float const *)b->mData + frame * bchannels;

                for (ch = 0; ch < bchannels; ++ch) {
                    ac->buf[ac->bufwr] = src[ch];
                    ac->bufwr++;
                    if (ac->bufwr == ac->bufsize)
                        ac->bufwr = 0;
                    ac->bufrdavail++;
                }
            }
        }
    }

    pthread_cond_signal(&ac->cond);
    pthread_mutex_unlock(&ac->mutex);

    return kAudioHardwareNoError;
}

/* Helper function from https://stackoverflow.com/questions/4575408 */
static size_t DeviceChannelCountInScope(AudioObjectID deviceID,
                                        sox_bool is_input)
{
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector  = kAudioDevicePropertyStreamConfiguration,
        .mScope     = is_input ? kAudioObjectPropertyScopeInput
                               : kAudioObjectPropertyScopeOutput,
        .mElement   = kAudioObjectPropertyElementWildcard
    };
    UInt32 dataSize = 0;
    AudioBufferList *bufferList;
    size_t channels = 0;
    size_t i;

    if (deviceID == kAudioObjectUnknown) return 0;

    if (AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, NULL,
                                       &dataSize) != kAudioHardwareNoError)
        return 0;

    bufferList = lsx_malloc(dataSize);
    if (AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL,
                                   &dataSize, bufferList))
    {
        free(bufferList);
        return 0;
    }

    for (i = 0; i < bufferList->mNumberBuffers; ++i)
        channels += bufferList->mBuffers[i].mNumberChannels;

    free(bufferList);

    return channels;
}

static sox_bool DeviceHasBuffersInScope(AudioObjectID deviceID,
                                        sox_bool is_input)
{
    return DeviceChannelCountInScope(deviceID, is_input) > 0;
}

static int setup(sox_format_t *ft, int is_input)
{
    priv_t *ac = (priv_t *)ft->priv;
    AudioObjectPropertyAddress address = {
        .mScope = is_input ? kAudioObjectPropertyScopeInput
                           : kAudioObjectPropertyScopeOutput
    };
    UInt32 property_size;
    struct AudioStreamBasicDescription stream_desc;
    size_t detected_channels;
    UInt32 buf_size;
    char *io = is_input ? "input" : "output";  /* For use in error messages */

    /* Setup is called twice (why?) so reset adid to Not Found both times */
    ac->adid = kAudioDeviceUnknown;

    if (strncmp(ft->filename, "default", (size_t)7) == 0)
    {
        address.mSelector = is_input ? kAudioHardwarePropertyDefaultInputDevice
                                     : kAudioHardwarePropertyDefaultOutputDevice;
        address.mElement  = kAudioObjectPropertyElementMain;
        property_size = sizeof(ac->adid);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                       &address, 0, NULL,
                                       &property_size, &ac->adid))
        {
            lsx_fail_errno(ft, SOX_EPERM,
                           "there is no default audio %s device", io);
            return SOX_EOF;
        }
    }
    else
    {   /*
	 * Fetch a list of the audio devices and look for the one they want
	 */
        UInt32 datasize = 0;
	      AudioDeviceID *devices;
        int device_count;
	      int i;

        address.mSelector = kAudioHardwarePropertyDevices;
        address.mElement  = kAudioObjectPropertyElementWildcard;
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                           &address, 0, NULL, &datasize)
            || datasize == 0)
        {
nodevices:  lsx_fail_errno(ft, SOX_EPERM,
                           "there appear to be no audio %s devices", io);
            return SOX_EOF;
        }

	devices = lsx_malloc(datasize);
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
				       &address, 0, NULL,
				       &datasize, devices))
	{
	    free(devices);
	    goto nodevices;
	}

	address.mSelector = kAudioDevicePropertyDeviceName;

        /* Allow selection of audio device by number */
        device_count = (int)(datasize / sizeof(AudioDeviceID));
	i = atoi(ft->filename);
        if (i > 0 && i <= device_count) {
	    if (!DeviceHasBuffersInScope(devices[i], is_input)) {
		lsx_warn("audio device %d has no buffers in scope", i);
	    }
            ac->adid = devices[i-1];
        }
	else for (i = 0; i < device_count; i++)
	{
	    char *name;

	    if (!DeviceHasBuffersInScope(devices[i], is_input)) {
		lsx_warn("audio device %d has no buffers in scope", i);
		continue;
	    }
	    datasize = 0;
	    if (AudioObjectGetPropertyDataSize(devices[i], &address,
                                               0, NULL, &datasize))
		continue;

	    name = (char *) lsx_malloc(datasize + 1);
	    if (AudioObjectGetPropertyData(devices[i], &address,
                                           0, NULL, &datasize, name))
	    {
		free(name);
		continue;
	    }
	    name[datasize] = '\0';

	    lsx_report("found audio device %d '%s'", i+1, name);

            /* String returned from OS can be truncated
             * so only compare as much as returned. */
	    if (strncmp(name,ft->filename,datasize) == 0)
	    {
		/* Found it! */
		ac->adid = devices[i];
		free(name);
		break;
	    }
	    free(name);
	}

	if (ac->adid == kAudioDeviceUnknown) {
	   lsx_fail_errno(ft, SOX_EPERM,
	                  "can't find %s device '%s'. Try -V.\n",
                          io, ft->filename);
	   free(devices);
	   return SOX_EOF;
	}
	free(devices);
    }

    if (ac->adid == kAudioDeviceUnknown)
    {
      lsx_fail_errno(ft, SOX_EPERM, "can't open audio device");
      return SOX_EOF;
    }

    /* Query device to get initial values */
    address.mSelector = kAudioDevicePropertyStreamFormat;
    address.mElement  = kAudioObjectPropertyElementMain;
    property_size = sizeof(stream_desc);
    if (AudioObjectGetPropertyData(ac->adid, &address, 0, NULL,
                                   &property_size, &stream_desc))
    {
      lsx_fail_errno(ft, SOX_EPERM, "can't get audio device properties");
      return SOX_EOF;
    }

    if (!(stream_desc.mFormatFlags & kLinearPCMFormatFlagIsFloat)
        || stream_desc.mBitsPerChannel != 32)
    {
      lsx_fail_errno(ft, SOX_EPERM, "the audio device does not accept float32 samples");
      return SOX_EOF;
    }

    /* For input devices, prefer the total channels exposed by stream
     * configuration because mChannelsPerFrame can describe one substream. */
    if (is_input) {
        detected_channels = DeviceChannelCountInScope(ac->adid, sox_true);
        if (detected_channels == 0)
            detected_channels = stream_desc.mChannelsPerFrame;
    }
    else
        detected_channels = stream_desc.mChannelsPerFrame;

    ft->signal.channels = detected_channels ? detected_channels : 2;
    ft->signal.rate = 44100;
    ft->encoding.bits_per_sample = 32;

    /* TODO: My limited experience with hardware can only get floats working
     * with a fixed sample rate and stereo.  I know that is a limitation of
     * audio device I have so this may not be standard operating orders.
     * If some hardware supports setting sample rates and channel counts
     * then should do that over resampling and mixing.
     */
  #if  0
    stream_desc.mSampleRate = ft->signal.rate;
    stream_desc.mChannelsPerFrame = ft->signal.channels;

    /* Write them back */
    property_size = sizeof(struct AudioStreamBasicDescription);
    if (AudioDeviceSetProperty(ac->adid, NULL, 0, is_input,
                                    kAudioDevicePropertyStreamFormat,
                                    property_size, &stream_desc))
    {
      lsx_fail_errno(ft, SOX_EPERM, "can't set audio device properties");
      return SOX_EOF;
    }

    /* Query device to see if it worked */
    property_size = sizeof(struct AudioStreamBasicDescription);
    if (AudioDeviceGetProperty(ac->adid, 0, is_input,
                               kAudioDevicePropertyStreamFormat,
                               &property_size, &stream_desc) )
    {
      lsx_fail_errno(ft, SOX_EPERM, "can't get audio device properties");
      return SOX_EOF;
    }
  #endif

        if (is_input && stream_desc.mChannelsPerFrame != ft->signal.channels)
    {
            lsx_debug("stream format reports %d channels but stream configuration reports %d channels",
                                (int)stream_desc.mChannelsPerFrame, (int)ft->signal.channels);
    }

    if (stream_desc.mSampleRate != ft->signal.rate)
    {
      lsx_debug("audio device did not accept %d sample rate; use %d instead", (int)ft->signal.rate,
                (int)stream_desc.mSampleRate);
      ft->signal.rate = stream_desc.mSampleRate;
    }

    ac->bufsize = sox_globals.bufsiz / sizeof(sox_sample_t) * Buffactor;
    ac->bufrd = 0;
    ac->bufwr = 0;
    ac->bufrdavail = 0;
    lsx_valloc(ac->buf, ac->bufsize);

    buf_size = sox_globals.bufsiz / sizeof(sox_sample_t) * sizeof(float);
    address.mSelector = kAudioDevicePropertyBufferSize;
    property_size = sizeof(buf_size);
    if (AudioObjectSetPropertyData(ac->adid, &address, 0, NULL,
                                   property_size, &buf_size)) {
      lsx_fail_errno(ft, SOX_EPERM, "can't set the audio buffer size");
      free(ac->buf);
      return SOX_EOF;
    }

    if (pthread_mutex_init(&ac->mutex, NULL)) {
      lsx_fail_errno(ft, SOX_EPERM, "failed to initialize the mutex");
      free(ac->buf);
      return SOX_EOF;
    }

    if (pthread_cond_init(&ac->cond, NULL)) {
      lsx_fail_errno(ft, SOX_EPERM, "failed to initialize the condition");
      free(ac->buf);
      return SOX_EOF;
    }

    /* Registers callback with the device without activating it. */
    ac->adiopid = NULL;
    if (AudioDeviceCreateIOProcID(ac->adid, is_input ? RecIOProc : PlaybackIOProc,
                                  (void *)ft, &ac->adiopid) || !ac->adiopid) {
      lsx_fail_errno(ft, SOX_EPERM, "can't register the audio IO callback");
      free(ac->buf);
      return SOX_EOF;
    }
    ac->device_started = 0;

    return SOX_SUCCESS;
}

static int startread_coreaudio(sox_format_t *ft)
{
    return setup(ft, 1);
}

static size_t read_samples_coreaudio(sox_format_t *ft, sox_sample_t *buf, size_t nsamp)
{
    priv_t *ac = (priv_t *)ft->priv;
    size_t len;
    SOX_SAMPLE_LOCALS;

    if (!ac->device_started) {
        if (AudioDeviceStart(ac->adid, ac->adiopid)) {
            lsx_warn("can't start the audio input device");
            return 0;
        }
        ac->device_started = 1;
    }

    pthread_mutex_lock(&ac->mutex);

    /* Wait until input buffer has been filled by device driver */
    while (ac->bufrdavail == 0)
        pthread_cond_wait(&ac->cond, &ac->mutex);

    len = 0;
    while(len < nsamp && ac->bufrdavail > 0){
        buf[len] = SOX_FLOAT_32BIT_TO_SAMPLE(ac->buf[ac->bufrd], ft->clips);
        len++;
        ac->bufrd++;
        if(ac->bufrd == ac->bufsize)
            ac->bufrd = 0;
        ac->bufrdavail--;
    }

    pthread_mutex_unlock(&ac->mutex);

    return len;
}

static int stopread_coreaudio(sox_format_t * ft)
{
  priv_t *ac = (priv_t *)ft->priv;

  if (ac->device_started)
    if (AudioDeviceStop(ac->adid, ac->adiopid) == noErr)
      ac->device_started = 0;
  AudioDeviceDestroyIOProcID(ac->adid, ac->adiopid);
  pthread_cond_destroy(&ac->cond);
  pthread_mutex_destroy(&ac->mutex);
  free(ac->buf);

  return SOX_SUCCESS;
}

static int startwrite_coreaudio(sox_format_t * ft)
{
    return setup(ft, 0);
}

static size_t write_samples_coreaudio(sox_format_t *ft, const sox_sample_t *buf, size_t nsamp)
{
    priv_t *ac = (priv_t *)ft->priv;
    size_t i;

    SOX_SAMPLE_LOCALS;

    pthread_mutex_lock(&ac->mutex);

    /* Wait to start until mutex is locked to help prevent callback
     * getting zero samples.
     */
    if (!ac->device_started) {
        if (AudioDeviceStart(ac->adid, ac->adiopid)) {
            pthread_mutex_unlock(&ac->mutex);
            lsx_warn("can't start the audio output device");
            return SOX_EOF;
        }
        ac->device_started = 1;
    }

    /* globals.bufsize is in samples
     * buf_offset is in bytes
     * buf_size is in bytes
     */
    for(i = 0; i < nsamp; i++){
        while(ac->bufrdavail == ac->bufsize - 1)
            pthread_cond_wait(&ac->cond, &ac->mutex);

        ac->buf[ac->bufwr] = SOX_SAMPLE_TO_FLOAT_32BIT(buf[i], ft->clips);
        ac->bufwr++;
        if(ac->bufwr == ac->bufsize)
            ac->bufwr = 0;
        ac->bufrdavail++;
    }

    pthread_mutex_unlock(&ac->mutex);
    return nsamp;
}


static int stopwrite_coreaudio(sox_format_t * ft)
{
    priv_t *ac = (priv_t *)ft->priv;

    if(ac->device_started){
        pthread_mutex_lock(&ac->mutex);

        while (ac->bufrdavail > 0)
            pthread_cond_wait(&ac->cond, &ac->mutex);

        pthread_mutex_unlock(&ac->mutex);

        if (AudioDeviceStop(ac->adid, ac->adiopid) == noErr)
            ac->device_started = 0;
    }

    AudioDeviceDestroyIOProcID(ac->adid, ac->adiopid);
    pthread_cond_destroy(&ac->cond);
    pthread_mutex_destroy(&ac->mutex);
    free(ac->buf);

    return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(coreaudio)
{
  static char const *const names[] = { "coreaudio", NULL };
  static unsigned const write_encodings[] = {
    SOX_ENCODING_FLOAT, 32, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Mac AudioCore device driver",
    names, SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
    startread_coreaudio, read_samples_coreaudio, stopread_coreaudio,
    startwrite_coreaudio, write_samples_coreaudio, stopwrite_coreaudio,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
