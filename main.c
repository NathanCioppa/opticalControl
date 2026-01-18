
#include <stdio.h>
#include <stdlib.h>

#include "readtoc.h"
#include "playaudio.h"

int main(int argc, char *argv[]) {
	TOC *toc;
	int status = readTOC(&toc); // toc now points to a malloced TOC struct;
	if(status) {
		printf("readTOC failed: %d\n", status);
		return 1;
	}

	uint8_t startTrackNum = 1;
	if(argc > 1) {
		long numArg;
		char *endp;
		const char *str = argv[1];
		numArg = strtol(str, &endp, 10);
		if(endp == str || *endp != '\0' || numArg < 1 || numArg > 99) {
			printf("invalid arg '%s'\n", str);
			return 3;
		}
		startTrackNum = (uint8_t)numArg;
	}

	if(startTrackNum > getTrackCount(toc)) {
		printf("track number argument '%d' exceeds the track count on this disc.\n", startTrackNum);
		return 4;
	}

	TrackDescriptor *trackN = getTrack(toc, startTrackNum);

	PCM *pcm;
	status = initPCM(&pcm);
	if(status) {
		printf("initPCM failed %d\n", status);
		return 2;
	}

	uint32_t startLBA = getStartLBA(trackN);
	uint32_t leadoutLBA = getLeadoutLBA(toc);
	
	if(startPlayingFrom(startLBA, leadoutLBA, pcm))
		printf("BAD\n");
	destroyPCM(pcm);
	return 0;

}

 
