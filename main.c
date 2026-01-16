
#include <stdio.h>

#include "readtoc.h"
#include "readcd.h"
#include "playaudio.h"

// buffer roughly 2 seconds of audio data
#define CD_AUDIO_BLOCKS_TO_BUFFER (CD_AUDIO_BLOCKS_ONE_SEC * 2)

int main() {
	TOC *toc;
	int status = readTOC(&toc); // toc now points to a malloced TOC struct;
	if(status) {
		printf("readTOC failed: %d\n", status);
		return 1;
	}

	TrackDescriptor *tracks = getTracks(toc);
	TrackDescriptor *trackN = getTrack(tracks, 1);

	PCM *pcm;
	status = initPCM(&pcm);
	if(status) {
		printf("initPCM failed %d\n", status);
		return 2;
	}

	void *sampleBuf = NULL;
	uint32_t startLBA = getStartLBA(trackN);
	status = readCDAudio(startLBA, CD_AUDIO_BLOCKS_TO_BUFFER, &sampleBuf);
	if(status) {
		printf("readaudio failed: %d\n", status);
		return 3;
	}
	const long sampleBufSize = CD_AUDIO_BLOCK_SIZE * CD_AUDIO_BLOCKS_TO_BUFFER;

	for(int i = 0; 1; i++) {


		readCDAudio(startLBA+(i*CD_AUDIO_BLOCKS_TO_BUFFER), CD_AUDIO_BLOCKS_TO_BUFFER, &sampleBuf);

		long offset = 0;
		while(offset < sampleBufSize - getTransferSize(pcm)) {
			setSamples(pcm, sampleBuf+offset);
			long bytesWritten = playBufferedAudio(pcm);
			if(bytesWritten >= 0) {
				offset+=bytesWritten;
				continue;
			}
		}
	}
	

	return 0;
}
