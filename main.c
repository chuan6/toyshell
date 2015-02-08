#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell.h"

#define NPATH_MAX 5

/**
 * Execute the command.
 **/

static pid_t cmdpid;	// the pid of the process forked from shell,
						//  or, the groupid of all the processes forked later.

static const char *cmd_alias = "alias";
static const char *cmd_cd = "cd";
static const char *cmd_exit = "exit";
static const char *cmd_alarm = "alarm";

typedef struct {
	char *prompt;
	char **path;
} prf;
static prf *readProfile(char *profile);
static void alarm_sighandler(int sig);

int
main(int argc, char *argv[])
{
	// Not to be interrupted by Ctrl-C.
	//sigaction(SIGINT, SIG_IGN, NULL);
	//sigaction(SIGQUIT, SIG_IGN, NULL);
	//signal(SIGALRM, alarm_sighandler);

	char *profile;
	switch (argc) {
	case 1:
		profile = "PROFILE";
		break;
	case 2:
		profile = argv[1];
		break;
	default:
		printf("Please start the program using \"program path_to_profile_file\".\n");
		return 0;
	}

	prf *file;
	if ((file = readProfile(profile)) == NULL)
		return 0;

	Alias *env = (Alias *)calloc(NALIAS_MAX, sizeof(Alias));
	SetEnv(file->path, env);

	char buf[CMDBUF];
	int withAlarm = 0;
	struct sigaction sa;
	int *stat_loc;
	Command *c;
	printf("%s", file->prompt);
	while (fgets(buf, CMDBUF, stdin) != NULL) {
		if (*Norm(buf) == '\0')
			continue;

		if (!strcmp(buf, cmd_exit))
			break;

		if (withAlarm) {
			alarm(ALARM_SEC);
			sa.sa_handler = alarm_sighandler;
			sigemptyset(&sa.sa_mask);	// block sigs of type being handled
			sa.sa_flags = SA_RESTART;	// restart syscalls if possible
			if (sigaction(SIGALRM, &sa, NULL) < 0) {
				perror("Handling of SIGALRM failed.\n");
			}
		}

		if (!strncmp(buf, cmd_cd, strlen(cmd_cd))
			&& isspace(*(buf + strlen(cmd_cd)))) {
			Builtin_cd(buf + strlen(cmd_cd));
		} else if (!strncmp(buf, cmd_alias, strlen(cmd_alias))
			&& isspace(*(buf + strlen(cmd_alias)))) {
			Builtin_alias(buf + strlen(cmd_alias), env);
		} else if (!strncmp(buf, cmd_alarm, strlen(cmd_alarm))
			&& isspace(*(buf + strlen(cmd_alarm)))) {
			Builtin_alarm(buf + strlen(cmd_alarm), &withAlarm);
		} else {
			switch (cmdpid = fork()) {
			case -1:
				fprintf(stderr, "fork() in main failed.\n");
				break;
			case 0:		// child process
				setpgrp();	// change groupid of the child process to its own pid
				RunCommand(c = ParseCommand(buf), withAlarm);
				FreeCommand(c);
				break;
			default:	// parent process; pid > 0
				printf("main: pid at parent: %d (gpid: %d)\n", cmdpid, getpgrp());
				waitpid(cmdpid, stat_loc, 0);printf("main: hello, %d returns.\n", cmdpid);
				break;
			}
		}

		if (withAlarm) {printf("to clear the alarm\n");
			alarm(0);	// command finishes, clear alarm
		}

		printf("%s", file->prompt);
	}
	printf("main: exit from pid: %d\n", getpid());
	//exit(0);
	return 0;
}

prf *readProfile(char *profile) {
	char *prompt = NULL;
	char **path = NULL;
	char *home = NULL;
	prf *r;
	FILE *pf;
	char *buf;
	char *pmem;	// memory to be allocated to store paths

	pf = fopen(profile, "r");
	if (pf == NULL) {
		printf("Failed to open %s.\n", profile);
		return NULL;
	} else {
		buf = (char *)malloc(BUFSIZ);

		char *p = fgets(buf, BUFSIZ, pf);
		char *t;
		for (; p != NULL; p = fgets(buf, BUFSIZ, pf)) {
			t = strtok(p, ":");
			if (prompt==NULL && !strcmp(t, "prompt_sign")) {
				t = strtok(NULL, "\n");
				prompt = (char *)malloc(strlen(t)+1);
				strcpy(prompt, t);
			} else if (path==NULL && !strcmp(t, "path")) {
				pmem = (char *)malloc(BUFSIZ);
				char *pp = pmem;

				path = (char **)malloc((NPATH_MAX + 1)*sizeof(char *));
				int i;
				t = strtok(NULL, "|");
				for (i = 0; i < NPATH_MAX && t != NULL; i++, t=strtok(NULL, "|")) {
					if (*(t = (Norm(t))) != '\0') {
						*(path+i) = strcpy(pp, t);
						pp += strlen(pp)+1;
					}
				}
				*(path+i) = NULL;
			} else if (home == NULL && !strcmp(t, "home")) {
				home = t + strlen(t) + 1;
				Builtin_cd(home);
			}
			if (prompt != NULL && path != NULL && home != NULL) {	// validify paths
				DIR *d;
				char **p;
				int i;
				for (i = 0; *(p = path+i) != NULL; i++) {
					if ((d = opendir(*p)) != NULL) {
						closedir(d);
						printf("Path validified: %s\n", *p);
					} else {
						goto FAIL;
					}
				}
				goto SUCCESS;
			}
		}
		goto FAIL;
	}
SUCCESS:	//printf("SUCCESS %s\n", prompt);
	fclose(pf);
	free(buf);
	r = (prf *)malloc(sizeof(prf));
	r->prompt = prompt;
	r->path = path;
	return r;
FAIL:
	fclose(pf);
	free(buf);
	free(prompt);
	free(pmem);
	free(path);
	printf("Invalid content or wrong path value in %s.\n", profile);
	return NULL;
}


void alarm_sighandler(int sig) {
	printf("inside signal handler.\n");
	if (sig != SIGALRM)
		return;

	pid_t groupid = cmdpid;	printf("alarm_sighandler: groupid to be used is %d\n", groupid);

	// 1st, stop all the processes in process group groupid
	kill(0 - groupid, SIGSTOP);

	// 2nd, ask user, if continue, or abort
	char c;
ASK:
	printf("\nThe command has run for %d seconds now. Do you want to continue? (y/N) ", ALARM_SEC);
	scanf("%c", &c);
	if (c=='y' || c=='Y') {
		// 3rd, user chooses to continue, hence continue all the processes that have been stopped
		kill(0 - groupid, SIGCONT);
	} else if (c=='n' || c=='N') {
		// 3rd, user chooses to abort, hence kill all the processes that have been stopped
		kill(0 - groupid, SIGKILL);
	} else
		goto ASK;
}