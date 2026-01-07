#include <stdint.h>

typedef enum ReadAudioStatus {
	SUCCESS,
	FAILED_TO_OPEN_DEVICE,
	IOCTL_FAIL,
	BAD_SENSE_DATA,
} ReadAudioStatus;

ReadAudioStatus readCDAudio(uint32_t startLBA, uint32_t transferLen);
