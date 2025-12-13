

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
#include <wchar.h>
#include <locale.h>
#include <stdint.h>
#include <stdbool.h>
#include "cd.h"

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
#define PACK_TYPE_TOC_INFO 0x88
#define PACK_TYPE_BLOCK_SIZE_INFO 0x8f
#define SCSI_GENERIC_INTERFACE_ID 'S'
#define SG_IO_TIMEOUT 10000

#define PACK_LEN 18
#define TEXT_DATA_FIELD_LEN 12
#define TEXT_DATA_FIELD_START 4
#define TITLE_INDICATOR 0x80
#define ALBUM_INDICATOR 0x00
#define ARTIST_INDICATOR 0x81
#define BLOCK_NUM_MASK 0b01110000
#define PACK_OFFSET_TO_BLOCKNUM_BYTE 3
#define PACK_OFFSET_TO_CHARACTER_POSITON_INDICATOR_BYTE 3
#define CHARACTER_POSITION_INDICATOR_MASK 0b00001111
#define FIRST_TRACK_NUM 5
#define LAST_TRACK_NUM 6


#define UTF8_2BYTE_HEADER 0b11000000
#define UTF8_CONINUATION_BYTE 0b10000000
#define LOW_ORDER_6BITS 0b00111111

#define INITAL_POOL_SIZE 256


typedef struct PackData PackData;
typedef struct Block Block;
typedef struct TrackNumRange TrackNumRange;

typedef enum ReadTextStatus ReadTextStatus ;

void printPack(unsigned char *pack);
int readBit(char byte, int bit);
void printByte(char byte);
unsigned int getDataLen(unsigned char *readTextResponse);
unsigned char *getPackStart(unsigned char *readTextResponse);
void printTrackTitles(unsigned char *packs, unsigned int packsSize) ;
void buildCDB(unsigned char cdb[CDB_SIZE]);
void buildSgIoHdr(sg_io_hdr_t *hdr, unsigned char *cdb, unsigned char dataBuf[ALLOC_LEN], unsigned char senseBuf[MAX_SENSE]);
ReadTextStatus readText(CDText *dest);
char *getAlbumName(PackData packs);
char *getAlbumArtist(PackData packs);
uint16_t toUtf8(unsigned char c);
uint8_t getBlockNum(void *pack);
CDText makeCDText(PackData packs);
ReadTextStatus setBlock(CDText *text, uint8_t blockNum);
static PackData makePackData(void *packDataStart, unsigned int packDataSize);
Track *getAlbum(PackData packs);
TrackNumRange getTrackNumRange(PackData packs);
char *makeTrackNamesPool(PackData packs, uint8_t trackCount);
char *makeArtistNamesPool(PackData packs, uint8_t trackCount);
uint8_t getCharacterPositionIndicator(uint8_t *pack);

struct PackData {
	void *start;
	unsigned int size;
};

struct TrackNumRange {
	uint8_t count;
	uint8_t first;
	uint8_t last;
};

struct Block {
	PackData packs;
	Track *album;
	Track *tracks;
	TrackNumRange range;
	char *trackNamesPool;
	char *trackArtistsPool;
};

struct CDText {
	PackData packs;
	Block block;
};


// return values for functions that are publicly available from this file's header
enum ReadTextStatus {
	// general
	SUCCESS, // make sure this stays first (value 0)
	FAILED_TO_ALLOCATE_MEMORY,

	// readText()
	FAILED_TO_OPEN_DEVICE_FILE,
	FAILED_IOCTL,
	CDTEXT_DOES_NOT_EXIST,
	CDTEXT_DATA_EMPTY,

	// setBlock()
	BLOCKNUM_OUT_OF_RANGE,
	BLOCKNUM_NOT_FOUND,

};

int main() {
	CDText text;
	if(readText(&text))
		return 1;
	if(setBlock(&text, 0))
		return 2;
	printf("%d\n", text.packs.size);
	printf("%d\n", text.block.packs.size);
	//char *albumArtist = getAlbumArtist(text.block.packs);
	//char *albumName = getAlbumName(text.block.packs);
	printf("HERE\n");
	printf("%s\n", text.block.album->name);
	printf("%s\n", text.block.album->artist);

	TrackNumRange x = getTrackNumRange(text.block.packs);
	printf("Tracks: %d\n", x.count);

	char *z = makeTrackNamesPool(text.block.packs, x.count);
	int n=0;
	while(n<x.count) {
		if(*z == '\0') {
			putchar('|');
			n++;
		}
		else
			putchar(*z);
		z++;
	}
	//printTrackTitles(text.block.packs.start, text.block.packs.size);

	return 0;

}

// CDText **dest pointer will be replaced with a memory allocated pointer returned by makeCDText()
// On failiure, **dest is unmodified
// returns an error code, 0 is success.
ReadTextStatus readText(CDText *dest) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		return FAILED_TO_OPEN_DEVICE_FILE;
	}

	unsigned char dataBuf[ALLOC_LEN]; 
	unsigned char senseBuf[MAX_SENSE];
	unsigned char cdb[CDB_SIZE];
	buildCDB(cdb);

	sg_io_hdr_t hdr;
	buildSgIoHdr(&hdr, cdb, dataBuf, senseBuf);

	if(ioctl(fd, SG_IO, &hdr) == -1) {
		return FAILED_IOCTL;
	}

	if(hdr.sb_len_wr != 0) {
		return CDTEXT_DOES_NOT_EXIST;
	}

	unsigned int packsLen = getDataLen(dataBuf);
	if(packsLen <= 2)  {
		return CDTEXT_DATA_EMPTY;
	}
	packsLen -= 2; // There are 2 bytes in the header after the data length field that are not part of pack data.
	
	void *packsStart = getPackStart(dataBuf);
	
	int packDataSize = ALLOC_LEN - 4; // -4 to remove whole header
	if(packsLen < ALLOC_LEN)
		packDataSize = packsLen;

	void *packsAlloc = malloc(packDataSize);
	if(!packsAlloc)
		return 5;
	memcpy(packsAlloc, packsStart, packDataSize);
	PackData packs = makePackData(packsAlloc, packDataSize);

	CDText text = makeCDText(packs);

	*dest = text;
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

char *getAlbumName(PackData packs) {
	size_t maxNameLen = 32;
	size_t nameLen = 0;
	char *albumName = malloc(maxNameLen);
	if(!albumName)
		return NULL;

	unsigned char *thisPack = packs.start;
	for(int i=0; i<packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		unsigned char id1 = thisPack[0];
		unsigned char id2 = thisPack[1];
		if(id1 != TITLE_INDICATOR || id2 != ALBUM_INDICATOR) 
			continue; // this is not album name info

		// iterate through just the portion of the pack that has the text data (everything else is metadata)
		for(int iData = TEXT_DATA_FIELD_START; iData < TEXT_DATA_FIELD_START+TEXT_DATA_FIELD_LEN; iData++, nameLen++) {
			// resize *albumName if needed
			if(nameLen >= maxNameLen-1) {
				maxNameLen *= 2;
				char *temp = realloc(albumName, maxNameLen*sizeof(wchar_t));
				if(temp)
					albumName = temp;
				else {
					free(albumName);
					return NULL;
				};
			}

			albumName[nameLen] = thisPack[iData];
			if(albumName[nameLen] == '\0') // can return early since the string would terminate here anyway
				return albumName;
		}
	}
	albumName[nameLen] = '\0'; // ensure is termainated as a string
	return albumName;
}

char *getAlbumArtist(PackData packs) {
	size_t maxNameLen = 32;
	size_t nameLen = 0;
	char *albumName = malloc(maxNameLen);
	if(!albumName)
		return NULL;

	unsigned char *thisPack = packs.start;
	for(int i=0; i<packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		unsigned char id1 = thisPack[0];
		unsigned char id2 = thisPack[1];
		if(id1 != ARTIST_INDICATOR || id2 != ALBUM_INDICATOR) 
			continue; // this is not album artist info

		// iterate through just the portion of the pack that has the text data (everything else is metadata)
		for(int iData = TEXT_DATA_FIELD_START; iData < TEXT_DATA_FIELD_START+TEXT_DATA_FIELD_LEN; iData++, nameLen++) {
			// resize *albumName if needed, 2 since in it possible that a two-byte UTF character will be added
			if(nameLen >= maxNameLen-2) {
				maxNameLen *= 2;
				char *temp = realloc(albumName, maxNameLen);
				if(temp)
					albumName = temp;
				else {
					free(albumName);
					return NULL;
				};
			}

			unsigned char c = thisPack[iData];
			uint16_t utf8Char = toUtf8(c);
			albumName[nameLen] = (char)utf8Char;
			if(c >= 128)
				albumName[++nameLen] = (char)(utf8Char >> ONE_BYTE); 

			if(albumName[nameLen] == '\0') // can return early since the string would terminate here anyway
				return albumName;
		}
	}
	albumName[nameLen] = '\0'; // ensure is termainated as a string
	return albumName;
}

void printTrackTitles(unsigned char *packs, unsigned int packsSize) {
	for(int i=0; i<packsSize; i+=18, packs+=18) {
		if(packs[0] == 0x81/*PACK_TYPE_TRACK*/) {
			printf("%d ", packs[1]);
			for(unsigned char *payload = packs+4; payload < packs+16; payload++) {
				if(*payload == '\0')
					putchar('\n');
				else if (*payload == '\t')
					putchar('#');
				else
					putchar((char)*payload);
			}
		}
	}

}

uint16_t toUtf8(unsigned char c) {
	if(c < 128)
		return (uint16_t)c;

	// two highest order bits of c become the lowest order bits of the UTF8 header byte
	unsigned char firstByteUtf8 = UTF8_2BYTE_HEADER | (c >> 6);
	// remaining 6 low order bits become low order bits of the UTF8 continuation byte
	unsigned char secondByteUtf8 = UTF8_CONINUATION_BYTE | (c & LOW_ORDER_6BITS);

	uint16_t twoByteUtf8 = secondByteUtf8;
	twoByteUtf8 = (twoByteUtf8 << ONE_BYTE) | firstByteUtf8;
	return twoByteUtf8;
}

uint8_t getBlockNum(void *pack) {
	pack += PACK_OFFSET_TO_BLOCKNUM_BYTE;
	uint8_t blockNum = (*((uint8_t *)pack) & BLOCK_NUM_MASK) >> 4;
	return blockNum;
}

CDText makeCDText(PackData packs) {
	CDText text; 
	Block block;

	memset(&block, 0, sizeof(Block));

	text.packs = packs;
	text.block = block;
	return text;
}

// Modifies CDText *text->block->packs & packsSize to represent the Block blockNum. 
// If Block blockNum does not exist the CD Text, or is a status other than SUCCESS is returned, *text will not be modified in any way.
// CDText *text should be non-null and returned from makeCDText()
// blockNum should be in range [0,7],but out of that range will still behave the same as if Block "blockNum" does not exist
ReadTextStatus setBlock(CDText *text, uint8_t blockNum) {
	if(blockNum > 7)
		return BLOCKNUM_OUT_OF_RANGE;
	
	unsigned char *blockStart = NULL;
	uint16_t blockSize = 0;
	unsigned char *thisPack = text->packs.start;
	// Search for the start of the target block (the first pack with Block Number blockNum)
	// Increase blockLen such that it represents the size in bytes of the block
	// break once the target has been fully iterated through (Blocks in CD Text are contiguous)
	for(int i=0; i<text->packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		bool thisPackIsInTargetBlock = getBlockNum(thisPack) == blockNum;
		
		if(!blockStart && thisPackIsInTargetBlock)
			blockStart = thisPack;
		
		if(blockStart && thisPackIsInTargetBlock)
			blockSize += PACK_LEN;
		else if(blockStart && !thisPackIsInTargetBlock)
			break; 
	}

	if(!blockStart)
		return BLOCKNUM_NOT_FOUND;
	
	Block block;
	memset(&block, 0, sizeof(Block));
	block.packs.start = blockStart;
	block.packs.size = blockSize;

	block.range = getTrackNumRange(block.packs);
	
	Track *album = getAlbum(block.packs);
	if(!album)
		return FAILED_TO_ALLOCATE_MEMORY;
	
	block.album = album;


	// TODO add album and tracks members to block
	// 	free old  block on success
	
	text->block = block;
	return SUCCESS;
}

PackData makePackData(void *packsStart, unsigned int packsSize) {
	PackData packData;
	packData.start = packsStart;
	packData.size = packsSize;

	return packData;
}

Track *getAlbum(PackData packs) {
	char *name = getAlbumName(packs);
	char *artist = getAlbumArtist(packs);
	Track *album = malloc(sizeof(Track));
	
	if(!album || !artist || !name) {
		free(name);
		free(artist);
		free(album);
		return NULL;
	}

	album->name = name;
	album->artist = artist;
	return album;
}

// Returns an array of Track of size trackCount.
// Typically trackCount is determined by TrackNumRange.count returned from getTrackNumRange(packs), where packs is identical to packs as passed to this function.
// Returns NULL if memory fails to be allocated.
// The returned Track array may be NULL terminated earlier than trackCount if trackCount is not actually accurate to the number of tracks.
// 	For a well formatted CD Text, that should never happen. 
// If trackCount is correct, the array will not be NULL terminated as it's length will be correctly represented by trackCount.
Track *getTracks(PackData packs, uint8_t trackCount) {
	Track *tracks = calloc(trackCount, sizeof(Track));

	char *trackNamesPool = makeTrackNamesPool(packs, trackCount);

	if(!tracks || !trackNamesPool) {
		free(tracks);
		free(trackNamesPool);
		return NULL;
	}
	return tracks;
}

// Returns a TrackNumRange that is properly set to reflect the Block Size Info of packs.
// if Block Size Info cannot be found for packs, all members of returned TrackNumRange are set to 0.
TrackNumRange getTrackNumRange(PackData packs) {
	uint8_t *pack = packs.start;
	TrackNumRange range;
	for(int i=0; i<packs.size; i+=PACK_LEN, pack+=PACK_LEN) {
		if(*pack == PACK_TYPE_BLOCK_SIZE_INFO) {
			range.first = pack[FIRST_TRACK_NUM];
			range.last = pack[LAST_TRACK_NUM];
			int count = ((int)range.last) - ((int)range.first - 1);
			if(count <= 0)
				range.count = 0;
			else
				range.count = (uint8_t)count;
			
			return range;
		}
	}
	memset(&range, 0, sizeof(TrackNumRange));
	return range;
}

uint8_t getCharacterPositionIndicator(uint8_t *pack) {
	return pack[PACK_OFFSET_TO_CHARACTER_POSITON_INDICATOR_BYTE] & CHARACTER_POSITION_INDICATOR_MASK;
}

char *makeTrackNamesPool(PackData packs, uint8_t trackCount) {
	char *trackNames;
	int maxPoolSize = INITAL_POOL_SIZE;
	if(trackCount == 0 || !(trackNames = malloc(maxPoolSize)) )
		return NULL;

	int iPool = 0;
	uint8_t stringsFound = 0;
	// find the start of the track titles info
	uint8_t *thisPack = packs.start;
	bool hasPassedAlbum = false;
	for(int i=0; i<packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		if(*thisPack != TITLE_INDICATOR)
			continue;
		for(int j=0; j<TEXT_DATA_FIELD_LEN && stringsFound < trackCount; j++) {
			unsigned char c = thisPack[TEXT_DATA_FIELD_START + j];
			uint8_t id2 = thisPack[1];
			if(id2 == ALBUM_INDICATOR && !hasPassedAlbum) {
				hasPassedAlbum = c == '\0';
				continue;
			}
			else if (id2 != ALBUM_INDICATOR || hasPassedAlbum) {
				// add this character into the pool.
				// make space in the pool if needed.
				if(iPool >= maxPoolSize - 2) {
					maxPoolSize *= 2;
					char *temp = realloc(trackNames, maxPoolSize);
					if(!temp) {
						free(trackNames);
						return NULL;
					}
					trackNames = temp;
				}
				uint16_t utf8Char = toUtf8(c);
				trackNames[iPool++] = (char)utf8Char;
				if(c > 127)
					trackNames[iPool++] = (char)(utf8Char >> ONE_BYTE);
				else if(c == '\0')
					stringsFound++;
			}
		}
	}
	
	// if less name strings were found than expected, add empty strings to the pool unitl it has the expected ammount
	int stringsLeftToAdd = (int)trackCount - (int)stringsFound;
	if(stringsLeftToAdd < 0) {
		free(trackNames);
		printf("trackCount was less than stringsFound in makeTrackNamesPool(), which should be impossible\n");
		return NULL;
	}
	// make sure theres enough space
	if(iPool+stringsLeftToAdd >= maxPoolSize) {
		maxPoolSize += stringsLeftToAdd+1;
		char *temp = realloc(trackNames, maxPoolSize);
		if(!temp) {
			free(trackNames);
			return NULL;
		}
		trackNames = temp;
	}
	for(int i=0; i<stringsLeftToAdd; i++) {
		trackNames[iPool++] = '\0';
	}
	return trackNames;
}


char *makeArtistNamesPool(PackData packs, uint8_t trackCount) {
	return NULL;
}



