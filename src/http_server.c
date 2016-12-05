#include "../include/http_server.h"

void *exec(void *arg)
{
	int sock;
	struct journal *journal = (struct journal *)arg;
	struct stat info;
	int r,i,fd;
	char buff[SIZE],*p,*line[2],temp[30],*elt[3],cwd[PATH_MAX+1],*retmess,retcode[3],final[SIZE],*write_journal;

	sock = journal->sock;

	r = recv(sock,buff,sizeof(buff),0); /*un seul recv ?*/

	journal->date = gettime(); /*date de récéption de la requète*/
	journal->tid = pthread_self();

	buff[r] = '\0';

	i = 0;
	for(p = strtok(buff,"\r\n");p != NULL;p = strtok(NULL,"\r\n"))
	{
		line[i] = p;/*line[0] contient la 1ere ligne (GET /chemin HTTP), line[1] contient 
(Host: ...)*/
		i++;
		if(i == 1)/*on ne prend que les 2 premières ligne de la requète du client*/
			break;
	}

	strcpy(temp,line[0]);/*copie de line[0] via une variable temporaire car le strtok va modifier cett avaleur par la suite*/
	journal->first_line = temp;

	/*traitement des 2 premières lignes*/
	i = 0;
	for(p = strtok(line[0]," ");p != NULL;p = strtok(NULL," "))
	{
		elt[i] = p;/*elt[0] contient GET, elt[1] contient /chemin et elt[2] contient HTTP/1.1*/
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

					/*envoi de la premiere ligne uniquement car erreur*/
					strcpy(final,elt[2]); 
					strcat(final," ");
					strcat(final,retcode);
					strcat(final," ");
					strcat(final,retmess);
					strcat(final,"\n");
					send(sock,final,strlen(final),0);

					break;
				case EACCES:
					strcpy(retcode,"403");
					retmess = "Forbidden";

					/*envoi de la premiere ligne uniquement car erreur*/
					strcpy(final,elt[2]); 
					strcat(final," ");
					strcat(final,retcode);
					strcat(final," ");
					strcat(final,retmess);
					strcat(final,"\n");
					send(sock,final,strlen(final),0);

					break;
			}
		}
		else{
			strcpy(retcode,"200");
			retmess = "OK";

			/*envoi de la premiere ligne*/
			strcpy(final,elt[2]); 
			strcat(final," ");
			strcat(final,retcode);
			strcat(final," ");
			strcat(final,retmess);
			strcat(final,"\n");
			send(sock,final,strlen(final),0);
		
			/*envoi de la 2eme ligne*/
			strcpy(final,"Content-Type: ");
			strcat(final,get_mimetype(cwd));
			strcat(final,"\n");
			send(sock,final,strlen(final),0);

			/*envoi de la ligne vide*/
			send(sock,"\n",1,0);

			/*envoi du contenu du fichier*/		
			while((r = read(fd,buff,sizeof(buff))) > 0)
			{
				send(sock,buff,r,0);
			}
		}
	}

	journal->retcode = retcode;
	stat(cwd,&info);/*fonction stat pour avoir la taille du fichier*/
	journal->size_file = (int)info.st_size;

	/*écriture dans le fichier de log avec le verrou*/
	pthread_mutex_lock(&mutex);
	fd = open("/tmp/http3605942.log",O_WRONLY|O_CREAT|O_APPEND,0666);
	write_journal = journal_to_string(journal);
	write(fd,write_journal,strlen(write_journal));
	write(fd,"\n",1);
	cpt_max_cli--;
	pthread_mutex_unlock(&mutex);
	
	close(fd);
	close(sock);

	pthread_exit((void *)0);
}

char *gettime()
{
	char *ret = malloc(20*sizeof(char));
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	sprintf(ret,"%d/%d/%d-%d:%d:%d",tm.tm_mday,tm.tm_mon+1,tm.tm_year+1900,tm.tm_hour, tm.tm_min, tm.tm_sec);
	return ret;
}

char *journal_to_string(struct journal *journal)
{
	char *ret = malloc(100*sizeof(char));
	sprintf(ret,"%s %s %d %u %s %s %d",journal->ip,journal->date,journal->pid,journal->tid,journal->first_line,journal->retcode,journal->size_file);
	return ret;
	
}

char *get_mimetype(char *name)
{
	FILE *fp;
	char *p,*line;
	size_t len = 0;
	int i,r,j;
	char *temp[4], *mimetemp[2];

	i = 0;
	for(p = strtok(name,".");p != NULL;p = strtok(NULL,"."))
	{
		temp[i] = p; /*temp[i-1] = extension recherché*/
		i++;
	}
		
	/*parcours fichier mime.types et récupération mime en fonction de l'extension*/
	fp = fopen("/etc/mime.types","r");
	while((r = getline(&line,&len,fp)) != -1)
	{
		if( (strstr(line,"#") == NULL) && line[0] != '\n')
		{
			j=0;
			for(p=strtok(line,"\t"); p!=NULL; p=strtok(NULL,"\t"))
			{
				mimetemp[j] = p;
				j++;		
			}
			if(j == 2)
			{
				for(p = strtok(mimetemp[1]," "); p != NULL; p = strtok(NULL," "))
				{
					if(strcmp(p,temp[i-1]) == 0)
					{
						fclose(fp);
						return mimetemp[0];
					}
				}
			}
		}
	}
	fclose(fp);
	return "text/plain";/*valeur par défaut*/;
}

int main(int argc, char **argv)
{
	/*variables*/
	int port,sock,max_cli,r,comm;
	struct sockaddr_in addr,caller;
	struct journal *journal;
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
	cpt_max_cli = 0;
	journal = malloc(sizeof(struct journal));

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
				comm = accept(sock,(struct sockaddr *)&caller,&a);
				if(cpt_max_cli < max_cli){
					if(comm >= 0)
					{
						/*remplissage de la structure journal pour la journalisation*/
						journal->sock = comm;
						journal->ip = inet_ntoa(caller.sin_addr);
						journal->pid = getpid();
						pthread_mutex_lock(&mutex);
						cpt_max_cli++;
						pthread_mutex_unlock(&mutex);
						/*lancement du client dans une thread*/
						pthread_create(&th,NULL,exec,(void *)journal);
					}
					else
					{
						perror("accept");
						exit(1);
					}
				}
				else{
					printf("nombre de client simultanés max atteint\n");
					close(comm);
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
