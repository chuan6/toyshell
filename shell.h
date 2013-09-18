#include <stdio.h>

#define CMDBUF 1024
#define NALIAS_MAX 100
#define ALARM_SEC 1

// a single simple form command
typedef struct scmd {
	char *name;		// program name
	char **argv;	// program arguments
	struct scmd *pipewith;
} PipeElem;

// a general form of commands: p1|p2|...|pn <fin >fout
typedef struct {
	PipeElem *pipe;
	int fin;		// system default: stdin
	int fout;		// system default: stdout
} Command;

typedef struct {
	char *key;
	char *val;
} Alias;

// util.c
char *Norm(char *raw);

// parse.c
Command *ParseCommand(char *commandString);
Command *PrintCommand(Command *command);
void FreeCommand(Command *command);

// run.c
void SetEnv(char **path, Alias *env);
void RunCommand(Command *command, int withAlarmOrNot);

// builtin.c
void Builtin_cd(char *pathstr);
void Builtin_alarm(char *str, int *alarm_stat);
void Builtin_alias(char *keyvaluestr, Alias *env);