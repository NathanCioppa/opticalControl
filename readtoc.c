
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>

#define CDB_SIZE 10
#define iOPCODE 0
#define OPCODE 0x43
#define iFORMAT 1
#define FORMAT 0x02
#define iMAX_ALLOC_LEN1 7
#define iMAX_ALLOC_LEN2 8
#define MAX_ALLOC_LEN_HALF 0xff

#define DEVICE_FILE "/dev/sg0"
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define MAX_SENSE_BUF_LEN 64

int main() {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return 1;
	}

	unsigned char cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	///cdb[iFORMAT] = FORMAT;
	cdb[iMAX_ALLOC_LEN2] = 0x04;
	//cdb[iMAX_ALLOC_LEN2] = MAX_ALLOC_LEN_HALF;

	
	// Documentation for sg_io_hdr_t type (at least the best docs I could find):
	// https://sg.danny.cz/sg/p/scsi-generic_v3.txt
	sg_io_hdr_t hdr;
	memset(&hdr, 0, sizeof(sg_io_hdr_t));

	char dxferp[256];
	memset(dxferp, 0, 256);

	unsigned char senseBuf[MAX_SENSE_BUF_LEN];
	memset(&senseBuf, 0, MAX_SENSE_BUF_LEN);

	hdr.interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.dxferp = dxferp;
	hdr.dxfer_len = 256;
	hdr.cmd_len = CDB_SIZE;
	hdr.mx_sb_len = MAX_SENSE_BUF_LEN;
	hdr.cmdp = cdb;
	hdr.sbp = senseBuf;
	hdr.timeout = 5000;

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		close(fd);
		printf("Error in ioctl()\n");
		return 2;
	}

	if(hdr.sb_len_wr == 0) {
		printf("good\n");
		for(int i = 0; i<hdr.resid; i++) {
			putchar(dxferp[i]);
		}
		putchar('\n');
	}
	else {
		printf("sb len: %d\n", hdr.sb_len_wr);
		for(int i=0; i<hdr.sb_len_wr; i++) {
			printf("%x\n", hdr.sbp[i]);
		}
	}

	close(fd);
	return 0;
}

