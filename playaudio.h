
#ifndef PLAYAUDIO_H
#define PLAYAUDIO_H

#include <stdint.h>

typedef struct PCM PCM;

int initPCM(PCM **pcm);
void destroyPCM(PCM *pcm);
void setSamples(PCM *pcm, uint8_t *samples);
long writeBuffer(PCM *pcm);

#endif
