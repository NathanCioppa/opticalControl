#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>

#define DEVICE_FILE "/dev/sg0"
#define CDB_SIZE 10
#define iOPCODE 0
#define OPCODE 0x43 // read TOC op-code
#define iALLOC_LEN_LSBYTE 8
#define ALLOC_LEN 0xff
#define iFORMAT 2
#define FORMAT 5

void printPack(unsigned char *pack);
int readBit(unsigned char byte, int bit);

int main() {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open device %s\n", DEVICE_FILE);
		return 1;
	}

	unsigned char cdb[CDB_SIZE];
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	cdb[iALLOC_LEN_LSBYTE] = ALLOC_LEN;
	cdb[iFORMAT] = FORMAT;

	unsigned char dxferp[ALLOC_LEN]; 

	sg_io_hdr_t hdr;
	memset(&hdr, 0, sizeof(sg_io_hdr_t));

	unsigned char sb[255];

	hdr.interface_id = 'S';
	hdr.cmdp = cdb;
	hdr.cmd_len = CDB_SIZE;
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.dxferp = dxferp;
	hdr.dxfer_len = ALLOC_LEN;
	hdr.mx_sb_len = 255;
	hdr.sbp = sb;
	hdr.timeout = 20000;

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		printf("ioctl failed\n");
		return 2;
	}

	if(hdr.sb_len_wr != 0) {
		printf("sense buffer:\n");
		for(int i=0; i<hdr.sb_len_wr; i++) {
			printf("%d: ",i);
			for(int j=7; j>=0; j--)
				printf("%d",readBit(sb[i], j));
			printf("\n");
		}
		return 3;
	}
	unsigned char *pack = (unsigned char *)dxferp;
	for(int i=0; i<ALLOC_LEN; i+=18, pack+=18) {
		printPack(pack);
	}
	putchar('\n');


	return 0;
}

void printPack(unsigned char *pack) {
	printf("Type: %c\n", *pack++);
	printf("Track: %c\n", *pack++);
	printf("Counter: %c\n", *pack++);
	printf("BlockNum/CharIndic: %c\n", *pack++);
	printf("Payload: \n");
	for(int i=0; i<14; i++, pack++) {
		printf("%c",*pack);
	}
	putchar('\n');
	//printf("CRC: %c ", *pack++);
	//printf("%c\n", *pack);
}

int readBit(unsigned char byte, int bit) {
	return (byte >> bit) & 1;
}
