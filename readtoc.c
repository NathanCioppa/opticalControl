
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "readtoc.h"

#define CDB_SIZE 10
#define iOPCODE 0
#define OPCODE 0x43
#define iFORMAT 2
#define FORMAT 0x00
#define iALLOC_LEN_LSBYTE 8 // index of least significant byte of allocation length in cdb
#define iALLOC_LEN_MSBYTE 7 // index of most significant byte of allocation length in cdb
#define ALLOC_LEN 796 // max number of tracks is 99, each track descriptor is 8 bytes, response header is 4 bytes.
		      // (99 * 8) + 4 = 796

#define DEVICE_FILE "/dev/sg0"
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define MAX_SENSE_BUF_LEN 64

#define ONE_BYTE 8


uint16_t getAllocLen(uint8_t *readTocResponse);

struct TOC {
	void *trackDescriptors;
	uint16_t trackDescriptorsSize;
	uint8_t firstTrackNum;
	uint8_t lastTrackNum; 
};

int main() {
	TOC toc;
	if(readTOC(&toc)) {
		return 1;
	}
	printf("%d, %d, %d\n", toc.firstTrackNum, toc.lastTrackNum, toc.trackDescriptorsSize);
	for(int i=0; i<toc.trackDescriptorsSize; i+=8) {
		printf("%d\n", ((uint8_t *)(toc.trackDescriptors+i))[2]);
	}
	return 0;
}

// Return value indicates either success of command or an indicator of faliliure.
// Return values are in readtoc.h
// On success, value of *trackCount is set to the number of tracks on the CD. 
READ_TOC_STATUS readTOC(TOC *dest) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return FAILED_TO_OPEN_DEVICE; 
	}

	// only a very simple command descriptor block is needed
	uint8_t cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	cdb[iALLOC_LEN_LSBYTE] = (uint8_t)ALLOC_LEN;
	cdb[iALLOC_LEN_MSBYTE] = (uint8_t)(ALLOC_LEN >> ONE_BYTE);
	
	// Documentation for sg_io_hdr_t type (at least the best docs I could find):
	// https://sg.danny.cz/sg/p/scsi-generic_v3.txt
	sg_io_hdr_t hdr;
	memset(&hdr, 0, sizeof(sg_io_hdr_t));
	
	unsigned char dxferp[ALLOC_LEN];
	memset(dxferp, 0, ALLOC_LEN);

	unsigned char senseBuf[MAX_SENSE_BUF_LEN];
	memset(senseBuf, 0, MAX_SENSE_BUF_LEN);

	hdr.interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.dxferp = dxferp;
	hdr.dxfer_len = ALLOC_LEN;
	hdr.cmd_len = CDB_SIZE;
	hdr.mx_sb_len = MAX_SENSE_BUF_LEN;
	hdr.cmdp = cdb;
	hdr.sbp = senseBuf;
	hdr.timeout = 5000;

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		close(fd);
		return IOCTL_FAIL;
	}
	close(fd);

	if(hdr.sb_len_wr != 0) {
		return BAD_SENSE_DATA; 
	}

	uint16_t tocDataLen = getAllocLen(dxferp);
	if(tocDataLen <= 2)
		return NO_TOC_DATA_FOUND;
	TOC toc;
	uint16_t trackDescriptorsLen;
	if(tocDataLen < ALLOC_LEN)
		trackDescriptorsLen = tocDataLen - 2;
	else
		trackDescriptorsLen = ALLOC_LEN - 4;

	void *trackDescriptors = malloc(trackDescriptorsLen);
	if(!trackDescriptors) 
		return FAILED_TO_ALLOCATE_MEMORY;

	memcpy(trackDescriptors, dxferp+4, trackDescriptorsLen);

	toc.trackDescriptorsSize = trackDescriptorsLen;
	toc.trackDescriptors = trackDescriptors;
	toc.firstTrackNum = dxferp[2];
	toc.lastTrackNum = dxferp[3];

	*dest = toc;
	return SUCCESS;
}

uint16_t getAllocLen(uint8_t *readTocResponse) {
	uint8_t msbyte = *readTocResponse;
	uint8_t lsbyte = *(readTocResponse+1);
	uint16_t ret = msbyte;
	ret = ret << ONE_BYTE;
	ret |= lsbyte;
	return ret;
}
