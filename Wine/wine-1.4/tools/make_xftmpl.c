/*
 * Binary encode X templates from text format.
 *
 * Copyright 2011 Dylan Smith
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "windef.h"
#include "guiddef.h"

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#define TOKEN_NAME         1
#define TOKEN_STRING       2
#define TOKEN_INTEGER      3
#define TOKEN_GUID         5
#define TOKEN_INTEGER_LIST 6
#define TOKEN_FLOAT_LIST   7
#define TOKEN_OBRACE      10
#define TOKEN_CBRACE      11
#define TOKEN_OPAREN      12
#define TOKEN_CPAREN      13
#define TOKEN_OBRACKET    14
#define TOKEN_CBRACKET    15
#define TOKEN_OANGLE      16
#define TOKEN_CANGLE      17
#define TOKEN_DOT         18
#define TOKEN_COMMA       19
#define TOKEN_SEMICOLON   20
#define TOKEN_TEMPLATE    31
#define TOKEN_WORD        40
#define TOKEN_DWORD       41
#define TOKEN_FLOAT       42
#define TOKEN_DOUBLE      43
#define TOKEN_CHAR        44
#define TOKEN_UCHAR       45
#define TOKEN_SWORD       46
#define TOKEN_SDWORD      47
#define TOKEN_VOID        48
#define TOKEN_LPSTR       49
#define TOKEN_UNICODE     50
#define TOKEN_CSTRING     51
#define TOKEN_ARRAY       52

struct parser
{
    FILE *infile;
    FILE *outfile;
    int line_no;
    UINT bytes_output;
    BOOL(*write_bytes)(struct parser *parser, const BYTE *data, DWORD size);
    BOOL error;
};

struct keyword
{
    const char *word;
    WORD token;
};

static const struct keyword reserved_words[] = {
    {"ARRAY", TOKEN_ARRAY},
    {"CHAR", TOKEN_CHAR},
    {"CSTRING", TOKEN_CSTRING},
    {"DOUBLE", TOKEN_DOUBLE},
    {"DWORD", TOKEN_DWORD},
    {"FLOAT", TOKEN_FLOAT},
    {"SDWORD", TOKEN_SDWORD},
    {"STRING", TOKEN_LPSTR},
    {"SWORD", TOKEN_SWORD},
    {"TEMPLATE", TOKEN_TEMPLATE},
    {"UCHAR", TOKEN_UCHAR},
    {"UNICODE", TOKEN_UNICODE},
    {"VOID", TOKEN_VOID},
    {"WORD", TOKEN_WORD}
};

static char *program_name;

static inline BOOL read_byte(struct parser *parser, char *byte)
{
    int c = fgetc(parser->infile);
    *byte = c;
    return c != EOF;
}

static inline BOOL unread_byte(struct parser *parser, char last_byte)
{
    return ungetc(last_byte, parser->infile) != EOF;
}

static inline BOOL read_bytes(struct parser *parser, void *data, DWORD size)
{
    return fread(data, size, 1, parser->infile) > 0;
}

static BOOL write_c_hex_bytes(struct parser *parser, const BYTE *data, DWORD size)
{
    while (size--)
    {
        if (parser->bytes_output % 12 == 0)
            fprintf(parser->outfile, "\n ");
        fprintf(parser->outfile, " 0x%02x,", *data++);
        parser->bytes_output++;
    }
    return TRUE;
}

static BOOL write_raw_bytes(struct parser *parser, const BYTE *data, DWORD size)
{
    return fwrite(data, size, 1, parser->outfile) > 0;
}

static inline BOOL write_bytes(struct parser *parser, const void *data, DWORD size)
{
    return parser->write_bytes(parser, data, size);
}

static inline BOOL write_byte(struct parser *parser, BYTE value)
{
    return write_bytes(parser, &value, sizeof(value));
}

static inline BOOL write_word(struct parser *parser, WORD value)
{
    return write_bytes(parser, &value, sizeof(value));
}

static inline BOOL write_dword(struct parser *parser, DWORD value)
{
    return write_bytes(parser, &value, sizeof(value));
}

static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(const char **)a, *(const char **)b);
}

static BOOL parse_keyword(struct parser *parser, const char *name)
{
    const struct keyword *keyword;

    keyword = bsearch(&name, reserved_words, ARRAY_SIZE(reserved_words),
                      sizeof(reserved_words[0]), compare_names);
    if (!keyword)
        return FALSE;

    return write_word(parser, keyword->token);
}

static BOOL parse_guid(struct parser *parser)
{
    char buf[39];
    GUID guid;
    DWORD tab[10];
    BOOL ret;
    static const char *guidfmt = "<%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X>";

    buf[0] = '<';
    if (!read_bytes(parser, buf + 1, 37)) {
        fprintf(stderr, "%s: Truncated GUID (line %d)\n",
                program_name, parser->line_no);
        parser->error = TRUE;
        return FALSE;
    }
    buf[38] = 0;

    ret = sscanf(buf, guidfmt, &guid.Data1, tab, tab+1, tab+2, tab+3, tab+4, tab+5, tab+6, tab+7, tab+8, tab+9);
    if (ret != 11) {
        fprintf(stderr, "%s: Invalid GUID '%s' (line %d)\n",
                program_name, buf, parser->line_no);
        parser->error = TRUE;
        return FALSE;
    }

    guid.Data2 = tab[0];
    guid.Data3 = tab[1];
    guid.Data4[0] = tab[2];
    guid.Data4[1] = tab[3];
    guid.Data4[2] = tab[4];
    guid.Data4[3] = tab[5];
    guid.Data4[4] = tab[6];
    guid.Data4[5] = tab[7];
    guid.Data4[6] = tab[8];
    guid.Data4[7] = tab[9];

    return write_word(parser, TOKEN_GUID) &&
           write_bytes(parser, &guid, sizeof(guid));
}

static BOOL parse_name(struct parser *parser)
{
    char c;
    int len = 0;
    char name[512];

    while (read_byte(parser, &c) && len < sizeof(name) &&
           (isalnum(c) || c == '_' || c == '-'))
    {
        if (len + 1 < sizeof(name))
            name[len++] = c;
    }
    unread_byte(parser, c);
    name[len] = 0;

    if (parse_keyword(parser, name)) {
        return TRUE;
    } else {
        return write_word(parser, TOKEN_NAME) &&
               write_dword(parser, len) &&
               write_bytes(parser, name, len);
    }
}

static BOOL parse_number(struct parser *parser)
{
    int len = 0;
    char c;
    char buffer[512];
    BOOL dot = FALSE;
    BOOL ret;

    while (read_byte(parser, &c) &&
           ((!len && c == '-') || (!dot && c == '.') || isdigit(c)))
    {
        if (len + 1 < sizeof(buffer))
            buffer[len++] = c;
        if (c == '.')
            dot = TRUE;
    }
    unread_byte(parser, c);
    buffer[len] = 0;

    if (dot) {
        float value;
        ret = sscanf(buffer, "%f", &value);
        if (!ret) {
            fprintf(stderr, "%s: Invalid float token (line %d).\n",
                    program_name, parser->line_no);
            parser->error = TRUE;
        } else {
            ret = write_word(parser, TOKEN_FLOAT) &&
                  write_bytes(parser, &value, sizeof(value));
        }
    } else {
        int value;
        ret = sscanf(buffer, "%d", &value);
        if (!ret) {
            fprintf(stderr, "%s: Invalid integer token (line %d).\n",
                    program_name, parser->line_no);
            parser->error = TRUE;
        } else {
            ret = write_word(parser, TOKEN_INTEGER) &&
                  write_dword(parser, value);
        }
    }

    return ret;
}

static BOOL parse_token(struct parser *parser)
{
    char c;

    if (!read_byte(parser, &c))
        return FALSE;

    switch (c)
    {
        case '\n':
            parser->line_no++;
            /* fall through */
        case '\r':
        case ' ':
        case '\t':
            return TRUE;

        case '{': return write_word(parser, TOKEN_OBRACE);
        case '}': return write_word(parser, TOKEN_CBRACE);
        case '[': return write_word(parser, TOKEN_OBRACKET);
        case ']': return write_word(parser, TOKEN_CBRACKET);
        case '(': return write_word(parser, TOKEN_OPAREN);
        case ')': return write_word(parser, TOKEN_CPAREN);
        case ',': return write_word(parser, TOKEN_COMMA);
        case ';': return write_word(parser, TOKEN_SEMICOLON);
        case '.': return write_word(parser, TOKEN_DOT);

        case '/':
            if (!read_byte(parser, &c) || c != '/') {
                fprintf(stderr, "%s: Invalid single '/' comment token (line %d).\n",
                        program_name, parser->line_no);
                parser->error = TRUE;
                return FALSE;
            }
            /* fall through */
        case '#':
            while (read_byte(parser, &c) && c != '\n');
            return c == '\n';

        case '<':
            return parse_guid(parser);

        case '"':
        {
            int len = 0;
            char buffer[512];

            /* FIXME: Handle '\' (e.g. "valid\"string") */
            while (read_byte(parser, &c) && c != '"') {
                if (len + 1 < sizeof(buffer))
                    buffer[len++] = c;
            }
            if (c == EOF) {
                fprintf(stderr, "%s: Unterminated string (line %d).\n",
                        program_name, parser->line_no);
                parser->error = TRUE;
                return FALSE;
            }
            return write_word(parser, TOKEN_STRING) &&
                   write_dword(parser, len) &&
                   write_bytes(parser, buffer, len);
        }

        default:
            unread_byte(parser, c);
            if (isdigit(c) || c == '-')
                return parse_number(parser);
            if (isalpha(c) || c == '_')
                return parse_name(parser);
            fprintf(stderr, "%s: Invalid character (%d) to start token (line %d).\n",
                    program_name, c, parser->line_no);
            parser->error = TRUE;
            return FALSE;
    }

    return TRUE;
}

static const char *output_file;

static void cleanup_files(void)
{
    if (output_file) unlink(output_file);
}

static void exit_on_signal( int sig )
{
    exit(1);  /* this will call the atexit functions */
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s [OPTIONS] INFILE\n"
                    "Options:\n"
                    "  -i NAME   Output to a c header file, data in variable NAME\n"
                    "  -s NAME   In a c header file, define NAME to be the data size\n"
                    "  -o FILE   Write output to FILE\n",
                    program_name);
}

static char *option_inc_var_name = NULL;
static char *option_inc_size_name = NULL;
static const char *option_outfile_name = "-";

static char **parse_options(int argc, char **argv)
{
    int optc;

    while ((optc = getopt(argc, argv, "hi:o:s:")) != -1)
    {
        switch (optc)
        {
            case 'h':
                usage();
                exit(0);
            case 'i':
                option_inc_var_name = strdup(optarg);
                break;
            case 'o':
                option_outfile_name = strdup(optarg);
                break;
            case 's':
                option_inc_size_name = strdup(optarg);
                break;
        }
    }
    return &argv[optind];
}

int main(int argc, char **argv)
{
    const char *infile_name;
    char header[16];
    struct parser parser;
    char **args;
    char *header_name = NULL;

    program_name = argv[0];

    args = parse_options(argc, argv);
    infile_name = *args++;
    if (!infile_name || *args)
    {
        usage();
        return 1;
    }

    parser.infile = stdin;
    parser.outfile = NULL;
    parser.error = FALSE;

    if (!strcmp(infile_name, "-")) {
        infile_name = "stdin";
    } else if (!(parser.infile = fopen(infile_name, "rb"))) {
        perror(infile_name);
        goto error;
    }

    if (!read_bytes(&parser, header, sizeof(header))) {
        fprintf(stderr, "%s: Failed to read file header\n", program_name);
        goto error;
    }
    if (strncmp(header, "xof ", 4))
    {
        fprintf(stderr, "%s: Invalid magic value '%.4s'\n", program_name, header);
        goto error;
    }
    if (strncmp(header + 4, "0302", 4) && strncmp(header + 4, "0303", 4))
    {
        fprintf(stderr, "%s: Unsupported version '%.4s'\n", program_name, header + 4);
        goto error;
    }
    if (strncmp(header + 8, "txt ", 4))
    {
        fprintf(stderr, "%s: Only support conversion from text encoded X files.",
                program_name);
        goto error;
    }
    if (strncmp(header + 12, "0032", 4) && strncmp(header + 12, "0064", 4))
    {
        fprintf(stderr, "%s: Only 32-bit or 64-bit float format supported, not '%.4s'.\n",
                program_name, header + 12);
        goto error;
    }

    if (!strcmp(option_outfile_name, "-")) {
        option_outfile_name = "stdout";
        parser.outfile = stdout;
    } else {
        output_file = option_outfile_name;
        atexit(cleanup_files);
        signal(SIGTERM, exit_on_signal);
        signal(SIGINT, exit_on_signal);
#ifdef SIGHUP
        signal(SIGHUP, exit_on_signal);
#endif
        if (!(parser.outfile = fopen(output_file, "wb"))) {
            perror(option_outfile_name);
            goto error;
        }
    }

    if (option_inc_var_name)
    {
        char *str_ptr;

        header_name = strrchr(option_outfile_name, '/');
        if (header_name)
            header_name = strdup(header_name + 1);
        else
            header_name = strdup(option_outfile_name);
        if (!header_name) {
            fprintf(stderr, "Out of memory\n");
            goto error;
        }

        str_ptr = header_name;
        while (*str_ptr) {
            if (*str_ptr == '.')
                *str_ptr = '_';
            else
                *str_ptr = toupper(*str_ptr);
            str_ptr++;
        }

        fprintf(parser.outfile,
            "/* File generated automatically from %s; do not edit */\n"
            "\n"
            "#ifndef __WINE_%s\n"
            "#define __WINE_%s\n"
            "\n"
            "unsigned char %s[] = {",
            infile_name, header_name, header_name, option_inc_var_name);

        if (ferror(parser.outfile))
            goto error;

        parser.write_bytes = &write_c_hex_bytes;
    } else {
        parser.write_bytes = &write_raw_bytes;
    }

    parser.bytes_output = 0;
    if (!write_bytes(&parser, "xof 0302bin 0064", 16))
        goto error;

    parser.line_no = 1;
    while (parse_token(&parser));

    if (parser.error || ferror(parser.outfile) || ferror(parser.infile))
        goto error;

    if (option_inc_var_name)
    {
        fprintf(parser.outfile, "\n};\n\n");
        if (option_inc_size_name)
            fprintf(parser.outfile, "#define %s %u\n\n", option_inc_size_name, parser.bytes_output);
        fprintf(parser.outfile, "#endif /* __WINE_%s */\n", header_name);
        if (ferror(parser.outfile))
            goto error;
    }

    fclose(parser.infile);
    fclose(parser.outfile);
    output_file = NULL;

    return 0;
error:
    if (parser.infile) {
        if (ferror(parser.infile))
            perror(infile_name);
        fclose(parser.infile);
    }
    if (parser.outfile) {
        if (ferror(parser.outfile))
            perror(option_outfile_name);
        fclose(parser.outfile);
    }
    return 1;
}
