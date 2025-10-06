
// Establish communication with netlink.
// Ultimately used to determine when a disc is inserted/removed from the drive.
//

#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

bool isOpticalDriveMsg(char *msgBuf, ssize_t msgSize, char *targetSubstr);

#define MAX_NETLINK_MSG 2048

static char *targetDevname;
static ssize_t targetDevnameLen = -1;

int main() {
	int sockFd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if(sockFd < 0) {
		printf("failed to create socket.\n");
		return 1;
	}

	// man 7 netlink for info on this formatting.
	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = 1;
	
	if(bind(sockFd, (struct sockaddr *) &addr, sizeof(struct sockaddr_nl)) == -1) {
		printf("bind failed.\n");
		return 2;
	}

	// the property that is expected to exist in a netlink message relating to a disc drive event.
	char identifier[] = "DEVNAME=sr0";

	char msg[MAX_NETLINK_MSG];
	ssize_t msgSize = recv(sockFd, &msg, MAX_NETLINK_MSG, 0);
	if(msgSize == -1) {
		printf("recv failed.\n");
		return 3;
	}

	if(isOpticalDriveMsg((char *)msg, msgSize, identifier)) {
		printf("Message from optical drive recieved\n");
	}
	return 0;

}

bool isOpticalDriveMsg(char *msgBuf, ssize_t msgSize, char *identifierStr) {
	bool isMatching = true;
	char *idStart = identifierStr;
	char *firstInvalid = msgBuf+msgSize;
	while(msgBuf != firstInvalid) {
		if(isMatching && (isMatching = *msgBuf == *identifierStr)) {
			if(*msgBuf == '\0')
				return true;
			identifierStr++;
		}
		else if(*msgBuf == '\0') {
			isMatching = true;
			identifierStr = idStart;
		}
		msgBuf++;
	}
	return false;
}
