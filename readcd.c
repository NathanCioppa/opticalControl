
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h> 

#include "readcd.h"
#include "config.h"

#define CDB_SIZE 12
#define OPCODE 0xbe
#define SECTOR_TYPE 0b00000100
#define RET_TYPE_FIELDS 0b00010000 // returns userdata and a 4 byte header
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
#define BLOCK_SIZE CD_AUDIO_BLOCK_SIZE

#define BLOCKS_PER_BATCH 4
#define BATCH_SIZE (BLOCK_SIZE * BLOCKS_PER_BATCH) // must always be a multiple of BLOCK_SIZE

#define SUCCESS 0
#define FAILED_OPEN_DEVICE 1
#define FAILED_ALLOCATE_MEMORY 2
#define FAILED_IOCTL 3
#define BAD_SENSE_DATA 4
#define START_LBA_OUT_OF_RANGE 5
#define LEADOUT_REACHED READ_CD_AUDIO_LEADOUT_REACHED

void buildCDB(uint8_t cdb[CDB_SIZE]);
void setCDBStartLBA(uint8_t cdb[CDB_SIZE], uint32_t startLBA);
void setCDBTransferLen(uint8_t cdb[CDB_SIZE], uint32_t transferLen);
void buildSgIoHdr(sg_io_hdr_t *hdr, uint8_t cdb[CDB_SIZE], uint8_t *dataBuf, unsigned int dataBufSize, uint8_t senseBuf[MAX_SENSE]);
int getCDAudioBatch(unsigned long startLBA, unsigned long batchSize, int opticalDriveFD, void *dest);

static int opticalDriveFD = -1;

// transferLen is the number of logical blocks to read, each block being BLOCK_SIZE (2352) bytes
int readCDAudio(uint32_t startLBA, uint32_t leadoutLBA,uint32_t transferLen, void **dest, long *destSizeWritten) {
	bool leadoutReached = false;
	if(startLBA >= leadoutLBA)
		return START_LBA_OUT_OF_RANGE;
	if(startLBA+transferLen >= leadoutLBA) {
		transferLen = leadoutLBA - startLBA;
		leadoutReached = true;
	}
	// opticalDriveFD will be initialized to -1 and set to -1 on error since its value is only set by open()
	if(opticalDriveFD == -1 && (opticalDriveFD = open(OPTICAL_DRIVE_PATH, O_RDONLY)) == -1) 
		return FAILED_OPEN_DEVICE;

	const long dataSize = transferLen*BLOCK_SIZE;
	*dest = realloc(*dest, dataSize);
	if(!dest)
		return FAILED_ALLOCATE_MEMORY;

	// CD Audio is read in batches since ioctl will fail on large transfers. (ex. it fails to grab a full 2 seconds of audio data in one command, in my testing)
	// getCDAudioBatch() is called repeatedly to fill *dest with transferLen*BLOCK_SIZE bytes of audio data.

	long offset; // only increment offset in multiples of BATCH_SIZE
			      // this allows the offset of bytes to be converted to an offset of CD Audio blocks accurrately with division by BLOCK_SIZE
	int status; // return value for getCDAudioBatch to check for errors
	
	// loop while there is still space in *dest for another full batch.
	for(offset = 0; offset<(dataSize-BATCH_SIZE); offset+=BATCH_SIZE) {
		if((status = getCDAudioBatch(startLBA+(offset/BLOCK_SIZE), BLOCKS_PER_BATCH, opticalDriveFD, (*dest)+offset)))
			return status;
	}
	// when there is no longer space for a full batch, get a smaller one to fill the rest of *dest
	long blocksRemaining = (dataSize-offset)/BLOCK_SIZE;
	if(blocksRemaining > 0)
		status = getCDAudioBatch(startLBA+(offset/BLOCK_SIZE), blocksRemaining, opticalDriveFD, (*dest)+offset);
	
	if(status)
		return status;

	*destSizeWritten = dataSize;
	if(leadoutReached)
		return LEADOUT_REACHED;
	return SUCCESS;
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

int getCDAudioBatch(unsigned long startLBA, unsigned long batchSize /*should be a very small number*/, int opticalDriveFD, void *dest) {
	uint8_t cdb[CDB_SIZE];
	sg_io_hdr_t hdr;
	uint8_t senseBuf[MAX_SENSE];

	buildCDB(cdb);
	setCDBStartLBA(cdb, startLBA);
	setCDBTransferLen(cdb, batchSize);
	buildSgIoHdr(&hdr, cdb, dest, BLOCK_SIZE*batchSize, senseBuf);
	if(ioctl(opticalDriveFD, SG_IO, &hdr) == -1)
		return FAILED_IOCTL;
	if(hdr.sb_len_wr != 0)
		return BAD_SENSE_DATA; 
	return SUCCESS;
}

