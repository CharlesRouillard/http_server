#include "../include/http_server.h"

void *exec(void *arg)
{
	int sock = *((int *)arg);
	int r,i,fd;
	char buff[SIZE],*p,*line[8],*elt[3],cwd[PATH_MAX+1],*retmess,retcode[3],final[SIZE];

	r = recv(sock,buff,sizeof(buff),0); /*un seul recv ?*/
	buff[r] = '\0';	
	i = 0;
	for(p = strtok(buff,"\r\n");p != NULL;p = strtok(NULL,"\r\n"))
	{
		line[i] = p;/*line[0] contient la 1ere ligne (GET /chemin HTTP), line[1] contient 
(Host: ...)*/
		i++;
	}

	/*traitement des 2 premières lignes*/
	i = 0;
	for(p = strtok(line[0]," ");p != NULL;p = strtok(NULL," "))
	{
		elt[i] = p;/*elt[0] contient GET, elt[1] contient /chemin et elt[2] contient HTTP1.1*/
		i++;
	}
	
	strcpy(cwd,".");
	strcat(cwd,elt[1]);

	/*tester si c'est un requète GET ?*/
	if(strcmp(elt[0],"GET") == 0)
	{
		/*envoi du fichier*/
		fd = open(cwd,O_RDONLY);
		if(fd == -1)
		{
			switch(errno)
			{
				case ENOENT:
					strcpy(retcode,"404");
					retmess = "Not Found";
					break;
				case EACCES:
					strcpy(retcode,"403");
					retmess = "Forbidden";
					break;
			}
		}
		else{
			strcpy(retcode,"200");
			retmess = "OK";
		}
	
		/*envoi de la premiere ligne*/
		strcpy(final,elt[2]); 
		strcat(final," ");
		strcat(final,retcode);
		strcat(final," ");
		strcat(final,retmess);
		strcat(final,"\n");
		send(sock,final,strlen(final),0);
		
		/*envoi de la 2eme ligne*/
		strcpy(final,"Content-Type: text/plain\n");
		send(sock,final,strlen(final),0);

		/*envoi de la ligne vide*/
		send(sock,"\n",1,0);

		/*envoi du contenu du fichier*/		
		while((r = read(fd,buff,sizeof(buff))) > 0)
		{
			send(sock,buff,r,0);
		}
	}		
	close(sock);

	pthread_mutex_lock(&mutex);
	cpt_max_cli--;
	pthread_mutex_unlock(&mutex);

	pthread_exit((void *)0);
}

int main(int argc, char **argv)
{
	/*variables*/
	int port,sock,max_cli,r;
	int *comm;
	struct sockaddr_in addr,caller;
	socklen_t a;
	pthread_t th;

	if(argc != 4)
	{
		fprintf(stderr,"Usage: ./http_server port max_cli X\n");
		exit(1);
	}

	/*allocation variables*/
	port = atoi(argv[1]);
	max_cli = atoi(argv[2]);
	a = sizeof(caller);
	comm = (int *)malloc(sizeof(int));
	cpt_max_cli = 0;

	sock = socket(AF_INET,SOCK_STREAM,0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*corp du serveur*/
	r = bind(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in));
	if(r == 0)
	{
		r = listen(sock,0);
		if(r == 0)
		{
			while(1)
			{
				*comm = accept(sock,(struct sockaddr *)&caller,&a);
				if(cpt_max_cli < max_cli){
					if(*comm >= 0)
					{
						pthread_mutex_lock(&mutex);
						cpt_max_cli++;
						pthread_mutex_unlock(&mutex);
						/*lancement du client dans une thread*/
						pthread_create(&th,NULL,exec,comm);
					}
					else
					{
						perror("accept");
						exit(1);
					}
				}
				else{
					printf("nombre de client simultanés max atteint\n");
					close(*comm);
				}
			}
		}
		else
		{
			perror("listen");
			exit(1);
		}	
	}
	else
	{
		perror("bind");
		exit(1);
	}

	return EXIT_SUCCESS;
}
