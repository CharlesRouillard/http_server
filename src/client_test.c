#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

int main()
{
	int sock,r;
	char tampon[1000];
	struct sockaddr_in addr;

	sock = socket(AF_INET,SOCK_STREAM,0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	r = connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in));
	
	strcpy(tampon,"GET /test.html HTTP/1.1\n");
	strcat(tampon,"Host: 127.0.0.1\n");
	strcat(tampon,"\n"); 

	send(sock,tampon,strlen(tampon),0);

	r = recv(sock,tampon,sizeof(tampon),0);
	tampon[r] = '\0';
	printf("%s\n",tampon);

	r = recv(sock,tampon,sizeof(tampon),0);
	tampon[r] = '\0';
	printf("%s\n",tampon);
	
	close(sock);
	return 0;
}
