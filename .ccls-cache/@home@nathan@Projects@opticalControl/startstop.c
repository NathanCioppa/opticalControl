
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>

#define CDB_SIZE 6

int main(int argc, char **argv) {

	if(argc != 2) {
		printf("Expects exactly one arg, but got %d args\n", argc - 1);
		return 3;
	}
	if(argv[1][0] != 'l' && argv[1][0] != 'e') {
		printf("arg must be either 'l' to load or 'e' to eject\n");
		return 4;
	}

	int fd = open("/dev/sg0", O_RDONLY | O_NONBLOCK);
	if(fd == -1) {
		printf("failed to open file /dev/sg0\n");
		return 1;
	}

	unsigned char startBit = argv[1][0] == 'l';

	unsigned char cdb[CDB_SIZE] = {0x1b, 0x00, 0x00, 0x00, 0x02 | startBit, 0x00};
	unsigned char sbp[256];
	memset(&sbp, 0, 256);

	sg_io_hdr_t io_hdr;
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = CDB_SIZE;
	io_hdr.cmdp = cdb;
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.sbp = sbp;
	io_hdr.mx_sb_len = 255;
	io_hdr.timeout = 5000;


	int ret = ioctl(fd, SG_IO, &io_hdr);
	if(ret == -1) {
		printf("ioctl gave %d\n", ret);
		close(fd);
		return 2;
	}
	
	printf("success\n");
	printf("MSG: %s\n",sbp);
	close(fd);
	return 0;


}
