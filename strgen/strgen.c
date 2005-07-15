#define STRGEN

#include "../stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if !defined(WIN32) || defined(__CYGWIN__)
#include <unistd.h>
#endif

#ifdef __MORPHOS__
#ifdef stderr
#undef stderr
#endif
#define stderr stdout
#endif // __MORPHOS__

/* Compiles a list of strings into a compiled string list */

typedef void (*ParseCmdProc)(char *buf, int value);

typedef struct {
	uint32 ident;
	uint32 version;			// 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];			// the international name of this language
	char own_name[32];	// the localized name of this language
	char isocode[16];	// the ISO code for the language (not country code)
	uint16 offsets[32];	// the offsets
} LanguagePackHeader;

typedef struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
} CmdStruct;

typedef struct LineName {
	struct LineName *hash_next;
	int value;
	char str[VARARRAY_SIZE];
} LineName;

static int _cur_line;
static bool _warnings;

static uint32 _hash;
static char _lang_name[32];
static char _lang_ownname[32];
static char _lang_isocode[16];

#define HASH_SIZE 1023
static LineName *_hash_head[HASH_SIZE];

static uint HashStr(const char *s)
{
	uint hash = 0;
	for (; *s; s++) hash = ((hash << 3) | (hash >> 29)) ^ *s;
	return hash % HASH_SIZE;
}

static void HashAdd(const char *s, int value)
{
	uint len = strlen(s) + 1;
	LineName *ln = malloc(sizeof(*ln) + len);
	uint hash = HashStr(s);

	ln->hash_next = _hash_head[hash];
	_hash_head[hash] = ln;
	ln->value = value;
	memcpy(ln->str, s, len);
}

static int HashFind(const char *s)
{
	LineName *ln;

	for (ln = _hash_head[HashStr(s)]; ln != NULL; ln = ln->hash_next) {
		if (strcmp(ln->str, s) == 0) return ln->value;
	}
	return -1;
}

static void warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	fprintf(stderr, "%d: ERROR: %s\n", _cur_line, buf);
	_warnings = true;
}

static void NORETURN error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	fprintf(stderr, "%d: FATAL: %s\n", _cur_line, buf);
	exit(1);
}

static void ttd_strlcpy(char *dst, const char *src, size_t len)
{
	assert(len > 0);
	while (--len && *src)
		*dst++=*src++;
	*dst = 0;
}


// first byte tells if it's english.lng or a custom language file
// second part is the name of the string
// third part is the actual string contents
static char *allstr[65536];

static byte _put_buf[4096];
static int _put_pos;
static int _next_string_id;

static void put_byte(byte c)
{
	if (_put_pos == lengthof(_put_buf))
		error("Put buffer too small");
	_put_buf[_put_pos++] = c;
}

static void emit_buf(int ent)
{
	char *s;

	if (ent < 0 || ent >= 0x10000) {
		warning("Invalid string ID %d\n", ent);
		return;
	}

	if (allstr[ent] != 0) {
		warning("Duplicate string ID %d\n", ent);
		return;
	}

	// Allocate the string, and put the uint16 before with the length.
	s = (char*)malloc(sizeof(uint16) + _put_pos);
	*((uint16*)s) = _put_pos;
	memcpy(s + sizeof(uint16), _put_buf, _put_pos);

	allstr[ent] = s;

	_put_pos = 0;
}

static void EmitSingleByte(char *buf, int value)
{
	if (*buf != '\0') warning("Ignoring trailing letters in command");
	put_byte((byte)value);
}

static void EmitEscapedByte(char *buf, int value)
{
	if (*buf != '\0') warning("Ignoring trailing letters in command");
	put_byte((byte)0x85);
	put_byte((byte)value);
}


static void EmitStringInl(char *buf, int value)
{
	int id;

	if (*buf >= '0' && *buf <= '9') {
		id = strtol(buf, NULL, 0);
		if (id < 0 || id >= 0x10000) {
			warning("Invalid inline num %s\n", buf);
			return;
		}
	} else {
		id = HashFind(buf);
		if (id == -1) {
			warning("Invalid inline string '%s'", buf);
			return;
		}
	}

	put_byte(0x81);
	put_byte((byte)(id & 0xFF));
	put_byte((byte)(id >> 8));
}

static void EmitSetX(char *buf, int value)
{
	char *err;
	int x = strtol(buf, &err, 0);

	if (*err != '\0') error("SetX param invalid");
	put_byte(1);
	put_byte((byte)x);
}

static void EmitSetXY(char *buf, int value)
{
	char *err;
	int x;
	int y;

	x = strtol(buf, &err, 0);
	if (*err != '\0') error("SetXY param invalid");
	y = strtol(err + 1, &err, 0);
	if (*err != '\0') error("SetXY param invalid");

	put_byte(0x1F);
	put_byte((byte)x);
	put_byte((byte)y);
}

static const CmdStruct _cmd_structs[] = {
	// Update position
	{"SETX", EmitSetX, 1},
	{"SETXY", EmitSetXY, 2},

	// Font size
	{"TINYFONT", EmitSingleByte, 8},
	{"BIGFONT", EmitSingleByte, 9},

	// New line
	{"", EmitSingleByte, 10},

	// Colors
	{"BLUE", EmitSingleByte, 15},
	{"SILVER", EmitSingleByte, 16},
	{"GOLD", EmitSingleByte, 17},
	{"RED", EmitSingleByte, 18},
	{"PURPLE", EmitSingleByte, 19},
	{"LTBROWN", EmitSingleByte, 20},
	{"ORANGE", EmitSingleByte, 21},
	{"GREEN", EmitSingleByte, 22},
	{"YELLOW", EmitSingleByte, 23},
	{"DKGREEN", EmitSingleByte, 24},
	{"CREAM", EmitSingleByte, 25},
	{"BROWN", EmitSingleByte, 26},
	{"WHITE", EmitSingleByte, 27},
	{"LTBLUE", EmitSingleByte, 28},
	{"GRAY", EmitSingleByte, 29},
	{"DKBLUE", EmitSingleByte, 30},
	{"BLACK", EmitSingleByte, 31},

	// 0x7B=123 is the LAST special character we may use.

	// Numbers
	{"COMMA", EmitSingleByte, 0x7B},

	{"NUMU16", EmitSingleByte, 0x7E}, // These two do the same thing nowadays
	{"INT32", EmitSingleByte, 0x7E},	// signed 32 bit integer

	{"CURRENCY", EmitSingleByte, 0x7F},

	// 0x85
	{"CURRCOMPACT", EmitEscapedByte, 0},		// compact currency (32 bits)
	{"REV", EmitEscapedByte, 2},						// openttd revision string
	{"SHORTCARGO", EmitEscapedByte, 3},			// short cargo description, only ### tons, or ### litres
	{"CURRCOMPACT64", EmitEscapedByte, 4},	// compact currency 64 bits

	{"STRINL", EmitStringInl, 0x81},

	{"DATE_LONG", EmitSingleByte, 0x82},
	{"DATE_SHORT", EmitSingleByte, 0x83},

	{"VELOCITY", EmitSingleByte, 0x84},
	{"SKIP16", EmitSingleByte, 0x85},
	{"SKIP", EmitSingleByte, 0x86},
	{"VOLUME", EmitSingleByte, 0x87},

	{"STRING", EmitSingleByte, 0x88},

	{"CARGO", EmitSingleByte, 0x99},
	{"STATION", EmitSingleByte, 0x9A},
	{"TOWN", EmitSingleByte, 0x9B},
	{"CURRENCY64", EmitSingleByte, 0x9C},
	{"WAYPOINT", EmitSingleByte, 0x9D}, // waypoint name
	{"DATE_TINY", EmitSingleByte, 0x9E},
	// 0x9E=158 is the LAST special character we may use.

	{"UPARROW", EmitSingleByte, 0xA0},
	{"POUNDSIGN", EmitSingleByte, 0xA3},
	{"YENSIGN", EmitSingleByte, 0xA5},
	{"COPYRIGHT", EmitSingleByte, 0xA9},
	{"DOWNARROW", EmitSingleByte, 0xAA},
	{"CHECKMARK", EmitSingleByte, 0xAC},
	{"CROSS", EmitSingleByte, 0xAD},
	{"RIGHTARROW", EmitSingleByte, 0xAF},

	{"TRAIN", EmitSingleByte, 0xb4},
	{"LORRY", EmitSingleByte, 0xb5},
	{"BUS",   EmitSingleByte, 0xb6},
	{"PLANE", EmitSingleByte, 0xb7},
	{"SHIP",  EmitSingleByte, 0xb8},

	{"SMALLUPARROW", EmitSingleByte, 0xBC},
	{"SMALLDOWNARROW", EmitSingleByte, 0xBD},
	{"THREE_FOURTH", EmitSingleByte, 0xBE},
};

static const CmdStruct *FindCmd(const char *s, int len)
{
	const CmdStruct *cs;

	for (cs = _cmd_structs; cs != endof(_cmd_structs); cs++) {
		if (strncmp(cs->cmd, s, len) == 0 && cs->cmd[len] == '\0') return cs;
	}
	return NULL;
}


/* Returns command struct of next command in str
 * NULL if command is invalid or if there are no more commands in str
 */
static const CmdStruct *ParseCommandString(char **str, char *param, const char *errortext)
{
	char *s = *str;
	char *start;
	byte c;
	const CmdStruct *cmd;

	for (; *s != '{'; s++) if (*s == '\0') return NULL;
	s++;

	// parse command name
	start = s;
	for(;;) {
		c = *s++;
		if (c == '}' || c == ' ')
			break;
		if (c == '\0') {
			if (errortext) warning("Missing } from command '%s' in '%s'", start, errortext);
			return NULL;
		}
	}

	cmd = FindCmd(start, s - start - 1);
	if (cmd == NULL) {
		if (errortext) warning("Undefined command '%.*s' in '%s'", s - start - 1, start, errortext);
		return NULL;
	}

	if (c == ' ') {
		// copy params
		start = s;
		for(;;) {
			c = *s++;
			if (c == '}') break;
			if (c == 0) {
				if (errortext) warning("Missing } from command '%s' in '%s'", start, errortext);
				return NULL;
			}
			*param++ = c;
		}

	}
	*param = 0;

	*str = s;

	return cmd;
}


static void handle_pragma(char *str)
{
	if (memcmp(str, "id ", 3) == 0) {
		_next_string_id = strtoul(str + 3, NULL, 0);
	} else if (memcmp(str, "name ", 5) == 0) {
		ttd_strlcpy(_lang_name, str + 5, sizeof(_lang_name));
	} else if (memcmp(str, "ownname ", 8) == 0) {
		ttd_strlcpy(_lang_ownname, str + 8, sizeof(_lang_ownname));
	} else if (memcmp(str, "isocode ", 8) == 0) {
		ttd_strlcpy(_lang_isocode, str + 8, sizeof(_lang_isocode));
	} else {
		error("unknown pragma '%s'", str);
	}
}

static bool check_commands_match(char *a, char *b)
{
	const CmdStruct *ar;
	const CmdStruct *br;
	char param[100];

	do {
		ar = ParseCommandString(&a, param, NULL);
		br = ParseCommandString(&b, param, NULL);

		// make sure they are identical
		if (ar != br) return false;
	} while (ar != NULL);

	return true;
}

static void handle_string(char *str, bool master)
{
	char *s;
	char *t;
	char *r;
	int ent;

	if (*str == '#') {
		if (str[1] == '#' && str[2] != '#')
			handle_pragma(str + 2);
		return;
	}

	// Ignore comments & blank lines
	if (*str == ';' || *str == ' ' || *str == '\0')
		return;

	s = strchr(str, ':');
	if (s == NULL) {
		warning("Line has no ':' delimiter");
		return;
	}

	// Trim spaces
	for (t = s; t > str && (t[-1] == ' ' || t[-1] == '\t'); t--);
	*t = '\0';

	ent = HashFind(str);

	s++; // skip :

	// allocate string entry
	r = malloc(strlen(str) + strlen(s) + 3);
	*r = master;
	strcpy(r + 1, str);
	strcpy(r + 2 + strlen(str), s);

	if (master) {
		if (ent != -1) {
			warning("String name '%s' is used multiple times", str);
			return;
		}

		ent = _next_string_id++;
		if (allstr[ent]) {
			warning("String ID 0x%X for '%s' already in use by '%s'", ent, str, allstr[ent] + 1);
			return;
		}
		allstr[ent] = r;

		// add to hash table
		HashAdd(str, ent);
	} else {
		if (ent == -1) {
			warning("String name '%s' does not exist in master file", str);
			_warnings = 0; // non-fatal
			return;
		}

		if (!allstr[ent][0]) {
			warning("String name '%s' is used multiple times", str);
			_warnings = 0; // non-fatal
			return;
		}

		// check that the commands match
		if (!check_commands_match(s, allstr[ent] + 2 + strlen(allstr[ent] + 1))) {
			fprintf(stderr, "Warning: String name '%s' does not match the layout of the master string\n", str);
			_warnings = 0; // non-fatal
			return;
		}

		if (s[0] == ':' && s[1] == '\0') {
			allstr[ent][0] = 0; // use string from master file legitiately
			free(r);
		} else {
			free(allstr[ent]);
			allstr[ent] = r;
		}
	}
}

static uint32 my_hash_str(uint32 hash, const char *s)
{
	for (; *s != '\0'; s++) {
		hash = ((hash << 3) | (hash >> 29)) ^ *s;
		if (hash & 1) hash = (hash >> 1) ^ 0xDEADBEEF; else hash >>= 1;
	}
	return hash;

}

static void parse_file(const char *file, bool english)
{
	char buf[2048];
	int i;
	FILE *in;

	in = fopen(file, "r");
	if (in == NULL) error("Cannot open file '%s'", file);

	_cur_line = 1;
	while (fgets(buf, sizeof(buf),in) != NULL) {
		i = strlen(buf);
		while (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n' || buf[i - 1] == ' ')) i--;
		buf[i] = '\0';

		handle_string(buf, english);
		_cur_line++;
	}

	fclose(in);

	// make a hash of the file to get a unique "version number"
	if (english) {
		uint32 hash = 0;

		for (i = 0; i != 65536; i++) {
			char *s = allstr[i];

			if (s != NULL) {
				const CmdStruct *cs;

				hash ^= i * 0x717239;
				if (hash & 1) hash = (hash >> 1) ^ 0xDEADBEEF; else hash >>= 1;
				hash = my_hash_str(hash, s + 1);

				s = s + 2 + strlen(s + 1);
				while ((cs = ParseCommandString(&s, buf, NULL)) != NULL) {
					hash ^= (cs - _cmd_structs) * 0x1234567;
					if (hash & 1) hash = (hash >> 1) ^ 0xF00BAA4; else hash >>= 1;
				}
			}
		}
		_hash = hash;
	}
}

static int count_inuse(int grp)
{
	int i;

	for (i = 0x800; --i >= 0;) {
		if (allstr[(grp << 11) + i] != NULL) break;
	}
	return i + 1;
}

static void check_all_used(void)
{
	int i;
	LineName *ln;
	int num_warn = 10;

	for (i = 0; i != HASH_SIZE; i++) {
		for (ln = _hash_head[i]; ln != NULL; ln = ln->hash_next) {
			if (allstr[ln->value] == 0) {
				if (++num_warn < 50) {
					warning("String %s has no definition. Using NULL value", ln->str);
				}
				_put_pos = 0;
				emit_buf(ln->value);
			}
		}
	}
}

static void write_length(FILE *f, uint length)
{
	if (length < 0xC0) {
		fputc(length, f);
	} else if (length < 0x4000) {
		fputc((length >> 8) | 0xC0, f);
		fputc(length & 0xFF, f);
	} else {
		error("string too long");
	}
}

static void gen_output(FILE *f)
{
	uint16 in_use[32];
	uint16 in_use_file[32];
	int i;
	int j;
	int tot_str = 0;

	check_all_used();

	for (i = 0; i != 32; i++) {
		int n = count_inuse(i);

		in_use[i] = n;
		in_use_file[i] = TO_LE16(n);
		tot_str += n;
	}

	fwrite(in_use_file, 32 * sizeof(uint16), 1, f);

	for (i = 0; i != 32; i++) {
		for (j = 0; j != in_use[i]; j++) {
			char *s = allstr[(i << 11) + j];
			if (s == NULL) error("Internal error, s == NULL");

			write_length(f, *(uint16*)s);
			fwrite(s + sizeof(uint16), *(uint16*)s , 1, f);
			tot_str--;
		}
	}

	fputc(0, f); // write trailing nul character.

	if (tot_str != 0) {
		error("Internal error, tot_str != 0");
	}
}

static bool compare_files(const char *n1, const char *n2)
{
	FILE *f1, *f2;
	char b1[4096];
	char b2[4096];
	size_t l1, l2;

	f2 = fopen(n2, "rb");
	if (f2 == NULL) return false;

	f1 = fopen(n1, "rb");
	if (f1 == NULL) error("can't open %s", n1);

	do {
		l1 = fread(b1, 1, sizeof(b1), f1);
		l2 = fread(b2, 1, sizeof(b2), f2);

		if (l1 != l2 || memcmp(b1, b2, l1) != 0) {
			fclose(f2);
			fclose(f1);
			return false;
		}
	} while (l1);

	fclose(f2);
	fclose(f1);
	return true;
}

static void write_strings_h(const char *filename)
{
	FILE *out;
	int i;
	int next = -1;
	int lastgrp;

	out = fopen("tmp.xxx", "w");
	if (out == NULL) error("can't open tmp.xxx");

	fprintf(out, "enum {");

	lastgrp = 0;

	for (i = 0; i != 65536; i++) {
		if (allstr[i]) {
			if (lastgrp != (i >> 11)) {
				lastgrp = (i >> 11);
				fprintf(out, "};\n\nenum {");
			}

			fprintf(out, next == i ? "%s,\n" : "\n%s = 0x%X,\n", allstr[i] + 1, i);
			next = i + 1;
		}
	}

	fprintf(out, "};\n");

	fprintf(out,
		"\nenum {\n"
		"\tLANGUAGE_PACK_IDENT = 0x474E414C, // Big Endian value for 'LANG' (LE is 0x 4C 41 4E 47)\n"
		"\tLANGUAGE_PACK_VERSION = 0x%X,\n"
		"};\n", (uint)_hash);

	fclose(out);

	if (compare_files("tmp.xxx", filename)) {
		// files are equal. tmp.xxx is not needed
		unlink("tmp.xxx");
	} else {
		// else rename tmp.xxx into filename
#if defined(WIN32)
		unlink(filename);
#endif
		if (rename("tmp.xxx", filename) == -1) error("rename() failed");
	}
}

static void write_langfile(const char *filename, int show_todo)
{
	FILE *f;
	int in_use[32];
	LanguagePackHeader hdr;
	int i;
	int j;
	const CmdStruct *cs;
	char param[128];

	f = fopen(filename, "wb");
	if (f == NULL) error("can't open %s", filename);

	memset(&hdr, 0, sizeof(hdr));
	for (i = 0; i != 32; i++) {
		int n = count_inuse(i);
		in_use[i] = n;
		hdr.offsets[i] = TO_LE16(n);
	}

	// see line 655: fprintf(..."\tLANGUAGE_PACK_IDENT = 0x474E414C,...)
	hdr.ident = TO_LE32(0x474E414C); // Big Endian value for 'LANG'
	hdr.version = TO_LE32(_hash);
	strcpy(hdr.name, _lang_name);
	strcpy(hdr.own_name, _lang_ownname);
	strcpy(hdr.isocode, _lang_isocode);

	fwrite(&hdr, sizeof(hdr), 1, f);

	for (i = 0; i != 32; i++) {
		for (j = 0; j != in_use[i]; j++) {
			char *s = allstr[(i << 11) + j];

			if (s == NULL) {
				write_length(f, 0);
			} else {
				// move to string
				char* str = s + 2 + strlen(s + 1);

				if (show_todo && s[0]) {
					if (show_todo == 2) {
						fprintf(stderr, "Warning:%s: String '%s' is untranslated\n", filename, s + 1);
					} else {
						const char *s = "<TODO> ";
						while(*s) put_byte(*s++);
					}
				}

				while (*str != '\0') {
					if (*str != '{') {
						put_byte(*str++);
					} else {
						cs = ParseCommandString(&str, param, s[0] ? "english.lng" : filename);
						if (cs == NULL) break;
						cs->proc(param, cs->value);
					}
				}

				write_length(f, _put_pos);
				fwrite(_put_buf, 1, _put_pos, f);
				_put_pos = 0;
			}
		}
	}

	fputc(0, f);

	fclose(f);
}

int CDECL main(int argc, char* argv[])
{
	int show_todo = 0;

	if (argc > 1 &&
			(strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		puts("$Revision$");
		return 0;
	}

	if (argc > 1 && strcmp(argv[1], "-t") == 0) {
		show_todo = 1;
		argc--;
		argv++;
	}

	if (argc > 1 && strcmp(argv[1], "-w") == 0) {
		show_todo = 2;
		argc--;
		argv++;
	}


	if (argc == 1) {
		// parse master file
		parse_file("lang/english.txt", true);
		if (_warnings) return 1;

		// write english.lng and strings.h

		write_langfile("lang/english.lng", 0);
		write_strings_h("table/strings.h");
	} else if (argc == 2) {
		char buf[256];
		char *r;

		parse_file("lang/english.txt", true);
		parse_file(argv[1], false);
		if (_warnings) return 1;
		strcpy(buf, argv[1]);
		r = strrchr(buf, '.');
		if (r == NULL || strcmp(r, ".txt") != 0) r = strchr(buf, 0);
		strcpy(r, ".lng");
		write_langfile(buf, show_todo);
	} else {
		fprintf(stderr, "invalid arguments\n");
	}

	return 0;
}
