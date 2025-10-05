
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

int main() {
	int fd = open(DEVICE_FILE, O_RDONLY | O_NONBLOCK);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return 1;
	}

	// SCSI command descriptor block for TEST UNIT READY a 6 bytes, all zero bytes.
	unsigned char cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	
	// Documentation for sg_io_hdr_t type (at least the best docs I could find):
	// https://sg.danny.cz/sg/p/scsi-generic_v3.txt
	sg_io_hdr_t hdr;
	memset(&hdr, 0, sizeof(sg_io_hdr_t));

	unsigned char senseBuf[MAX_SENSE_BUF_LEN];
	memset(&senseBuf, 0, MAX_SENSE_BUF_LEN);

	hdr.interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr.dxfer_direction = SG_DXFER_NONE;
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
		printf("unit ready\n");
	}
	else {
		printf("unit not ready\n");
	}

	close(fd);
	return 0;
}
