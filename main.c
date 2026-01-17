
#include <stdio.h>

#include "readtoc.h"
#include "playaudio.h"

int main() {
	TOC *toc;
	int status = readTOC(&toc); // toc now points to a malloced TOC struct;
	if(status) {
		printf("readTOC failed: %d\n", status);
		return 1;
	}

	TrackDescriptor *tracks = getTracks(toc);
	TrackDescriptor *trackN = getTrack(tracks, 19);

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

 
