
#ifndef CDTEXT_H
#define CDTEXT_H

#include "cd.h"
#include <stdint.h>

typedef struct CDText CDText;

int readText(CDText **dest, uint8_t defaultBlockNum);
int setBlock(CDText *text, uint8_t blockNum);
void destroyCDText(CDText *text);
void printReadTextErr(int err);

char *getAlbumName(CDText *text);
char *getAlbumArtist(CDText *text);
char *getTrackName(CDText *text, uint8_t trackNum);
char *getTrackArtist(CDText *text, uint8_t trackNum);


#endif
