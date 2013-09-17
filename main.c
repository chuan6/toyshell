#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

#define NPATH_MAX 5

/**
 * Execute the command.
 **/

static const char *cmd_alias = "alias";
static const char *cmd_cd = "cd";
static const char *cmd_exit = "exit";
static const char *cmd_alarm = "alarm";

typedef struct {
	char *prompt;
	char **path;
} prf;
static prf *readProfile(char *profile);

int
main(int argc, char *argv[])
{
	// Not to be interrupted by Ctrl-C.
	//sigaction(SIGINT, SIG_IGN, NULL);
	//sigaction(SIGQUIT, SIG_IGN, NULL);

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
		goto EXIT;

	Alias *env = (Alias *)calloc(NALIAS_MAX, sizeof(Alias));
	SetEnv(file->path, env);

	char buf[1024];
	int withAlarm = 0;
	int *stat_loc;
	Command *c;
	for (;;) {
		printf("%s", file->prompt);
		if (gets(buf) != NULL) {
			if (!strcmp(buf, cmd_exit))
				goto EXIT;
			if (!strncmp(buf, cmd_cd, strlen(cmd_cd))
				&& isspace(*(buf + strlen(cmd_cd)))) {
				Builtin_cd(buf + strlen(cmd_cd));
				continue;
			}
			if (!strncmp(buf, cmd_alias, strlen(cmd_alias))
				&& isspace(*(buf + strlen(cmd_alias)))) {
				Builtin_alias(buf + strlen(cmd_alias), env);
				continue;
			}
			if (!strncmp(buf, cmd_alarm, strlen(cmd_alarm))
				&& isspace(*(buf + strlen(cmd_alarm)))) {
				Builtin_alarm(buf + strlen(cmd_alarm), &withAlarm);
				continue;
			}
			//if (fork() == 0) {
				RunCommand(c = ParseCommand(buf), withAlarm);
				FreeCommand(c);
			//}
			//wait(stat_loc);
		} else {
			perror("error\n");
			goto EXIT;
		}
	}
EXIT:
	exit(0);
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
