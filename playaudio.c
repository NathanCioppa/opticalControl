
// ALSA PCM interface doumentation: 
// 	https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html
// I tried to stick to these docs for information, but despite seeming to be the most offical docs avalable, they suck.
// Other sources will be sited throughout and I'll try to clarify convoluded things with comments.

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include "playaudio.h"
#include "readcd.h"

#define STEREO 2
#define CD_SAMPLING_RATE 44100 // frames per second
#define PCM_BUF_BEFORE_BLOCKING (CD_SAMPLING_RATE / 2) // pcm only buffers this much audio
#define TARGET_PCM "default"
#define FRAME_SIZE 4 // Each frames has 2 samples, one for each channel since CD audio is stero, and each sample is 2 bytes (signed 16 bit little endian)
		    // 	Thus, the size of a single frame is 4 bytes
#define PERIODS_TO_BUFFER 4 // try to keep at least this many periods in the PCM at a time for smooth playback

#define CD_AUDIO_BLOCKS_TO_BUFFER (CD_AUDIO_BLOCKS_ONE_SEC * 2)

#define SUCCESS 0
#define FAILED_OPEN_PCM 1
#define FAILED_SET_ACCESS 2
#define FAILED_SET_FORMAT 3
#define FAILED_SET_CHANNELS 4
#define FAILED_SET_RATE 5
#define FAILED_SET_PARAMS 6
#define FAILED_ALLOCATE_MEMORY 7
#define FAILED_SET_BUF 8

sframes writeFramesForPlayback(PCM *pcm, void *frameBuf, snd_pcm_uframes_t framesInBuf);

struct PCM {
	snd_pcm_t *handle;
	uframes transferLen; // the desired number of frames to send to snd_pcm_writei() at a time
	uframes samplingRate;
};

// initializes the passed PCM to a valid PCM.
// If any error occurs, dest is unmodified.
int initPCM(PCM **dest) {
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

	// Sets the number of frames the PCM handle can accept before blocking, keep small to avoid latency when draining the pcm
	snd_pcm_uframes_t bufFrames = PCM_BUF_BEFORE_BLOCKING;
	if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &bufFrames) < 0))
		return FAILED_SET_BUF;

	// apply the parameters
	if((err = snd_pcm_hw_params(handle, params)) < 0) {
		return FAILED_SET_PARAMS;
	}

	// from my understanding of https://www.linuxjournal.com/article/6735, a period is the smallest unit of audio data transfer to the PCM. 
	// get_period_size sets the second param to the number of frames that each period has.
	// For now this application just uses whatever the default value is and does not attempt to set it's own.
	snd_pcm_uframes_t periodFrames = 0;
	snd_pcm_hw_params_get_period_size(params, &periodFrames, NULL);

	uframes transferLen = periodFrames * PERIODS_TO_BUFFER;
	
	PCM *pcm = malloc(sizeof(PCM));
	if(!pcm)
		return FAILED_ALLOCATE_MEMORY;

	pcm->handle = handle;
	pcm->transferLen = transferLen;
	pcm->samplingRate = rate;

	*dest = pcm;

	snd_pcm_prepare(pcm->handle);

	return SUCCESS;
}

// Free memory associated with the PCM.
// Does not free the actual pointer to the PCM
// Accessing the passed PCM is invalid unless it is re-initialized with initPCM.
void destroyPCM(PCM *pcm) {
	snd_pcm_drain(pcm->handle);
	snd_pcm_close(pcm->handle);
}

// writes framesInBuf frames from frameBuf to the PCM's buffer to be played
// returns the number of frames written, otherwise a negative error code;
sframes writeFramesForPlayback(PCM *pcm, void *frameBuf, snd_pcm_uframes_t framesInBuf) {
	snd_pcm_sframes_t framesWritten = snd_pcm_writei(pcm->handle, frameBuf, framesInBuf);
	if(framesWritten >= 0)
		return framesWritten;
	if(framesWritten == -EBADFD)
		return BAD_STATE;
	if(framesWritten == -EPIPE)
		return UNDERRUN;
	if(framesWritten == -ESTRPIPE)
		return SUSPENDED;
	return UNKNOWN_ERR;
}

snd_pcm_uframes_t getTransferLen(PCM *pcm) {
	return pcm->transferLen;
}

uframes getSamplingRate(PCM *pcm) {
	return pcm->samplingRate;
}


int startPlayingFrom(uint32_t startLBA, uint32_t leadoutLBA, PCM *pcm) {
	void *framesBuf = NULL;
	long framesBufSize = 0;
	bool leadoutReached = false;

	for(int buffersFilled = 0; !leadoutReached; buffersFilled++) {
		//printf("NEW BUFF\n");
		int status = readCDAudio(startLBA+(buffersFilled*CD_AUDIO_BLOCKS_TO_BUFFER), leadoutLBA, CD_AUDIO_BLOCKS_TO_BUFFER, &framesBuf, &framesBufSize);
		if(status ==  READ_CD_AUDIO_LEADOUT_REACHED) {
			//printf("LEADOUT\n");
			leadoutReached = true;
		}
		else if(status) {
			printf("readaudio failed: %d\n", status);
			return 3;
		}

		// TODO error handling for writeFramesForPlayback() calls

		long offset = 0; // offset is in bytes, always incremented in multiples of FRAME_SIZE
		uframes framesPerTransfer = getTransferLen(pcm);
		while(offset < framesBufSize - (framesPerTransfer*FRAME_SIZE)) {
			sframes framesWritten = writeFramesForPlayback(pcm, framesBuf+offset, framesPerTransfer);
			if(framesWritten >= 0) {
				offset += framesWritten*FRAME_SIZE;
				continue;
			}
		}
		sframes remainder = (framesBufSize - offset)/FRAME_SIZE;
		sframes framesWritten = writeFramesForPlayback(pcm, framesBuf+offset, remainder);
	}
	return 0;
}
