
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>

#include "readtoc.h"

#define CDB_SIZE 10
#define iOPCODE 0
#define OPCODE 0x43
#define iFORMAT 2
#define FORMAT 0x00
#define iALLOC_LEN_LSBYTE 8 // least significant byte of allocation length
#define ALLOC_LEN 0x04 // only enough space for the header, the data for the actual entries is not needed.

#define DEVICE_FILE "/dev/sg0"
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define MAX_SENSE_BUF_LEN 64

int getTrackCount(unsigned char *tocHeader);

int main() {
	int trackCount = -1;
	readTOC(&trackCount);
	printf("%d\n", trackCount);
}

// Return value indicates either success of command or an indicator of faliliure.
// Return values are in readtoc.h
// On success, value of *trackCount is set to the number of tracks on the CD. 
int readTOC(int *trackCount) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return -1;
	}

	unsigned char cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;

	cdb[iALLOC_LEN_LSBYTE] = ALLOC_LEN;
	
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
		return TOC_IOCTL_FAIL;
	}
	close(fd);

	if(hdr.sb_len_wr == 0) {
		*trackCount = getTrackCount(dxferp);
		return READ_TOC_SUCCESS; 
	}
	return NO_TOC;
}

int getTrackCount(unsigned char *tocHeader) {
	return tocHeader[3];
}
