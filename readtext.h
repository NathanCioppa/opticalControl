
#ifndef CDTEXT_H
#define CDTEXT_H

#include "cd.h"
#include <stdint.h>

typedef struct CDText CDText;

typedef enum ReadTextStatus {
	// general
	SUCCESS, // make sure this stays first (value 0)
	FAILED_TO_ALLOCATE_MEMORY,

	// readText()
	FAILED_TO_OPEN_DEVICE_FILE,
	FAILED_IOCTL,
	CDTEXT_DOES_NOT_EXIST,
	CDTEXT_DATA_EMPTY,

	// setBlock()
	BLOCKNUM_OUT_OF_RANGE,
	BLOCKNUM_NOT_FOUND,

} ReadTextStatus;

ReadTextStatus readText(CDText *dest, uint8_t defaultBlockNum);
ReadTextStatus setBlock(CDText *text, uint8_t blockNum);
void destroyCDText(CDText text);

#endif
