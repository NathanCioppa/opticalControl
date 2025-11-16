
/* Issues the SCSI Multi Media Command to read the CD-Text of an audio disc.
 *
 * Sources are as follows and will be refered to by aliases when referenced:
 *
 * SCSI Manual:
 * 	Seagate 2016, SCSI Commands Reference Manual
 * 	https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf
 * Describes general SCSI commands, and defines terms used in other SCSI Docs/Drafts that I am sourcing.
 *
 * SCSI-3 MMC Manual:
 * 	ANSI X3.304-1997
 * 	https://www.13thmonkey.org/documentation/SCSI/x3_304_1997.pdf
 * Describes many of the SCSI Multi Media commands and is an offical release, but outdated.
 *
 * MMC-3 Manual:
 * 	T10/1363-D, revision 10g, November 12, 2001, WORKING DRAFT
 *	https://www.13thmonkey.org/documentation/SCSI/mmc3r10g.pdf
 * Describes many SCSI Muiti Media commands and is recent enough to contain documentation for CD-Text.
 * Used because SCSI-3 MMC Manual did not contain any mention of CD-Text
 * This is not an offical release, it is a working draft.
 * Despite this, it was accurate enough write this code.
 *
 * GNU:
 * 	GNU libcdio CD Text Format
 * 	https://www.gnu.org/software/libcdio/cd-text-format.html#References
 * Provides a good summary of the CD-Text format.
 * Cites MMC-3 Manual as a reference
 *
 * The offical, up to date SCSI standards/documentation of those standards is locked behind paywalls.
 * These are the sources that I was able to find for free.
*/

#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <string.h>
#include <stdlib.h>

// Command Descriptor Block components for READ TOC/PMA/ATIP 
// Documentation in MMC-3 Manual 6.25 READ TOC/PMA/ATIP Command
#define CDB_SIZE 10
#define OPCODE 0x43
#define FORMAT 0x05 // 0101b
#define ALLOC_LEN 4612
#define ALLOC_MSBYTE 0x12
#define ALLOC_LSBYTE 0x04

#define iOPCODE 0
#define iFORMAT 2
#define iALLOC_LEN_MSBYTE 7
#define iALLOC_LEN_LSBYTE 8


#define DEVICE_FILE "/dev/sg0"
#define MAX_SENSE 0xff
#define ONE_BYTE 8
#define READ_TOC_HDR_SIZE 4
#define MAX_CD_TRACK_COUNT 99
#define PACK_TYPE_TRACK 0x80
#define PACK_TRACK_NUM_ALBUM 0x00
#define PACK_LEN 4
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define SG_IO_TIMEOUT 10000


void printPack(unsigned char *pack);
int readBit(char byte, int bit);
void printByte(char byte);
unsigned int getDataLen(unsigned char *readTextResponse);
unsigned char *getPackStart(unsigned char *readTextResponse);
void printTrackTitles(unsigned char *packs, unsigned int packsSize) ;
void buildCDB(unsigned char cdb[CDB_SIZE]);
void buildSgIoHdr(sg_io_hdr_t *hdr, unsigned char *cdb, unsigned char dataBuf[ALLOC_LEN], unsigned char senseBuf[MAX_SENSE]);
int readText(int *textLenDest, unsigned char **textDest);

//int main() {
//	unsigned char *dest = NULL;
//	int len = -1;
//	readText(&len, &dest);
//	printf("%d\n", len);
//	printTrackTitles(dest, len);
//	return 0;
//
//}

// *textLenDest will be populated with the size of the pointer to full CD-Text 
// **textDest should be the address of a pointer. 
// 	That pointer will be replaced with a memory allocated pointer to the full CD-Text
// If the function fails, neither parameters will be modified. 
int readText(int *textLenDest, unsigned char **textDest) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		printf("failed to open device %s\n", DEVICE_FILE);
		return 1;
	}

	unsigned char dataBuf[ALLOC_LEN]; 
	unsigned char senseBuf[MAX_SENSE];
	unsigned char cdb[CDB_SIZE];
	buildCDB(cdb);

	sg_io_hdr_t hdr;
	buildSgIoHdr(&hdr, cdb, dataBuf, senseBuf);

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		printf("ioctl failed\n");
		return 2;
	}

	if(hdr.sb_len_wr != 0) {
		printf("There is no CD-Text on this disc.\n");
		return 3;
	}

	unsigned int packsLen = getDataLen(dataBuf);
	if(packsLen <= 2)  {
		printf("there was no pack data returned\n");
		return 4;
	}
	packsLen -= 2; // There are 2 bytes in the header after the data length field that are not part of pack data.
	
	unsigned char *packs = getPackStart(dataBuf);
	
	int actualLen = ALLOC_LEN - 4; // -4 to remove whole header
	if(packsLen < ALLOC_LEN)
		actualLen = packsLen;

	void *text = malloc(actualLen);
	if(!text)
		return 11;
	memcpy(text, packs, actualLen);
	
	*textLenDest = actualLen;
	*textDest = text;

	return 0;
}

void buildCDB(unsigned char cdb[CDB_SIZE]) {
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	cdb[iALLOC_LEN_MSBYTE] = ALLOC_MSBYTE;
	cdb[iALLOC_LEN_LSBYTE] = ALLOC_LSBYTE;
	cdb[iFORMAT] = FORMAT;
}

void buildSgIoHdr(sg_io_hdr_t *hdr, unsigned char *cdb,unsigned char dataBuf[ALLOC_LEN], unsigned char senseBuf[MAX_SENSE]) {
	memset(hdr, 0, sizeof(sg_io_hdr_t));
	
	hdr->interface_id = SCSI_GENERIC_INTERFACE_ID;
	hdr->cmdp = cdb;
	hdr->cmd_len = CDB_SIZE;
	hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	hdr->dxferp = dataBuf;
	hdr->dxfer_len = ALLOC_LEN;
	hdr->mx_sb_len = MAX_SENSE;
	hdr->sbp = senseBuf;
	hdr->timeout = SG_IO_TIMEOUT;
}

int readBit(char byte, int bit) {
	return (byte >> bit) & 1;
}

unsigned char *getPackStart(unsigned char *readTextResponse) {
	return readTextResponse + READ_TOC_HDR_SIZE;
}

// *readTextResponse is the data recieved from READ TOC/PMA/ATIP with format 0101b (MMC-3 Manual)
// As described in MMC-3 Manual, the first two bytes of the response are the size of the CD-Text data. 
unsigned int getDataLen(unsigned char *readTextResponse) {
	unsigned int MSByte = (unsigned int)(readTextResponse[0]);
	unsigned int LSByte = (unsigned int)(readTextResponse[1]);
	return (MSByte << ONE_BYTE) | LSByte;
}

void printTrackTitles(unsigned char *packs, unsigned int packsSize) {
	for(int i=0; i<packsSize; i+=18, packs+=18) {
		if(packs[0] == PACK_TYPE_TRACK) {
			for(unsigned char *payload = packs+4; payload < packs+16; payload++) {
				if(*payload == '\0')
					putchar('\n');
				else
					putchar((char)*payload);
			}
		}
	}

}
