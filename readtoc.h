
#ifndef READ_TOC_H
#define READ_TOC_H

typedef struct TOC TOC;
typedef struct TrackDescriptor TrackDescriptor;

typedef enum READ_TOC_STATUS {
	SUCCESS,
	FAILED_TO_ALLOCATE_MEMORY,
	NO_TOC_DATA_FOUND,
	FAILED_TO_OPEN_DEVICE,
	IOCTL_FAIL,
	BAD_SENSE_DATA,
	INSUFFICIENT_BUFFER_SIZE,
} READ_TOC_STATUS;

READ_TOC_STATUS readTOC(TOC *dest);
void destroyTOC(TOC toc);

#endif
