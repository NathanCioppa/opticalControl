
#ifndef READ_TOC_H
#define READ_TOC_H

#include <stdint.h>

typedef struct TOC TOC;
typedef struct TrackDescriptor TrackDescriptor;

int readTOC(TOC **dest);
void destroyTOC(TOC *toc);

TrackDescriptor *getTracks(TOC *toc);
TrackDescriptor *getTrack(TrackDescriptor *tracks, uint8_t trackNum);
uint8_t getTracksLen(TOC *toc);
uint8_t getFirstTrackNumber(TOC *toc);
uint8_t getTrackCount(TOC *toc);
uint32_t getStartLBA(TrackDescriptor *track);
uint8_t getTrackNumber(TrackDescriptor *track);
uint32_t getLeadoutLBA(TOC *toc);
#endif
