

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

// TODO organize macro definitions

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

#define SUCCESS 0
#define	FAILED_TO_ALLOCATE_MEMORY 1
#define	FAILED_TO_OPEN_DEVICE_FILE 2
#define	FAILED_IOCTL 3
#define	CDTEXT_DOES_NOT_EXIST 4
#define	CDTEXT_DATA_EMPTY 5
#define	BLOCKNUM_OUT_OF_RANGE 6
#define	BLOCKNUM_NOT_FOUND 7

typedef struct PackData PackData;
typedef struct Block Block;
typedef struct TrackNumRange TrackNumRange;

CDText makeCDText(PackData packs);
unsigned int getDataLen(uint8_t *readTextResponse);
void *getPackStart(void *readTextResponse);
static void buildCDB(uint8_t cdb[CDB_SIZE]);
static void buildSgIoHdr(sg_io_hdr_t *hdr, uint8_t *cdb, uint8_t dataBuf[ALLOC_LEN], uint8_t senseBuf[MAX_SENSE]);
uint16_t toUtf8(unsigned char c);
uint8_t getBlockNum(void *pack);
PackData makePackData(void *packDataStart, unsigned int packDataSize);
Track *getAlbum(PackData packs);
TrackNumRange getTrackNumRange(PackData packs);
char *makeTrackInfoPool(PackData packs, uint8_t trackCount, char **strings, uint8_t typeIndicator);
uint8_t getCharacterPositionIndicator(uint8_t *pack);
Track *getTracks(PackData packs, uint8_t trackCount);
char *makeAlbumInfo(PackData packs, uint8_t typeIndicator);
void destroyTracks(Track *tracks);


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
};

struct CDText {
	PackData packs;
	Block block;
};


/*
int main() {
	CDText text;
	if(readText(&text, 0))
		return 1;

	printf("Album: %s, %s\n", text.block.album->title, text.block.album->artist);
	for(int i=0; i<text.block.range.count; i++) {
		printf("Track %d: %s, %s\n", i+1, text.block.tracks[i].title, text.block.tracks[i].artist);
	}

	destroyCDText(text);

	return 0;
}
*/

// Changes the value at *dest to be a valid CDText struct.
// On failiure, *dest is unmodified
// returns an error code, 0 is success.
// most reliable value for defaultBlockNum is 0
int readText(CDText **dest, uint8_t defaultBlockNum) {
	int fd = open(DEVICE_FILE, O_RDONLY);
	if(fd == -1) {
		return FAILED_TO_OPEN_DEVICE_FILE;
	}

	uint8_t dataBuf[ALLOC_LEN]; 
	uint8_t senseBuf[MAX_SENSE];
	uint8_t cdb[CDB_SIZE];
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
		return FAILED_TO_ALLOCATE_MEMORY;
	memcpy(packsAlloc, packsStart, packDataSize);
	
	PackData packs = makePackData(packsAlloc, packDataSize);
	CDText text = makeCDText(packs);

	int status = setBlock(&text, defaultBlockNum);
	if(status == SUCCESS) {
		CDText *textp = malloc(sizeof(CDText));
		if(!textp) {
			free(packsAlloc);
			return FAILED_TO_ALLOCATE_MEMORY;
		}
		*textp = text;
		*dest = textp;
	}
	return status;
}

void printReadTextErr(int err) {
	switch(err) {
		case CDTEXT_DOES_NOT_EXIST:
			printf("There is no CD-Text on this disc.");
			break;
		case SUCCESS:
			printf("readText() was successful.");
			break;
		default:
			printf("readText() failed.");

	}
}

static void buildCDB(uint8_t cdb[CDB_SIZE]) {
	memset(cdb, 0, CDB_SIZE);
	cdb[iOPCODE] = OPCODE;
	cdb[iALLOC_LEN_MSBYTE] = ALLOC_MSBYTE;
	cdb[iALLOC_LEN_LSBYTE] = ALLOC_LSBYTE;
	cdb[iFORMAT] = FORMAT;
}

static void buildSgIoHdr(sg_io_hdr_t *hdr, uint8_t cdb[CDB_SIZE], uint8_t dataBuf[ALLOC_LEN], uint8_t senseBuf[MAX_SENSE]) {
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

void *getPackStart(void *readTextResponse) {
	return readTextResponse+READ_TOC_HDR_SIZE;
}

// *readTextResponse is the data recieved from READ TOC/PMA/ATIP with format 0101b (MMC-3 Manual)
// As described in MMC-3 Manual, the first two bytes of the response are the size of the CD-Text data. 
unsigned int getDataLen(uint8_t *readTextResponse) {
	unsigned int MSByte = (uint8_t)(readTextResponse[0]);
	unsigned int LSByte = (uint8_t)(readTextResponse[1]);
	return (MSByte << ONE_BYTE) | LSByte;
}

// typeIndicator should be one of the indicators defined in MMC-3 Manual table J.2
// the indicators that this code actually uses will be defined as macros
char *makeAlbumInfo(PackData packs, uint8_t typeIndicator) {
	size_t stringAllocSize = 32;
	size_t iString = 0;
	char *string = malloc(stringAllocSize);
	if(!string)
		return NULL;

	unsigned char *thisPack = packs.start;
	for(int i=0; i<packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		unsigned char id1 = thisPack[0];
		unsigned char id2 = thisPack[1];
		if(id1 != typeIndicator || id2 != ALBUM_INDICATOR) 
			continue; // this is not album artist info

		// iterate through just the portion of the pack that has the text data (everything else is metadata)
		for(int iData = TEXT_DATA_FIELD_START; iData < TEXT_DATA_FIELD_START+TEXT_DATA_FIELD_LEN; iData++, iString++) {
			// resize *albumName if needed, 2 since in it possible that a two-byte UTF character will be added
			if(iString >=stringAllocSize-2) {
				stringAllocSize *= 2;
				char *temp = realloc(string, stringAllocSize);
				if(!temp) {
					free(string);
					return NULL; 
				}
				string = temp;
			}

			unsigned char c = thisPack[iData];
			uint16_t utf8Char = toUtf8(c);
			string[iString] = (char)utf8Char;
			if(c >= 128)
				string[++iString] = (char)(utf8Char >> ONE_BYTE); 

			if(string[iString] == '\0') // can return early since the string would terminate here anyway
				return string;
		}
	}
	string[iString] = '\0'; // ensure is termainated as a string
	return string;
}

uint16_t toUtf8(unsigned char c) {
	if(c < 128)
		return (uint16_t)c;

	// two highest order bits of c become the lowest order bits of the UTF8 header byte
	uint8_t firstByteUtf8 = UTF8_2BYTE_HEADER | (c >> 6);
	// remaining 6 low order bits become low order bits of the UTF8 continuation byte
	uint8_t secondByteUtf8 = UTF8_CONINUATION_BYTE | (c & LOW_ORDER_6BITS);

	uint16_t twoByteUtf8 = secondByteUtf8;
	twoByteUtf8 = (twoByteUtf8 << ONE_BYTE) | firstByteUtf8;
	return twoByteUtf8;
}

uint8_t getBlockNum(void *pack) {
	pack += PACK_OFFSET_TO_BLOCKNUM_BYTE;
	return (*((uint8_t *)pack) & BLOCK_NUM_MASK) >> 4;
}

// packs argument should represent the full pack data (excluding headers) from the Read TOC MMC command
// the returned value is not valid for usage intil it has been passed to a successful call to setBlock()
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
// CDText *text should be non-null and point to CDText returned from makeCDText()
// blockNum should be in range [0,7],but out of that range will still behave the same as if Block "blockNum" does not exist
// Note that this function does not allocate any memory; the pointer inside of text->block that is set simply points into memory alredy allocated for text->packs
int setBlock(CDText *text, uint8_t blockNum) {
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
	Track *tracks = getTracks(block.packs, block.range.count);
	if(!album || !tracks)
		return FAILED_TO_ALLOCATE_MEMORY;
	
	block.album = album;
	block.tracks = tracks;

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
	char *title = makeAlbumInfo(packs, TITLE_INDICATOR);
	char *artist = makeAlbumInfo(packs, ARTIST_INDICATOR);
	Track *album = malloc(sizeof(Track));
	
	if(!album || !artist || !title) {
		free(title);
		free(artist);
		free(album);
		return NULL;
	}

	album->title = title;
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

	char *titles[trackCount];
	char *artists[trackCount];

	char *trackTitlesPool = makeTrackInfoPool(packs, trackCount, (char **)titles, TITLE_INDICATOR);
	char *trackArtistsPool = makeTrackInfoPool(packs, trackCount, (char **)artists, ARTIST_INDICATOR);
	
	if(!tracks || !trackTitlesPool || !trackArtistsPool) {
		free(tracks);
		free(trackTitlesPool);
		free(trackArtistsPool);
		return NULL;
	}

	for(uint8_t i=0; i<trackCount; i++) {
		tracks[i].title = titles[i];
		tracks[i].artist = artists[i];
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

// allocates a contiguous block of memory that contains all the strings of type defined by typeIndicator argument
// 	ex. if typeIndicator is TITLE_INDICATOR, the allocated block has all the packs's track titles
// returns a pointer to the block of memory
// **strings should be an array large enough to hold trackCount pointers, those pointers are the start of each string in the returned char*
// the returned char* should be the same as strings[0], assuming a non NULL response.
char *makeTrackInfoPool(PackData packs, uint8_t trackCount, char **strings ,uint8_t typeIndicator) {
	char *poolStrings;
	size_t maxPoolSize = INITAL_POOL_SIZE;
	if(trackCount == 0 || !(poolStrings = malloc(maxPoolSize)) )
		return NULL;

	size_t iPool = 0;
	uint8_t stringsFound = 0;
	// find the start of the track titles info
	uint8_t *thisPack = packs.start;
	bool hasPassedAlbum = false;
	bool isNewString = true;
	for(size_t i=0; i<packs.size; i+=PACK_LEN, thisPack+=PACK_LEN) {
		if(*thisPack != typeIndicator)
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
					char *temp = realloc(poolStrings, maxPoolSize);
					if(!temp) {
						free(poolStrings);
						return NULL;
					}
					poolStrings = temp;
				}
				if(isNewString) {
					strings[stringsFound] = poolStrings+iPool;
					isNewString = false;
				}
				
				uint16_t utf8Char = toUtf8(c);
				poolStrings[iPool++] = (char)utf8Char;


				if(c > 127)
					poolStrings[iPool++] = (char)(utf8Char >> ONE_BYTE);
				else if(c == '\0') {
					stringsFound++;
					isNewString = true;
				}
			}
		}
	}
	
	// if less name strings were found than expected, add empty strings to the pool unitl it has the expected ammount
	int stringsLeftToAdd = (int)trackCount - (int)stringsFound;
	if(stringsLeftToAdd < 0) {
		free(poolStrings);
		printf("trackCount was less than stringsFound in makeTrackNamesPool(), which should be impossible\n");
		return NULL;
	}
	// make sure theres enough space
	if(iPool+stringsLeftToAdd >= maxPoolSize) {
		maxPoolSize += stringsLeftToAdd+1;
		char *temp = realloc(poolStrings, maxPoolSize);
		if(!temp) {
			free(poolStrings);
			return NULL;
		}
		poolStrings = temp;

	}
	for(int i=0; i<stringsLeftToAdd; i++) {
		strings[stringsFound+i] = poolStrings+iPool;
		poolStrings[iPool++] = '\0';
	}
	return poolStrings;
}

// free all heap allocated memory being used by text. Using text after this call has undefined behavior.
// readText() is (or at least should be) implemented such that this funcion can properly free CDText returned from it.
void destroyCDText(CDText *text) {
	free(text->packs.start);
	destroyTracks(text->block.album);
	destroyTracks(text->block.tracks);
}

void destroyTracks(Track *tracks) {
	free(tracks->title);
	free(tracks->artist);
	free(tracks);
} 



char *getAlbumName(CDText *text) {
	return text->block.album->title;
}
char *getAlbumArtist(CDText *text) {
	return text->block.album->artist;
}
char *getTrackName(CDText *text, uint8_t trackNum) {
	TrackNumRange range = text->block.range;
	if(trackNum == 0 || trackNum < range.first || trackNum > range.last)
		return NULL;
	uint8_t index = trackNum - range.first;
	return text->block.tracks[index].title;
}
char *getTrackArtist(CDText *text, uint8_t trackNum) {
	TrackNumRange range = text->block.range;
	if(trackNum == 0 || trackNum < range.first || trackNum > range.last)
		return NULL;
	uint8_t index = trackNum - range.first;
	return text->block.tracks[index].artist;
}

