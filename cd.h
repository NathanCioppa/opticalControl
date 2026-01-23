
#ifndef CD_H
#define CD_H

#include "readtext.h"

//typedef struct CD CD;
typedef struct Track Track;

//struct CD {
//	CDText *text;
//};

struct Track {
	char *title;
	char *artist;
};

#endif
