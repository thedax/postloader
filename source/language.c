/* 
language.c

simple language handling routine. This is the format.

{tag1}string1\n
{tag2}string2\n
etc...
*/

static char *langBuff = 0;

void SetLangFileBuffer (char *buff)
	{
	langBuff = buff;
	}