#ifndef _MYSTRING_
	#define _MYSTRING_
	void ms_strtoupper(char *str1);
	char *ms_strstr(char *str1, char *str2);
	int ms_strcmp(char *str1, char *str2);
	char *ms_utf8_to_ascii (char *string);
	u8 *ms_FindStringInBuffer (u8 *buffer, size_t size, char *string);
#endif