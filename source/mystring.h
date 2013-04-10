#ifndef _MYSTRING_
	#define _MYSTRING_
	char *ms_AllocCopy (char *source, int addbytes); // basically remove/add \n\r, source buffer must contain enought space
	void ms_strtoupper(char *str1);
	char *ms_strstr(char *str1, char *str2);
	int ms_strcmp(char *str1, char *str2);
	char *ms_utf8_to_ascii (char *string);
	u8 *ms_FindStringInBuffer (u8 *buffer, size_t size, char *string);
	
	// This function will return a section of delimited text, ex. "section0;section1;....;sectionn"
	char *ms_GetDelimitedString (char *string, char sep, int idx);
#endif