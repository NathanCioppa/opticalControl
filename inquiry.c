
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>

#define CDB_SIZE 6
#define DEVICE_FILE "/dev/sg0"
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define MAX_SENSE_BUF_LEN 64
#define ALLOC_LEN 36
#define OP_CODE 0x12


int readBit(unsigned char byte, int bit) ;

int main() {
	int fd = open(DEVICE_FILE, O_RDONLY | O_NONBLOCK);
	if(fd < 0) {
		printf("failed tp open /dev/sg0\n");
		return 1;
	}

	// the command descriptor block for INQUIRY
	unsigned char cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	cdb[0] = OP_CODE;
	cdb[4] = ALLOC_LEN;

	unsigned char senseBuf[MAX_SENSE_BUF_LEN];
	memset(senseBuf, 0, MAX_SENSE_BUF_LEN);

	unsigned char dataBuf[ALLOC_LEN];
	memset(dataBuf, 0, ALLOC_LEN);
	dataBuf[0] = 1;

	sg_io_hdr_t hdr;
	memset(&hdr, 0, sizeof(sg_io_hdr_t));

	hdr.interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr.cmdp = cdb;
	hdr.cmd_len = CDB_SIZE;
	hdr.mx_sb_len = MAX_SENSE_BUF_LEN;
	hdr.sbp = senseBuf;
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.dxferp = dataBuf;
	hdr.dxfer_len = ALLOC_LEN;
	hdr.timeout = 5000;

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		printf("failed\n");
		return 2;
	}
	else {
		for(int i=0; i<ALLOC_LEN; i++) {
			printf("%d: ", i);
			for(int j=7; j>=0; j--)
				printf("%d", readBit(dataBuf[i], j));
			printf(" | 0x%x\n", dataBuf[i]);
		}
	}
	return 0;

}

// returns the bit (zero indexed) of byte.
int readBit(unsigned char byte, int bit) {
	return (byte >> bit) & 1;
} 
