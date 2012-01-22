#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "mystring.h"

/* not case sensible functions */

char *ms_strstr(char *str1, char *str2)
	{
	if (!str1 || !str2 || !*str1 || !*str2)
		return NULL;
		
	int l1 = strlen(str1);
	int l2 = strlen(str2);

	char *s1 = malloc (l1+1);
	char *s2 = malloc (l2+1);

	int i;
	for (i = 0; i <= l1; i++) s1[i] = toupper((int)str1[i]);
	for (i = 0; i <= l2; i++) s2[i] = toupper((int)str2[i]);
	
	char *p;
	p = strstr (s1, s2);
	
	if (p != NULL)
		p = str1 + (p - s1);
	
	free (s1);
	free (s2);
	
	return p;
	}

int ms_strcmp(char *str1, char *str2)
	{
	if (!str1 || !str2 || !*str1 || !*str2)
		return NULL;
		
	int l1 = strlen(str1);
	int l2 = strlen(str2);

	char *s1 = malloc (l1+1);
	char *s2 = malloc (l2+1);

	int i;
	for (i = 0; i <= l1; i++) s1[i] = toupper((int)str1[i]);
	for (i = 0; i <= l2; i++) s2[i] = toupper((int)str2[i]);
	
	int ret;
	ret = strcmp (s1, s2);
	
	free (s1);
	free (s2);
	
	return ret;
	}
