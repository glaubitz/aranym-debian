#include <stdio.h>
#include <stdlib.h>

FILE *f1, *f2;
FILE *out;
int numdiffs;

struct ROMdiff {
	unsigned long start;
	unsigned long value;
	int len;
};
#define MAX_DIFFS 200
static struct ROMdiff diffs[MAX_DIFFS];

static void dumpdiff(unsigned long start, int len)
{
	unsigned int c1, c2, c3, c4;
	int i;
	long pos;
	
	if (numdiffs >= MAX_DIFFS)
	{
		fprintf(stderr, "too many diffs, aborting\n");
		exit(1);
	}
	
	if (!(start & 1) && (len & 1))
		len++;
	else if ((start & 1) && (len & 1))
		start--, len++;

	fprintf(out, "/* %06lx */\nstatic unsigned char const romdiff_%d[%d] = { ", start, numdiffs, len);
	pos = ftell(f2);
	fseek(f2, start, SEEK_SET);
	for (i = 0; i < len; )
	{
		c2 = fgetc(f2);
		fprintf(out, "0x%02x", c2);
		++i;
		if (i != len)
			fputs(", ", out);
	}
	fputs(" };\n", out);
	fseek(f2, pos, SEEK_SET);
	
	diffs[numdiffs].start = start;
	diffs[numdiffs].len = len;

	pos = ftell(f1);
	fseek(f1, start, SEEK_SET);
	c1 = fgetc(f1);
	c2 = fgetc(f1);
	c3 = fgetc(f1);
	c4 = fgetc(f1);
	diffs[numdiffs].value = (c1 << 24) | (c2 << 16) | (c3 << 8) | (c4 << 0);
	fseek(f1, pos, SEEK_SET);
	
	numdiffs++;
}


int main(int argc, char **argv)
{
	char *n1, *n2;
	unsigned long ptr;
	unsigned long diffstart = 0;
	int difflen = 0;
	int i;
	
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
		return 1;
	}

	n1 = argv[1];
	n2 = argv[2];
	f1 = fopen(n1, "rb");
	if (f1 == NULL)
	{
		fprintf(stderr, "Error opening '%s'\n", n1);
		return 2;
	}
	f2 = fopen(n2, "rb");
	if (f2 == NULL)
	{
		fprintf(stderr, "Error opening '%s'\n", n2);
		fclose(f1);
		return 3;
	}
	out = stdout;

	ptr = 0;
	numdiffs = 0;
	fprintf(out, "\
/*\n\
 * This is the list of neccessary changes\n\
 * for patching the original TOS 4.04 for 68040 compatibility.\n\
 *\n\
 * Autogenerated by tools/romdiff.c tool\n\
 */\n\
\n\
#include \"romdiff.h\"\n\
\n");

	while (!feof(f1) && !feof(f2))
	{
		unsigned char c1, c2;

		c1 = fgetc(f1);
		c2 = fgetc(f2);
		if (c1 != c2)
		{
			if (difflen == 0)
				diffstart = ptr;
			difflen++;
			ptr++;
		} else if (difflen != 0)
		{
			/* peek at next chars; if they are different again, continue */
			c1 = fgetc(f1);
			c2 = fgetc(f2);
			if (c1 == c2)
			{
				dumpdiff(diffstart, difflen);
				difflen = 0;
				ptr += 2;
			} else
			{
				difflen += 2;
				ptr += 2;
			}
		} else
		{
			ptr++;
		}
	}
	if (difflen != 0)
		dumpdiff(diffstart, difflen);

	fprintf(out, "\n");
	fprintf(out, "ROMdiff const tosdiff[] = {\n");
	for (i = 0; i < numdiffs; i++)
	{
		fprintf(out, "\t{ 0x%06lx, 0x%08lx, %3d, romdiff_%d },\n", diffs[i].start, diffs[i].value, diffs[i].len, i);
	}
	fprintf(out, "\t{ 0, 0, 0, 0 }\n};\n");
	fflush(out);

	fclose(f1);
	fclose(f2);
}
