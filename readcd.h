
#ifndef READCD_H
#define READCD_H

#include <stdint.h>

#define CD_AUDIO_BLOCK_SIZE 2352
#define CD_AUDIO_BLOCKS_ONE_SEC 75 // number of CD audio blocks for one second of CD audio
#define READ_CD_AUDIO_LEADOUT_REACHED 6
int readCDAudio(uint32_t startLBA, uint32_t leadoutLBA, uint32_t transferLen, void **dest, long *destSizeWritten);

#endif
