#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include "util.h"

typedef enum
{
	STYLE_NORMAL,
	STYLE_INFO,
	STYLE_ERROR,
	STYLE_DEBUG,
	STYLE_HEXDUMP
} OutputStyle;

typedef union
{
	uint64_t uint64;
	unsigned char bytes[sizeof(uint64_t)];
} Uint64Bytes;

static const char *prog_name = NULL;
static int debug_enabled = 0;
static int style_enabled = 1;

static int lockFile(FILE *file)
{
	int oldState;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
	flockfile(file);
	return oldState;
}

static void unlockFile(FILE *file, int savedCancelState)
{
	fflush(file);
	funlockfile(file);
	pthread_setcancelstate(savedCancelState, NULL);
}

static void setStyle(FILE *file, OutputStyle style)
{
	if(style_enabled && isatty(fileno(file)))
	{
		switch(style)
		{
			case STYLE_NORMAL:
				fputs("\033[0;39;49m", file);	//reset attributes and foreground color
				break;
			case STYLE_INFO:
				fputs("\033[1;39;49m", file);	//bold, default colors
				break;
			case STYLE_ERROR:
				fputs("\033[1;31;49m", file);	//bold, red
				break;
			case STYLE_DEBUG:
				fputs("\033[0;33;49m", file);	//regular, yellow
				break;
			case STYLE_HEXDUMP:
				fputs("\033[0;32;49m", file);	//regular, green
				break;
		}
	}
}

static void printIdentifier(FILE *file)
{
	fprintf(file, "%s [%ju]: ", getProgName(), (uintmax_t)getpid());
}

void utilInit(const char *argv0)
{
	assert(prog_name == NULL);

	setvbuf(stderr, NULL, _IOFBF, BUFSIZ);
	prog_name = argv0;
}

const char *getProgName(void)
{
	assert(prog_name != NULL);

	return prog_name;
}

void debugEnable(void)
{
	debug_enabled = 1;
}

int debugEnabled(void)
{
	return debug_enabled;
}

void debugDisable(void)
{
	debug_enabled = 0;
}

void styleEnable(void)
{
	style_enabled = 1;
}

int styleEnabled(void)
{
	return style_enabled;
}

void styleDisable(void)
{
	style_enabled = 0;
}

void normalPrint(const char *fmt, ...)
{
	va_list args;
	int savedCancelState;

	va_start(args, fmt);
	savedCancelState = lockFile(stderr);
	printIdentifier(stderr);
	vfprintf(stderr, fmt, args);
	putc_unlocked('\n', stderr);
	unlockFile(stderr, savedCancelState);
	va_end(args);
}

void debugPrint(const char *fmt, ...)
{
	va_list args;
	int savedCancelState;

	if(debug_enabled)
	{
		va_start(args, fmt);
		savedCancelState = lockFile(stderr);
		setStyle(stderr, STYLE_DEBUG);
		printIdentifier(stderr);
		vfprintf(stderr, fmt, args);
		putc_unlocked('\n', stderr);
		setStyle(stderr, STYLE_NORMAL);
		unlockFile(stderr, savedCancelState);
		va_end(args);
	}
}

void infoPrint(const char *fmt, ...)
{
	va_list args;
	int savedCancelState;

	va_start(args, fmt);
	savedCancelState = lockFile(stderr);
	setStyle(stderr, STYLE_INFO);
	printIdentifier(stderr);
	vfprintf(stderr, fmt, args);
	putc_unlocked('\n', stderr);
	setStyle(stderr, STYLE_NORMAL);
	unlockFile(stderr, savedCancelState);
	va_end(args);
}

void errorPrint(const char *fmt, ...)
{
	va_list args;
	int savedCancelState;

	va_start(args, fmt);
	savedCancelState = lockFile(stderr);
	setStyle(stderr, STYLE_ERROR);
	printIdentifier(stderr);
	vfprintf(stderr, fmt, args);
	putc_unlocked('\n', stderr);
	setStyle(stderr, STYLE_NORMAL);
	unlockFile(stderr, savedCancelState);
	va_end(args);
}

void errnoPrint(const char *prefixFmt, ...)
{
	va_list args;
	int savedCancelState;
	int savedErrno = errno;

	va_start(args, prefixFmt);
	savedCancelState = lockFile(stderr);
	setStyle(stderr, STYLE_ERROR);
	printIdentifier(stderr);
	vfprintf(stderr, prefixFmt, args);
	fputs(": ", stderr);
	errno = savedErrno;
	perror(NULL);
	setStyle(stderr, STYLE_NORMAL);
	unlockFile(stderr, savedCancelState);
	va_end(args);
}

void debugHexdump(const void *ptr, size_t n, const char *fmt, ...)
{
	va_list args;

	if(debug_enabled)
	{
		va_start(args, fmt);
		vhexdump(ptr, n, fmt, args);
		va_end(args);
	}
}

void hexdump(const void *ptr, size_t n, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vhexdump(ptr, n, fmt, args);
	va_end(args);
}

void vhexdump(const void *ptr, size_t n, const char *fmt, va_list args)
{
	static const size_t charsPerLine = 16U;
	const unsigned char *array = (const unsigned char *)ptr;
	const size_t fullLines = n/charsPerLine;
	const size_t incompleteLine = n%charsPerLine;
	const size_t totalLines = incompleteLine ? fullLines + 1U : fullLines;

	const int savedCancelState = lockFile(stderr);
	setStyle(stderr, STYLE_HEXDUMP);

	for(size_t line=0; line<totalLines; ++line)
	{
		//program name and process id
		printIdentifier(stderr);

		//prefix
		va_list a;
		va_copy(a, args);
		vfprintf(stderr, fmt, a);
		va_end(a);
		fputs(": ", stderr);

		const size_t columns = line >= fullLines ? incompleteLine : charsPerLine;
		size_t column;

		//bytes as hex values
		for(column=0; column<columns; ++column)
			fprintf(stderr, "%02x ", (unsigned)array[line*charsPerLine + column]);

		//fill empty hex value spaces in last line
		while(column++ < charsPerLine)
			fputs("   ", stderr);

		//space between hex values and ASCII characters
		fprintf(stderr, "%3s", "");

		//bytes as ASCII characters
		for(column=0; column<columns; ++column)
		{
			char byte = array[line*charsPerLine + column];
			fputc(isgraph(byte) ? byte : '.', stderr);
		}

		//line ending
		fputc('\n', stderr);
	}

	setStyle(stderr, STYLE_NORMAL);
	unlockFile(stderr, savedCancelState);
}

size_t nameBytesValidate(const char *input, size_t n)
{
	const unsigned char *s = (const unsigned char *)input;
	size_t i;

	for(i=0U; i<n; ++i)
	{
		//Reject lower control characters and spaces
		if(s[i] < 33)
			return i;

		//Reject quotes: "'`
		if(s[i] == 34 || s[i] == 39 || s[i] == 96)
			return i;

		//Reject DEL and above
		if(s[i] >= 127)
			return i;

		//Everything else is okay
	}

	return i;
}

uint64_t ntoh64u(uint64_t network64u)
{
	Uint64Bytes conv = { .uint64 = network64u };
	uint64_t host64u = 0U;

	for(size_t i=0U; i<sizeof(conv.bytes); ++i)
	{
		host64u <<= 8U;
		host64u |= conv.bytes[i];
	}

	return host64u;
}

uint64_t hton64u(uint64_t host64u)
{
	Uint64Bytes conv = { .uint64 = 0U };

	unsigned char *p = conv.bytes + sizeof(conv.bytes);
	for(size_t i=0U; i<sizeof(conv.bytes); ++i)
		*--p = (host64u & ((uint64_t)0xffU << (i*8U))) >> (i*8U);

	return conv.uint64;
}

bool isValidUsername(const char *name, uint8_t len)
{
	if( len < 1 || len > (5 + 31)) // name mindestens 1 Byte 
	{
		debugPrint("Username zu Lang oder Kurz");
		return false;
	}
	for( uint16_t i = 0; i < len; ++i)
	{
		char a = name[i];
		if(a < 33 || a > 126 || a == '\'' || a == '"' || a == '`')
		{
			return false;
		}
	}
	return true;
};