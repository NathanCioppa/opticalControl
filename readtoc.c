
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
// Make sure ALLOC_LEN is sufficiently large to accomidate all tracks, an error will be given if not.
#define ALLOC_LEN 804 // max number of tracks is 99, plus 1 descriptor for start of lead out (track 0xAA)
		      // each track descriptor is 8 bytes, response header is 4 bytes.
		      // (100 * 8) + 4 = 804

#define DEVICE_FILE "/dev/sg0"
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define MAX_SENSE_BUF_LEN 64

#define ONE_BYTE 8
#define TRACK_DESCRIPTOR_SIZE 8	
#define FIRST_TRACK_NUM 2
#define LAST_TRACK_NUM 3
#define RESPONSE_HEADER_SIZE 4
#define REPRESENTED_HEADER_SIZE 2
#define CONTROL_MASK 0b00001111


#define SUCCESS 0
#define FAILED_ALLOCATE_MEMORY 1
#define NO_TOC_DATA_FOUND 2
#define FAILED_OPEN_DEVICE 3
#define IOCTL_FAIL 4
#define BAD_SENSE_DATA 5
#define INSUFFICIENT_BUFFER_SIZE 6


uint16_t getDataSize(uint8_t *readTocResponse);
uint16_t getEffectiveDataSize(uint16_t returnedTocDataLen, uint16_t actualSizeAllocated);
uint8_t getTracksCount(uint8_t firstTrackNum, uint8_t lastTrackNum);
void setTrackDescriptors(TrackDescriptor *trackDescriptors, void *source, uint8_t count);
void setTrackDescriptor(TrackDescriptor *dest, void *rawTrackDescriptor);
uint8_t getAdr(void *rawTrackDescriptor);
uint8_t getControl(void *rawTrackDescriptor);
uint8_t getTrackNum(void *rawTrackDescriptor);
uint32_t getStartAddr(void *rawTrackDescriptor);

struct TOC {
	TrackDescriptor *trackDescriptors;
	uint8_t trackDescriptorsSize;
	uint8_t firstTrackNum;
	uint8_t lastTrackNum; 
	uint8_t tracksCount;
};

struct TrackDescriptor {
	uint32_t startAddr;
	uint8_t adr;
	uint8_t control;
	uint8_t trackNum;
};
/*
int main() {
	TOC toc;
	if(readTOC(&toc)) {
		return 1;
	}

	for(int i=0; i< toc.trackDescriptorsSize/8; i++) {
		printf("%d %d %d %d\n", toc.trackDescriptors[i].trackNum, toc.trackDescriptors[i].startAddr, toc.trackDescriptors[i].control, toc.trackDescriptors[i].adr);
	}

	return 0;
}
*/

// Return value indicates either success of command or an indicator of faliliure.
// Return values are in readtoc.h
// On success, value of *trackCount is set to the number of tracks on the CD. 
int readTOC(TOC **dest) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return FAILED_OPEN_DEVICE; 
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

	unsigned int tocDataSize = getDataSize(dxferp);
	// the 2 (value of REPRESENTED_HEADER_SIZE) bytes in the response header that hold the value of the response size are not part of the total size info.
	// in other words, the value of the data size is 2 less than the size of the full response.
	// see MMC-3, Table 233 – READ TOC/PMA/ATIP response data (Format = 0000b), paragraph directly below table.
	if(tocDataSize <= REPRESENTED_HEADER_SIZE) // there were no track descriptors returned
		return NO_TOC_DATA_FOUND;
	if(tocDataSize + REPRESENTED_HEADER_SIZE > ALLOC_LEN)
		return INSUFFICIENT_BUFFER_SIZE;
	uint16_t trackDescriptorsLen = tocDataSize - REPRESENTED_HEADER_SIZE;
	TOC toc;
	TrackDescriptor *trackDescriptors = malloc((trackDescriptorsLen/TRACK_DESCRIPTOR_SIZE) * sizeof(TrackDescriptor));
	if(!trackDescriptors)
		return FAILED_ALLOCATE_MEMORY;
	toc.firstTrackNum = dxferp[FIRST_TRACK_NUM];
	toc.lastTrackNum = dxferp[LAST_TRACK_NUM];
	toc.trackDescriptorsSize = trackDescriptorsLen;
	toc.tracksCount = getTracksCount(toc.firstTrackNum, toc.lastTrackNum);

	setTrackDescriptors(trackDescriptors, dxferp+4, trackDescriptorsLen/TRACK_DESCRIPTOR_SIZE);
	
	toc.trackDescriptors = trackDescriptors;

	*dest = malloc(sizeof(TOC));
	**dest = toc;
	return SUCCESS;
}

void destroyTOC(TOC toc) {
	free(toc.trackDescriptors);
}

uint16_t getDataSize(uint8_t *readTocResponse) {
	uint8_t msbyte = *readTocResponse;
	uint8_t lsbyte = *(readTocResponse+1);
	uint16_t ret = msbyte;
	ret = ret << ONE_BYTE;
	ret |= lsbyte;
	return ret;
}

uint8_t getTracksCount(uint8_t firstTrackNum, uint8_t lastTrackNum) {
	return (lastTrackNum - firstTrackNum) + 1;
}

void setTrackDescriptors(TrackDescriptor *trackDescriptors, void *source, uint8_t count) {
	for(int i=0; i<count; i++, source+=TRACK_DESCRIPTOR_SIZE) {
		setTrackDescriptor(&(trackDescriptors[i]), source);
	}
}

void setTrackDescriptor(TrackDescriptor *dest, void *rawTrackDescriptor) {
	dest->adr = getAdr(rawTrackDescriptor);
	dest->control = getControl(rawTrackDescriptor);
	dest->trackNum = getTrackNum(rawTrackDescriptor);
	dest->startAddr = getStartAddr(rawTrackDescriptor);
}

uint8_t getAdr(void *rawTrackDescriptor) {
	uint8_t adrByte = ((uint8_t *)rawTrackDescriptor)[1];
	return adrByte >> 4;
}

uint8_t getControl(void *rawTrackDescriptor) {
	uint8_t controlByte = ((uint8_t *)rawTrackDescriptor)[1];
	return controlByte & CONTROL_MASK;
}

uint8_t getTrackNum(void *rawTrackDescriptor) {
	uint8_t trackNumByte = ((uint8_t *)rawTrackDescriptor)[2];
	return trackNumByte;
}

uint32_t getStartAddr(void *rawTrackDescriptor) {
	uint8_t *startAddrMSB = rawTrackDescriptor+4;
	uint32_t startAddr = 0;
	startAddr |= *startAddrMSB;
	startAddr = startAddr << ONE_BYTE;
	startAddr |= *(startAddrMSB+1);
	startAddr = startAddr << ONE_BYTE;
	startAddr |= *(startAddrMSB+2);
	startAddr = startAddr << ONE_BYTE;
	startAddr |= *(startAddrMSB+3);

	return startAddr;
}


TrackDescriptor *getTracks(TOC toc) {
	return toc.trackDescriptors;
}
uint8_t getTracksLen(TOC toc) {
	return toc.trackDescriptorsSize/TRACK_DESCRIPTOR_SIZE;
}
uint8_t getFirstTrackNumber(TOC toc) {
	return toc.firstTrackNum;
}
uint8_t getTrackCount(TOC toc) {
	return toc.tracksCount;
}
uint32_t getStartLBA(TrackDescriptor track) {
	return track.startAddr;
}
uint8_t getTrackNumber(TrackDescriptor track) {
	return track.trackNum;
}
