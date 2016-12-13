#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>

#define SIZE 1024

struct journal{
	int sock;
	char *ip;
	pid_t pid;
	char *date;
	pthread_t tid;
	char *first_line;
	char *retcode;
	int size_file;
};

struct pipeline{
	int id;
	struct journal journal;
	char *req;
	char *path;
	char *version;
	char *cwd;
	char *buff;
};

int cpt_max_cli,verbeux,taille_pipeline;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pid_t pid;
pthread_t *pipe_tid;

void *exec(void *arg);
void *func_pipeline(void *arg);
void func_alarm(int);
char *gettime();
char *journal_to_string(struct journal);
char *get_mimetype(char *);



