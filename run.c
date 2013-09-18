#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell.h"

static char **pathenv;
static Alias *env;
static int withAlarm;
static pid_t groupid;

enum pio { RD = 0, WR = 1, STDIN = 0, STDOUT = 1 };

static void close_err(int fd);
static void dup_err(int fd);
static void pipe_err(int *pipeiofd);
static int redir_io(int fd_in, int fd_out);
static void runcmd(PipeElem *pipe);
static void execute(PipeElem *env);
static char *unalias(char *cmdname);

static pid_t test;

void SetEnv(char **p, Alias *e) {
	pathenv = p;
	env = e;
}

void RunCommand(Command *c, int alrm) {printf("RunCommand: pid is %d, gpid is %d\n", getpid(), getpgrp());
	if (c == NULL)
		return;

	PipeElem *head = c->pipe->pipewith;
	PipeElem *tail = head->pipewith;

	redir_io(c->fin, c->fout);
	if (tail == NULL) {
		execute(head);
	} else {
		runcmd(head);
	}

	exit(0);
}

void runcmd(PipeElem *c) {// printf("runcmd\n");
	if (c->pipewith == NULL)
		execute(c);

	pid_t pid1, pid2;
	int *status1, *status2;
	int p[2];	// file descripters for read or write of a pipe

	pipe_err(p);
	if ((pid1 = fork()) == 0) {
		redir_io(STDIN, p[WR]);
		close_err(p[RD]);
		execute(c);
	}
	if ((pid2 = fork()) == 0) {
		redir_io(p[RD], STDOUT);
		close_err(p[WR]);
		runcmd(c->pipewith);
	}
	close_err(p[RD]);// printf("runcmd -> 1st close\n");
	close_err(p[WR]);// printf("runcmd -> 2nd close\n");
	
	printf("runcmd: pid is %d(gpid: %d); pid1 is %d; pid2 is %d\n", getpid(), getpgrp(), pid1, pid2);
	waitpid(pid1, status1, 0);
	printf("runcmd: hello, %d returns.\n", pid1);
	waitpid(pid2, status2, 0);
	printf("runcmd: hello, %d returns.\n", pid2);
}

void execute(PipeElem *c) {
	char *p = (char *)malloc(256);
	char *q;
	int i;
	for (i = 0; (q = *(pathenv+i)) != NULL; i++) {
		strcpy(p, q);
		execv(strcat(p, c->name), c->argv);
	}
	// if above failed, try unaliasing the command name
	if ((c->name = unalias(c->name)) != NULL) {
		for (i = 0; (q = *(pathenv+i)) != NULL; i++) {
			strcpy(p, q);
			execv(strcat(p, c->name), c->argv);
		}
	}
	perror("execv failed.\n");
	exit(1);
}

int redir_io(int in, int out) {// printf("redir_io\n");
	if (in < 0 || out < 0)
		return -1;
	if (in != STDIN) {
		close_err(STDIN);
		dup_err(in);
		close_err(in);
	}
	if (out != STDOUT) {
		close_err(STDOUT);
		dup_err(out);
		close_err(out);
	}
	return 0;
}

char *unalias(char *name) {
	int i;
	Alias *p;
	unsigned empty[2] = {0, 0};
	for (p = env, i = 0; i < NALIAS_MAX; p++, i++) {
		if (!memcmp(p, empty, sizeof(Alias)))
			continue;

		if (!strcmp(name, p->key))
			return p->val;
	}
	return NULL;
}

void close_err(int fd) {
	if (close(fd)) {
		//printf("close %d failed.\n", fd);
		//perror("close failed.\n");
		exit(1);
	}
}

void dup_err(int fd) {
	if (dup(fd) < 0) {
		perror("dup failed.\n");
		exit(1);
	}
}

void pipe_err(int *p) {
	if (pipe(p) == 0)
		return;
	perror("pipe failed.\n");
	exit(1);
}