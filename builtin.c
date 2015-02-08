#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

/**
 * Builtin commands of the shell
 **/

void Builtin_cd(char *ps) {
	if (*(ps = Norm(ps)) == '\0') {
		printf("Empty path name; working directory is not changed.\n");
		return;
	}
	if (!chdir(ps))
		return;

	printf("Failed to change working directory to %s\n", ps);
}

void Builtin_alarm(char *s, int *t) {
	if (*(s = Norm(s)) == '\0') {
		printf("Empty argument; alarm setting unchanged.\n");
		return;
	}
	if (!strcmp(s, "on"))
		*t = 1;
	else if (!strcmp(s, "off"))
		*t = 0;
	else
		printf("Invalid argument.\n");
}

void Builtin_alias(char *kv, Alias *env) {
	char *k;
	char *v;
	if (*(kv = Norm(kv)) == '\0') {
		printf("Empty key-value pair; no alias is added to any command.\n");
		return;
	}
	char *p;
	for (p = kv; !isspace(*p) && *p != '\0'; p++)
		;
	if (*p == '\0') {
		printf("Incomplete key-value pair; no alias is added to any command.\n");
		return;
	}
	*p = '\0';
	k = kv;

	kv = Norm(kv + strlen(kv) + 1);
	for (p = kv; !isspace(*p) && *p != '\0'; p++)
		;
	if (*p == '\0') {
		v = kv;
	} else {
		printf("Too many arguments; no alias is added to any command.\n");
		return;
	}

	if (!strcmp(k, v))	// eliminate useless alias
		return;

	Alias *ep = NULL;	// to point to the first empty element in env
	Alias *ap;
	int i;
	for (i = 0, ap = env; i < NALIAS_MAX; i++, ap++) {
		if (ap->key == NULL && ap->val == NULL) {
			if (ep == NULL)
				ep = ap;
			continue;
		}
		// *ap is not empty
		if (!strcmp(k, ap->key)) {	// if key exists already, updating instead of adding
			//printf("cmp 1\n");
			ap->val = v;
			return;
		}
		if (!strcmp(k, ap->val)) {	// eliminate tautology
			//printf("cmp 2\n");
			return;
		}
		if (!strcmp(v, ap->key) ) {	// eliminate chain aliasing
			//printf("cmp 3\n");
			v = ap->val;
			if (ep != NULL)
				break;
			continue;
		}
	}
	if (ep == NULL) {
		printf("Alias container is full; no alias is added to any command.\n");
		return;
	}

	// add the pair to alias
	ep->key = (char *)malloc(strlen(k)+1);
	strcpy(ep->key, k);
	ep->val = (char *)malloc(strlen(v)+1);
	strcpy(ep->val, v);
	return;
}