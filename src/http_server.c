#include "../include/http_server.h"

void *exec(void *arg)
{
	/*variables*/
	int sock,r,i;
	char buff[SIZE],*p,*line[2],temp[30],*elt[3],cwd[PATH_MAX+1];
	struct journal journal = (*(struct journal *)arg);
	struct pipeline *pipes;

	/*init variables*/
	free(arg);

	taille_pipeline = 0;
	pipe_tid = malloc(taille_pipeline * sizeof(pthread_t));

	sock = journal.sock;
	while((r = recv(sock,buff,sizeof(buff),0)) > 0)
	{
		journal.date = gettime(); /*date de récéption de la requète*/
		journal.tid = pthread_self();

		pipes = malloc(sizeof(struct pipeline));
		buff[r] = '\0';

		if(verbeux)
			printf("\nRequète reçu :\n%s\n",buff);

		i = 0;
		for(p = strtok(buff,"\r\n");p != NULL;p = strtok(NULL,"\r\n"))
		{
			line[i] = p;/*line[0] contient la 1ere ligne (GET /chemin HTTP), line[1] contient 
	(Host: ...)*/
			i++;
			if(i == 1)/*on ne prend que les 2 premières ligne de la requète du client*/
				break;
		}

		if(line[0] == NULL)
		{
			close(sock);
			pthread_mutex_lock(&mutex);
			cpt_max_cli--;
			pthread_mutex_unlock(&mutex);
			pthread_exit((void *)0);
		}

		strcpy(temp,line[0]);/*copie de line[0] via une variable temporaire car le strtok va modifier cett avaleur par la suite*/
		journal.first_line = temp;

		/*traitement des 2 premières lignes*/
		i = 0;
		for(p = strtok(line[0]," ");p != NULL;p = strtok(NULL," "))
		{
			elt[i] = p;/*elt[0] contient GET, elt[1] contient /chemin et elt[2] contient HTTP/1.1*/
			i++;
		}

		strcpy(cwd,".");
		strcat(cwd,elt[1]);
	
		pipes->journal = journal;
		pipes->req = elt[0];
		pipes->path = elt[1];
		pipes->version = elt[2];
		pipes->cwd = cwd;
		pipes->buff = buff;
		pipes->id = taille_pipeline; /*id de chaque thread pour gérer la fermeture*/

		taille_pipeline++;
		pipe_tid = realloc(pipe_tid,(taille_pipeline*sizeof(pthread_t)));
		pthread_create(&(pipe_tid[taille_pipeline-1]),NULL,func_pipeline,(void *)pipes);
	}
	pthread_mutex_lock(&mutex);
	cpt_max_cli--;
	pthread_mutex_unlock(&mutex);
	close(sock);
	if(verbeux)
		printf("connexion fermé\n");
	pthread_exit((void *)0);
}

void *func_pipeline(void *arg)
{
	int sock,r,nbBytesSend,fd, status,tempo,tube[2],tube2[2];
	char buff[SIZE],temp[sizeof(int)],*elt[3],cwd[PATH_MAX+1],*retmess,*retcode,temp_retcode[3],final[SIZE],*write_journal,mime[100];
	struct stat info;
	struct sigaction action;
	struct dirent *dir;
	struct pipeline pipes;
	sigset_t sig;
	DIR *dp;
	
	pipes = (*(struct pipeline *)arg);
	free(arg);

	sock = pipes.journal.sock;
	elt[0] = pipes.req;
	elt[1] = pipes.path;
	elt[2] = pipes.version;
	strcpy(cwd,pipes.cwd);
	strcpy(buff,pipes.buff);

	nbBytesSend = 0;
	flag_kill = 0;

	/*tester si c'est un requète GET ?*/
	if(strcmp(elt[0],"GET") == 0)
	{
		fd = stat(cwd,&info);/*fonction stat pour avoir la taille du fichier ou le type*/
		if(fd == -1)
		{
			perror("stat");
		}

		if(S_ISDIR(info.st_mode))
		{
			/*Le chemin indique un répèrtoire, on envoi la taille du fichier*/
			if(verbeux)
				printf("Le chemin indique un répèrtoire\n");
			retcode = "200";
			retmess = "OK";

			/*envoi de la première ligne*/
			strcpy(final,elt[2]); 
			strcat(final," ");
			strcat(final,retcode);
			strcat(final," ");
			strcat(final,retmess);
			strcat(final,"\n");
			nbBytesSend += send(sock,final,strlen(final),0);
			if(verbeux)
				printf("Envoi de : \n%s",final);

			/*envoi de la deuxième ligne*/
			strcpy(final,"Content-Type: text/plain\n");
			nbBytesSend += send(sock,final,strlen(final),0);
			if(verbeux)
				printf("%s",final);
			
			/*premier parcours du rep pour obtenir la taille en octet de tous les noms des fichiers qu'il contient*/
			tempo = 0;
			dp = opendir(cwd);
			if(dp != NULL)
			{
				while((dir = readdir(dp)))
				{
					tempo += strlen(dir->d_name); 
				}
			}
			closedir(dp);

			/*ajout de cette taille dans le Content-Length*/
			strcpy(final,"Content-Length: ");
			sprintf(temp,"%d",tempo);
			strcat(final,temp);
			strcat(final,"\n");
			nbBytesSend += send(sock,final,strlen(final),0);

			if(verbeux)
				printf("%s\n",final);

			/*envoi de la ligne vide*/
			nbBytesSend += send(sock,"\n",1,0);
	
			/*envoi du contenu du répèrtoire*/
			dp = opendir(cwd);
			if(dp != NULL)
			{
				while((dir = readdir(dp)))
				{
					nbBytesSend += send(sock,dir->d_name,strlen(dir->d_name),0);
					nbBytesSend += send(sock,"\n",1,0);
					if(verbeux)
						printf("%s\n",dir->d_name);
				}
			}
			closedir(dp);
		}
		else
		{
			/*le chemin n'indique pas un répèrtoire, on test si c'est un éxécutable*/
			if(!access(cwd,X_OK))
			{
				if(verbeux)
					printf("Le chemin indique un éxécutable\n");
				/*c'est un éxécutable*/
				pipe(tube);
				pid = fork();
				if(pid == 0)
				{
					/*fils de niveau 1 qui va faire le relais entre la thread principal et son fils qui va gérer l'éxécutable*/
					pipe(tube2);

					/*gestion des signaux pour l'alarme*/
					sigemptyset(&sig);
					action.sa_mask = sig;
					action.sa_flags = 0;
					action.sa_handler = func_alarm;
					sigaction(SIGALRM, &action, NULL);

					/*nouveau fork qui va s'occuper de l'éxécutable*/
					pid = fork();
					if(pid == 0)
					{
						close(tube[0]);
						close(tube[1]);
						close(tube2[0]);
						dup2(tube2[1],STDOUT_FILENO);
						dup2(tube2[1],STDERR_FILENO);
						close(tube2[1]);

						/*execl via le chemin*/
						execl(cwd,cwd,0,0);
						perror("execl");
						exit(-1);
					}
					else
					{
						close(tube[0]);
						close(tube2[1]);
						
						alarm(10);/*timer 10 secondes*/
						waitpid(pid,&status,0);
						printf("code retour fils = %d\n",status);
						/*si la réponse est 0, on envoi nous même le code 200*/
						if(status == 0 && flag_kill == 0)
						{
							retcode = "200";
							retmess = "OK";
	
							strcpy(final,elt[2]); 
							strcat(final," ");
							strcat(final,retcode);
							strcat(final," ");
							strcat(final,retmess);
							strcat(final,"\nContent-Type: text/plain\n");
							nbBytesSend += send(sock,final,strlen(final),0);

							if(verbeux)
								printf("\nEnvoi de :\n%s",final);

							tempo = 0;
							while((r = read(tube2[0],buff,sizeof(buff))) > 0)
							{
								tempo += r;
							}

							/*ajout de cette taille dans le Content-Length*/
							strcpy(final,"Content-Length: ");
							sprintf(temp,"%d",tempo);
							strcat(final,temp);
							strcat(final,"\n\n");
							nbBytesSend += send(sock,final,strlen(final),0);

							if(verbeux)
								printf("%s",final);

							nbBytesSend += send(sock,buff,tempo,0);
							if(verbeux)
								printf("%s",buff);

							write(tube[1],"200",sizeof("200"));
						}
						else
						{
							/*si le code de retour n'est pas 0, on envoi le code 500, et le père s'occupera du reste*/
							write(tube[1],"500",sizeof("500"));
						}
						write(tube[1],&nbBytesSend,sizeof(int));
						close(tube[1]);
						close(tube2[0]);
						exit(status);/*on exit avec le code status du fils, pour que le père sache*/
					}
				}
				else
				{
					/*retour a la thread principal, qui attend son fils avec les résultats*/
					close(tube[1]);
					waitpid(pid,&status,0);
					read(tube[0],temp_retcode,sizeof(temp_retcode));
					/*on test si le code de renvoi lu dans le pipe est 200 ou 500 et que le retour du fils etait bien 0*/
					if(status == 0 && (strcmp(temp_retcode,"200") == 0))
					{
						retcode = "200";
						/*si le code était 200, on lis la taille de la réponse transmise*/
						read(tube[0],&tempo,sizeof(int));

					}
					else
					{
						retcode = "500";
						/*si echec de l'éxécutable, c'est a nous d'envoyer une réponse*/
						retcode = "500";
						retmess = "Internal Server Error";

						/*envoi de la premiere ligne*/
						strcpy(final,elt[2]); 
						strcat(final," ");
						strcat(final,retcode);
						strcat(final," ");
						strcat(final,retmess);
						strcat(final,"\n");
						nbBytesSend += send(sock,final,strlen(final),0);
						if(verbeux)
							printf("\nErreur de l'éxécutable. Envoi de : %s\n",final);
					}
					close(tube[0]);
				}
			}
			else
			{
				/*ce n'est pas un éxécutable, envoi du fichier*/
				if(verbeux)
					printf("Le chemin indique un fichier régulier\n");
				fd = open(cwd,O_RDONLY);
				if(fd == -1)
				{
					switch(errno)
					{
						case ENOENT:
							retcode = "404";
							retmess = "Not Found";

							/*envoi de la premiere ligne uniquement car erreur 404*/
							strcpy(final,elt[2]); 
							strcat(final," ");
							strcat(final,retcode);
							strcat(final," ");
							strcat(final,retmess);
							strcat(final,"\n");
							nbBytesSend += send(sock,final,strlen(final),0);
							if(verbeux)
								printf("\nFichier non trouvé. Envoi de :\n%s",final);
							break;
						case EACCES:
							retcode = "403";
							retmess = "Forbidden";

							/*envoi de la premiere ligne uniquement car erreur 403*/
							strcpy(final,elt[2]); 
							strcat(final," ");
							strcat(final,retcode);
							strcat(final," ");
							strcat(final,retmess);
							strcat(final,"\n");
							nbBytesSend += send(sock,final,strlen(final),0);
							if(verbeux)
								printf("\nDroit insufissant. Envoi de :\n%s",final);
							break;
					}
				}
				else{
					retcode = "200";
					retmess = "OK";

					/*envoi de la premiere ligne*/
					strcpy(final,elt[2]); 
					strcat(final," ");
					strcat(final,retcode);
					strcat(final," ");
					strcat(final,retmess);
					strcat(final,"\n");
					nbBytesSend += send(sock,final,strlen(final),0);

					if(verbeux)
						printf("\nEnvoi de :\n%s",final);
	
					/*envoi de la 2eme ligne*/
					strcpy(final,"Content-Type: ");
					strcpy(mime,get_mimetype(cwd));
					strcat(final,mime);
					strcat(final,"\n");
					nbBytesSend += send(sock,final,strlen(final),0);

					if(verbeux)
						printf("%s",final);

					strcpy(final,"Content-Length: ");
					sprintf(temp,"%d",(int)(info.st_size));
					strcat(final,temp);
					strcat(final,"\n");
					nbBytesSend += send(sock,final,strlen(final),0);

					if(verbeux)
						printf("%s\n",final);

					/*envoi de la ligne vide*/
					nbBytesSend += send(sock,"\n",1,0);

					/*envoi du contenu du fichier*/		
					while((r = read(fd,buff,sizeof(buff))) > 0)
					{
						nbBytesSend += send(sock,buff,r,0);
						if(verbeux)
							printf("%s",buff);
					}
				}
			}
		}
	}
	
	/*écriture dans le fichier log pour la journalisation*/
	pipes.journal.retcode = retcode;
	pipes.journal.size_file = nbBytesSend;

	/*écriture dans le fichier de log avec le verrou*/
	pthread_mutex_lock(&mutex);
	fd = open("/tmp/http3605942.log",O_WRONLY|O_CREAT|O_APPEND,0666);
	write_journal = journal_to_string(pipes.journal);
	if(verbeux)
		printf("Ecriture dans le journal de %s\n",write_journal);
	write(fd,write_journal,strlen(write_journal));
	write(fd,"\n",1);
	pthread_mutex_unlock(&mutex);
	
	close(fd);
	if(pipes.id == 0){
		if(verbeux)
			printf("thread %lu d'id %d quitte\n",(long)pthread_self,pipes.id);
		/*printf("fermeture %d\n",sock);
		close(sock);*/
		pthread_exit((void *)0);
	}
	else{
		/*chaque thread attend la thread d'avant*/
		if(verbeux)
			printf("thread %lu d'id %d attend la thread d'id n°%d\n",(long)pthread_self,pipes.id,(pipes.id-1));
		pthread_join(pipe_tid[taille_pipeline-1],NULL);
		printf("attente terminé, leave\n");
		pthread_exit((void *)0);
	}
}

void func_alarm(int sig)
{
	if(verbeux)
		printf("signal reçu %d\n",sig);
	if(sig == SIGALRM){
		flag_kill = 1;
		kill(pid, SIGINT);		
	}
}

char *gettime()
{
	char *ret = malloc(20*sizeof(char));
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	sprintf(ret,"%d/%d/%d-%d:%d:%d",tm.tm_mday,tm.tm_mon+1,tm.tm_year+1900,tm.tm_hour, tm.tm_min, tm.tm_sec);
	return ret;
}

char *journal_to_string(struct journal journal)
{
	char *ret = malloc(100*sizeof(char));
	sprintf(ret,"%s %s %d %lu %s %s %d",journal.ip,journal.date,journal.pid,(long)journal.tid,journal.first_line,journal.retcode,journal.size_file);
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

	line = malloc(512 * sizeof(char));
		
	/*parcours fichier mime.types et récupération mime en fonction de l'extension*/
	fp = fopen("./etc/mime.types","r");
	while((r = getline(&line,&len,fp)) != -1)
	{
		j=0;
		for(p = strtok(line,"\t");p != NULL;p = strtok(NULL,"\t"))
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
					free(line);
					return mimetemp[0];
				}
			}
		}
	}
	fclose(fp);
	free(line);
	return "text/plain";/*valeur par défaut*/;
}

int main(int argc, char **argv)
{
	/*variables*/
	int port,sock,max_cli,r,comm;
	char verb; /*permet de choisir le mode verbeux, qui affiche plusieurs information a l'écran*/
	struct sockaddr_in addr,caller;
	struct journal *journal;
	socklen_t a;
	pthread_t th;

	if(argc != 4)
	{
		fprintf(stderr,"Usage: ./http_server port max_cli X\n");
		exit(1);
	}

	while(1)
	{
		printf("Activer le mode verbeux ? O/N\n");
		scanf(" %c",&verb);
		if(verb == 'O' || verb == 'o'){
			verbeux = 1;
			break;
		}
		else if(verb == 'N' || verb == 'n'){
			verbeux = 0;
			break;
		}
		else
			printf("Erreur, il faut saisir O ou N\n");
	}
	
	if(verbeux)
		printf("Mode verbeux activé !\n");

	/*allocation variables*/
	port = atoi(argv[1]); 
	max_cli = atoi(argv[2]);
	a = sizeof(caller);
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
				journal = malloc(sizeof(struct journal));
				comm = accept(sock,(struct sockaddr *)&caller,&a);
				if(verbeux)
					printf("\nconnexion accepté <%s ; %d>\n",inet_ntoa(caller.sin_addr),ntohs(caller.sin_port));
				pthread_mutex_lock(&mutex);
				if(cpt_max_cli < max_cli){
					if(comm >= 0)
					{
						/*remplissage de la structure journal pour la journalisation*/
						journal->sock = comm;
						journal->ip = inet_ntoa(caller.sin_addr);
						journal->pid = getpid();
						cpt_max_cli++;
						if(verbeux)
							printf("\nnb client connecté = %d\n",cpt_max_cli);
						/*lancement du client dans une thread*/
						pthread_create(&th,NULL,exec,(void *)journal);
					}
					else
					{
						perror("accept");
						exit(1);
					}
					pthread_mutex_unlock(&mutex);
				}
				else{
					if(verbeux)
						printf("\nnombre de client simultanés max atteint\n");
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
