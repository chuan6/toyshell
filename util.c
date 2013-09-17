#include <ctype.h>
#include <stdio.h>
#include <string.h>

char *Norm(char *raw) {
	if (raw == NULL)
		return NULL;
	char *p = raw;					// begin of raw
	char *q = raw + strlen(raw);	// end ('\0') of raw

	for (; isspace(*p); p++);		// forward p to the first non-space character in raw
	if (*p == '\0')
		goto RETURN;

	while (isspace(*--q));			// backward q to the last non-space character in raw
	*++q = '\0';

RETURN:
	return p;
}