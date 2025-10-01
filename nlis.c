
// Establish communication with netlink.
// Ultimately used to determine when a disc is inserted/removed from the drive.
//

#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#include <unistd.h>

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

	char buf[256];
	ssize_t msgSize = recv(sockFd, &buf, 256, 0);
	if(msgSize == -1) {
		printf("recv failed.\n");
		return 3;
	}
	printf("passed\n");
	return 0;

}
