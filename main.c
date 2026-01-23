
#include <stdio.h>
#include <stdlib.h>

#include "readtoc.h"
#include "readtext.h"
#include "playaudio.h"

int main(int argc, char *argv[]) {
	TOC *toc;
	int status = readTOC(&toc); // toc now points to a malloced TOC struct;
	if(status) {
		printf("readTOC failed: %d\n", status);
		return 1;
	}

	CDText *text = NULL;
	status = readText(&text, 0);
	if(status) {
		printReadTextErr(status);
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
	
	char *albumName = NULL; 
	char *albumArtist = NULL;
	char *trackName = NULL;
	char *trackArtist = NULL;

	if(text) {
		albumName = getAlbumName(text); 
		albumArtist = getAlbumArtist(text);
		trackName = getTrackName(text, startTrackNum);
		trackArtist = getTrackArtist(text, startTrackNum);
	}
	if(albumName && *albumName != '\0') {
		printf("Album: %s", albumName);
	}
	if(albumArtist && *albumArtist != '\0') {
		printf(", by %s", albumArtist);
	}
	putchar('\n');
	printf("Starting playback from track %d", startTrackNum);
	if(trackName && *trackName) {
		printf(": %s", trackName);
	}
	if(trackArtist && *trackArtist)
		printf(", by %s", trackArtist);
	putchar('\n');

	if(startPlayingFrom(startLBA, leadoutLBA, pcm))
		printf("BAD\n");
	destroyPCM(pcm);
	return 0;

}

 
