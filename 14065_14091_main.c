#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include "14065_14091_vsfs.h"

#define buffer_size 1024
#define token_size 64
#define token_delim " \n\t\r\a"

int shelly_help (char **args);
int shelly_exit (char **args);
int shelly_hi (char **args);
int shelly_read(char **args);
int shelly_write(char **args);
int shelly_create(char **args);
int shelly_filelist(char **args);
int shelly_inode_bitmap(char **args);
int shelly_data_bitmap(char **args);

int pos;

char *builtin_str[] = {"create","write","read","exit","help","hi","filelist","inode_bitmap","data_bitmap"};

int (*builtin_func[]) (char**) = {&shelly_create,&shelly_write,&shelly_read,&shelly_exit,&shelly_help,&shelly_hi,&shelly_filelist,&shelly_inode_bitmap,&shelly_data_bitmap};

int num_builtins () {
	return sizeof(builtin_str) / sizeof(char*);
}

int shelly_create(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr,"shelly: excpected argument to \"create\"\n");
	}
	else {
		fileSystemName = malloc(sizeof(args[1]));
		strcpy(fileSystemName,args[1]);
		fileSystemDesc=createSFS(args[1],atoi(args[2]));
		printf("File System Descriptor: %d\n",fileSystemDesc);
	}
	return 1;
}

int shelly_write (char **args) {
	if (args[1] == NULL) {
		fprintf(stderr,"shelly: excpected argument to \"write\"\n");
	}
	else {
		int buf_size=0;
		for (int i=2;i<pos;i++) {
			buf_size += strlen(args[i]);
		}
		buf_size += (pos-1);
		char *filename = malloc(strlen(args[1]));
		char *buffer = malloc(buf_size);
		strcpy(filename,args[1]);
		for (int i=2;i<pos;i++) {
			strcat(buffer,args[i]);
			strcat(buffer," ");
		}
		int bytes = writeFile(fileSystemDesc,filename,buffer);
		printf("Inode Number: %d\n",bytes);
	}
	return 1;
}

int shelly_filelist (char **args) {
	print_FileList(fileSystemDesc);
	return 1;
}

int shelly_inode_bitmap (char **args) {
	print_inodeBitmaps(fileSystemDesc);
	return 1;
}

int shelly_data_bitmap (char **args) {
	print_dataBitmaps(fileSystemDesc);
	return 1;
}


int shelly_read (char **args) {
	if (args[1] == NULL) {
		fprintf(stderr,"shelly: excpected argument to \"read\"\n");
	}
	else {
		char *data;
		char *filename = malloc(strlen(args[1]));
		strcpy(filename,args[1]);
		int bytes = readFile(fileSystemDesc,filename, data);
		printf("Bytes Read: %d\n",bytes);
	}
}

int shelly_help (char **args) {
	printf("shelly by Nalin Gupta\n");
	printf("The following are the builtin functions:\n");
	for (int i=0;i<num_builtins();i++) {
		printf(" %s\n",builtin_str[i]);
	}
	return 1;
}

int shelly_exit (char **args) {
	return 0;
}

int shelly_hi (char **args) {
	char *arg [] = {"say","Hi, my name is shelly, I was developed by Nalin Gupta.",NULL};
	pid_t pid, wpid;
	int status;

	pid = fork();
	if (pid==0) {
		if (execvp(arg[0],arg)==-1) {
			perror("shelly");
			printf("\n");
		}
		return (EXIT_FAILURE);
	}
	else if (pid < 0 ) {
		perror("shelly");
		printf("\n");
	}
	else {
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	return 1;
}

char *get_username () {
	register struct passwd *pw;
	register uid_t uid;

	uid = geteuid ();
	pw = getpwuid (uid);

	if (pw)
	{
		return pw->pw_name;
	}

	fprintf (stderr,"shelly: cannot find username for UID %u\n",(unsigned) uid);
	exit (EXIT_FAILURE);
}

void signal_handler (int sig) {
	signal(SIGINT,signal_handler);
	printf("\nshelly: %s$ ", get_username());
	fflush(stdout);
}

char *shelly_read_line () {
	char *line = NULL;
	ssize_t buffer = 0;
	getline(&line, &buffer, stdin);
	return line;
}

char **shelly_split_line (char *line) {
	int buffer = token_size;
	pos =0;
	char **tokens = malloc(buffer * sizeof(char*));
	char *token;

	if (!tokens) {
		fprintf(stderr, "shelly: Allocation error\n");
		return (EXIT_FAILURE);
	} 

	token = strtok(line, token_delim);
	while (token != NULL) {
		tokens[pos] = token;
		pos++;

		if (pos >= token_size) {
			buffer += buffer;
			tokens = realloc(tokens,buffer*sizeof(char*));

			if (!tokens) {
				fprintf(stderr, "shelly: Allocation error\n");
				return (EXIT_FAILURE);
			} 
		}
		token = strtok(NULL, token_delim);
	}
	tokens[pos] = NULL;
	return tokens;
}

int shelly_execute (char **args) {
	int pipes=0;

	if (args[0] == NULL) {
		return 1;
	}

	for (int i=0;i<num_builtins();i++) {
		if (strcmp(args[0],builtin_str[i])==0){
			return (*builtin_func[i])(args);
		}
	}
}

void shelly_loop () {
	char *line;
	char **args;
	int status = 1;

	signal(SIGINT,signal_handler);
	printf("\n");
	do {
		printf("shelly: %s$ ", get_username());
		line = shelly_read_line();
		args = shelly_split_line(line);
		status = shelly_execute(args);

		free(line);
		free(args);
	} while(status);
}

int main(int argc, char *argv[]) {
	shelly_loop();
	return 0;
}