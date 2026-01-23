
#ifndef PLAYAUDIO_H
#define PLAYAUDIO_H

#include <stdint.h>

// error codes for playBufferedAudio()
#define BAD_STATE -1;
#define UNDERRUN -2;
#define SUSPENDED -3;
#define UNKNOWN_ERR -4;

typedef struct PCM PCM;
typedef unsigned long uframes;
typedef long sframes;

int initPCM(PCM **pcm);
void destroyPCM(PCM *pcm);
void setSamples(PCM *pcm, uint8_t *samples);
uframes getTransferLen(PCM *pcm);
uframes getSamplingRate(PCM *pcm);

int startPlayingFrom(uint32_t startLBA, uint32_t leadoutLBA, PCM *pcm);

#endif
