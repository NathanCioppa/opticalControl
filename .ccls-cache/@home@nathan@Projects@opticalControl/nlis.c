#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>

int main() {

	int sockFd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if(sockFd < 0) {
		printf("failed to create socket.\n");
		return 1;
	}

	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	
	if(bind(sockFd, (struct sockaddr *) &addr, sizeof(struct sockaddr_nl)) == -1) {
		printf("bind failed.\n");
		return 2;
	}


	return 0;

}
