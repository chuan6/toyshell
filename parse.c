#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

/**
 *	Parse the command, so that it can be executed correctly.
 **/

enum error { MALLOC = 1, OPEN, CLOSE };

static void *malloc_err(size_t nbytes);
static int open_err(const char *name, int flags, PipeElem *resource);
static void close_err(int filedes);

static PipeElem *makePipe();
static PipeElem *addToPipe(char *command, PipeElem *previousCommand);
static void freePipe(PipeElem *command);

static struct redir {
	char *sign;
	int flags;
	int dest;
} I = {"<", O_RDONLY, 0}, O = {">", O_WRONLY | O_CREAT, 1};
static struct redir io_pair[2];
static int redirgeti(char symbolchar);

int redirgeti(char s) {
	switch (s) {
	case '<':
		return 0;
	case '>':
		return 1;
	default:
		return -1;
	}
}

// parses a string of characters into a structured command
Command *ParseCommand(char *s) {
	if (!s) {
		printf("Error: NULL pointer passed to ParseCommand function.\n");
		return NULL;
	}
	if (*(s = Norm(s)) == '\0') {
		printf("Warning: empty string passed to ParseCommand function.\n");
		return NULL;
	}

	Command *command;

	char *initptr1;
	char *initptr = s;
	int io;
	// parse pipe
	PipeElem *pipe = makePipe();
	PipeElem *cmdp;
	for (cmdp = pipe; cmdp; s++) {
		switch (*s) {
		case '<': case '>':
			io = redirgeti(*s);
			*s = '\0';
			cmdp = addToPipe(initptr, cmdp);//printf("DEBUG\tParseCommand -> addToPipe %s\n", initptr);
			if (!cmdp || cmdp == pipe)
				goto ERROR;
			goto REDIR;
		case '\0':
			cmdp = addToPipe(initptr, cmdp);//printf("DEBUG\tParseCommand -> addToPipe %s\n", initptr);
			if (!cmdp)
				goto ERROR;
			goto PIPE;
		case '|':
			*s = '\0';
			cmdp = addToPipe(initptr, cmdp);//printf("DEBUG\tParseCommand -> addToPipe %s\n", initptr);
			initptr = s+1;
			break;
		default:
			break;
		}
	}
	// cmdp == NULL
	goto ERROR;

PIPE:	//printf("DEBUG\tParseCommand -> PIPE\n");
	command = (Command *)malloc_err(sizeof(Command));
	command->pipe = pipe;
	command->fin = 0;
	command->fout = 1;
	return command;

REDIR:	//printf("DEBUG\tParseCommand -> REDIR\n");
	if (*(s = Norm(++s)) == '\0')
		goto ERROR;

	io_pair[0] = I;
	io_pair[1] = O;

	initptr = strtok(s, io_pair[!io].sign);
	initptr1 = strtok(NULL, io_pair[!io].sign); if (initptr1 == NULL) printf("initptr1==NULL\n");
	initptr = Norm(initptr);		//printf("%s\t%s\n", io_pair[io].sign, initptr);
	initptr1 = Norm(initptr1);		//printf("%s\t%s\n", io_pair[!io].sign, initptr1);

	io_pair[io].dest = open_err(initptr, io_pair[io].flags, pipe);
	if (initptr1 != NULL && *initptr1 != '\0')
		io_pair[!io].dest = open_err(initptr1, io_pair[!io].flags, pipe);

	command = (Command *)malloc_err(sizeof(Command));
	command->pipe = pipe;
	command->fin = io_pair[0].dest;
	command->fout = io_pair[1].dest;
	return command;

ERROR:
	freePipe(pipe);
	return NULL;
}

Command *PrintCommand(Command *c) {printf("DEBUG\tPrintCommand()\n");
	if (!c)
		return NULL;
	PipeElem *p;
	char *s;
	int i;
	for (p = c->pipe->pipewith; p != NULL; p = p->pipewith) {
		s = *(p->argv);
		for (i = 0; (s = *(p->argv + i)) != NULL; i++) {
			printf("%s\t", s);
		}
		printf("\n");
	}
	return c;
}

void FreeCommand(Command *c) {printf("DEBUG\tFreeCommand()\n");
	if (!c)
		return;
	freePipe(c->pipe);printf("in: %d; out: %d\n", c->fin, c->fout);
	close_err(c->fin);
	close_err(c->fout);
}

// Note: pipe is made as a list of scmds
PipeElem *makePipe() {
	PipeElem *head = (PipeElem *)malloc_err(sizeof(PipeElem));
	head->name = NULL;
	head->argv = NULL;
	head->pipewith = NULL;
	return head;
}

// returns a pointer to the currently added PipeElem, or NULL if it fails
PipeElem *addToPipe(char *s, PipeElem *prev) {
	if (*(s = Norm(s)) == '\0')	// empty string
		return prev;

	PipeElem *cmd = (PipeElem *)malloc_err(sizeof(PipeElem));

	char *sepc = " ";
	cmd->name = strtok(s, sepc);
	
	int n;
	for (n = 1; strtok(NULL, sepc); n++);
	cmd->argv = (char **)malloc_err((n+1) * sizeof(char *));
	char *p;
	int i;
	for (i = 0, p = s; i < n; i++) {
		*(cmd->argv + i) = p;
		p += strlen(p) + 1;
	}
	*(cmd->argv + i) = NULL;

	cmd->pipewith = NULL;
	// PipeElem formed

	// add PipeElem to previous one
	prev->pipewith = cmd;

	return cmd;
}

void freePipe(PipeElem *c) {printf("DEBUG\tfreePipe()\n");
	PipeElem *p;
	for (p = c; p != NULL; p = p->pipewith) {
		//free(p->name);	//p->name == mem
		free(p->argv);
	}
}

void *malloc_err(size_t n) {
	void *p = malloc(n);
	if (p != NULL)
		return p;

	perror("malloc failed.\n");
	exit(MALLOC);
}

int open_err(const char *name, int flags, PipeElem *resource) {
	int fd;
	if ((fd = open(name, flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
		freePipe(resource);
		fprintf(stderr, "open %s failed.\n", name);
		exit(OPEN);
	}
	return fd;
}

void close_err(int fd) {
	if (fd == 0 || fd == 1 || fd == 2)
		return;
	if (close(fd) < 0) {
		printf("%d", fd);
		perror("close failed.\n");
		exit(CLOSE);
	}
}