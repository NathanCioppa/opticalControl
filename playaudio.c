
// ALSA PCM interface doumentation: 
// 	https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html
// I tried to stick to these docs for information, but despite seeming to be the most offical docs avalable, they suck.
// Other sources will be sited throughout and I'll try to clarify convoluded things with comments.

#include <alsa/asoundlib.h>
#include "playaudio.h"


#define STEREO 2
#define CD_SAMPLING_RATE 44100
#define TARGET_PCM "default"
#define FRAME_SIZE 4 // Each frames has 2 samples, one for each channel since CD audio is stero, and each sample is 2 bytes (signed 16 bit little endian)
		    // 	Thus, the size of a single frame is 4 bytes

// snd_pcm_writei() writes audio to a pcmHandle. it takes a buffer of frames, and the number of frames.
// each frame is 4 bytes, in the exact format returned from readcd.
// each sample is 16 bit signed little endian (each frame is a left sample and right sample)

struct PCM {
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	void *sampleBuf;
	unsigned long sampleBufSize;
	unsigned int samplingRate;
};

// initializes the passed PCM to a valid PCM.
// If any error occurs, *pcm is unmodified.
InitPCMStatus initPCM(PCM *pcm) {
	int err;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;

	if((err = snd_pcm_open(&handle, TARGET_PCM, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		return FAILED_OPEN_PCM;
	}

	// initialize variables for hardware params
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params); // set default values

	// set parameters.
	// for CD audio:
	// 	accesss is interleaved
	// 	format is signed 16 bit little endian
	// 	there are two channels, since stereo audio
	// 	sampling rate is 44100 bits/second
	//
	// 	None of these are documented in the "actual" ALSA docs, but this article explains them well:
	// 	https://www.linuxjournal.com/article/6735
	if((err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		return FAILED_SET_ACCESS;
	}
	if((err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE)) < 0) {
		return FAILED_SET_FORMAT;
	}
	if((err = snd_pcm_hw_params_set_channels(handle, params, STEREO)) < 0) {
		return FAILED_SET_CHANNELS;
	}
	unsigned int rate = CD_SAMPLING_RATE;
	// uses set_rate_near in case hardware does not support 44100 sampling rate exactly.
	// rate's value will be changed to reflect the actual sampling rate used.
	// NULL is an ignored value which, in other versions of the function, specifies which rate to use if the requested one is not supported.
	// 	I could not find any information on the final param besides asking Chat-GPT so take with a grain of salt. Seems to be accurate in my usage.
	// 	In the example https://gist.github.com/ghedo/963382/815c98d1ba0eda1b486eb9d80d9a91a81d995283, 0 is used as well (technically that param is a pointer so I feel like NULL is more appropriate)
	if((err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, NULL)) < 0) {
		return FAILED_SET_RATE;
	}

	// apply the parameters
	if((err = snd_pcm_hw_params(handle, params)) < 0) {
		return FAILED_SET_PARAMS;
	}

	// from my understanding of https://www.linuxjournal.com/article/6735, a period is the smallest unit of audio data transfer. 
	// get_period_size sets the second param to the number of frames that each period has.
	snd_pcm_uframes_t frames = 0;
	snd_pcm_hw_params_get_period_size(params, &frames, NULL);

	// make the buffer for sending samples exactly as large as one period, in bytes.
	unsigned long transferSize = frames * FRAME_SIZE;
	void *sampleBuf = calloc(1, transferSize);
	if(!sampleBuf)
		return FAILED_ALLOCATE_MEMORY;

	pcm->handle = handle;
	pcm->params = params;
	pcm->sampleBufSize = transferSize;
	pcm->sampleBuf = sampleBuf;
	pcm->samplingRate = rate;

	return SUCCESS;
}

// Free memory associated with the PCM.
// Does not free the actual pointer to the PCM
// Accessing the passed PCM is invalid unless it is re-initialized with initPCM.
void destroyPCM(PCM *pcm) {
	snd_pcm_drain(pcm->handle);
	snd_pcm_close(pcm->handle);
	free(pcm->sampleBuf);
}
