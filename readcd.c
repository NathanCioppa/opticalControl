
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>

#define CDB_SIZE 12
#define OPCODE 0xbe
#define SECTOR_TYPE 0b00100
#define RET_TYPE_FIELDS 0b00110000 // returns userdata and a 4 byte header
#define iOPCODE 0
#define iSECTOR_TYPE 1
#define iRET_TYPE 9
#define THREE_BYTE_LIMIT 0xffffff
#define ONE_BYTE 8
#define iSTART_LBA_LSB 5
#define iTRANSFER_LEN_LSB 8
#define MAX_SENSE 0xff
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define SG_IO_TIMEOUT 5000

void buildCDB(uint8_t cdb[CDB_SIZE]);
void setCDBStartLBA(uint8_t cdb[CDB_SIZE], uint32_t startLBA);
void setCDBTransferLen(uint8_t cdb[CDB_SIZE], uint32_t transferLen);
void buildSgIoHdr(sg_io_hdr_t *hdr, uint8_t cdb[CDB_SIZE], uint8_t *dataBuf, unsigned int dataBufSize, uint8_t senseBuf[MAX_SENSE]);

void readCDAudio(uint32_t startLBA, uint32_t transferLen);

static uint8_t cdb[CDB_SIZE];

int main() {
	int fd = open("/dev/sg0", O_RDONLY);
	if(fd == -1)
		return 1;
	return 0;
}

void readCDAudio(uint32_t startLBA, uint32_t transferLen) {
	if(!*cdb) { // if the opcode is unset, then the CDB must never have been initialized
		buildCDB(cdb);
	}
	setCDBStartLBA(cdb, startLBA);
	setCDBTransferLen(cdb, transferLen);
}

void buildCDB(uint8_t cdb[CDB_SIZE]) {
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	cdb[iSECTOR_TYPE] = SECTOR_TYPE;
	cdb[iRET_TYPE] = RET_TYPE_FIELDS;
}

void setCDBStartLBA(uint8_t cdb[CDB_SIZE], uint32_t startLBA) {
	cdb[iSTART_LBA_LSB] = startLBA;
	cdb[iSTART_LBA_LSB-1] = startLBA >> ONE_BYTE;
	cdb[iSTART_LBA_LSB-2] = startLBA >> ONE_BYTE*2;
	cdb[iSTART_LBA_LSB-3] = startLBA >> ONE_BYTE*3;
}

// Transfer Length is 3 bytes, so if transferLen passed is larger than 3 bytes it is set to the max 3 byte value
void setCDBTransferLen(uint8_t cdb[CDB_SIZE], uint32_t transferLen) {
	if(transferLen > THREE_BYTE_LIMIT)
		transferLen = THREE_BYTE_LIMIT;
	cdb[iTRANSFER_LEN_LSB] = transferLen;
	cdb[iTRANSFER_LEN_LSB-1] = transferLen >> ONE_BYTE;
	cdb[iTRANSFER_LEN_LSB-2] = transferLen >> ONE_BYTE*2;
}

void buildSgIoHdr(sg_io_hdr_t *hdr, uint8_t cdb[CDB_SIZE], uint8_t *dataBuf, unsigned int dataBufSize, uint8_t senseBuf[MAX_SENSE]) {
	memset(hdr, 0, sizeof(sg_io_hdr_t));
	
	hdr->interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr->cmdp = cdb;
	hdr->cmd_len = CDB_SIZE;
	hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	hdr->dxferp = dataBuf;
	hdr->dxfer_len = dataBufSize;
	hdr->mx_sb_len = MAX_SENSE;
	hdr->sbp = senseBuf;
	hdr->timeout = SG_IO_TIMEOUT;
}
