
#include <stdio.h>

#include "readtoc.h"
#include "readcd.h"
#include "playaudio.h"

int main() {
	TOC *toc;
	int status = readTOC(&toc); // toc now points to a malloced TOC struct;
	if(status) {
		printf("readTOC failed: %d\n", status);
		return 1;
	}

	PCM *pcm;
	status = initPCM(&pcm);
	if(status) {
		printf("initPCM failed %d\n", status);
		return 2;
	}

	void *samples = NULL;
	status = readCDAudio(5000, 500, &samples);
	if(status) {
		printf("readaudio failed: %d\n", status);
		return 3;
	}

	//for(int i=0; i<1000; i++) {
	//	printf("%d ", ((uint8_t *)samples)[i]);
	//}
	//printf("\n################\n");

	long offset = 0;
	while(1) {
		setSamples(pcm, samples+offset);
		offset += writeBuffer(pcm) * 4;
		//printf("%ld\n",offset);
	}
	
	
	//setSamples(pcm, samples);
	//writeBuffer(pcm);
	

	return 0;
}
