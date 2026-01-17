
#ifndef PLAYAUDIO_H
#define PLAYAUDIO_H

#include <stdint.h>

// error codes for playBufferedAudio()
#define BAD_STATE -1;
#define UNDERRUN -2;
#define SUSPENDED -3;
#define UNKNOWN_ERR -4;

typedef struct PCM PCM;

int initPCM(PCM **pcm);
void destroyPCM(PCM *pcm);
void setSamples(PCM *pcm, uint8_t *samples);
unsigned long getTransferSize(PCM *pcm);
unsigned int getSamplingRate(PCM *pcm);
unsigned long getAudioQueueSize(PCM *pcm);

int startPlayingFrom(uint32_t startLBA, uint32_t leadoutLBA, PCM *pcm);

#endif
