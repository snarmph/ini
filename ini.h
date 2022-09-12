/*  ini.h - simple single-header ini parser in c99

    in just one c/c++ file do this:
        #define INI_IMPLEMENTATION
        #include "ini.h"

    license:
        see end of file

    options:
        by default, when there are multiple tables with the same name, the
        parser simply keeps adding to the tables list, it wastes memory
        but it is also much faster, especially for bigger files.
        if you want the parser to merge all the tables together use the
        following option:
         - merge_duplicate_tables
        when adding keys, if a table has two same keys, the parser
        adds it to the table anyway, meaning that when it searches for the
        key with ini_get it will get the first one but it will still have
        both in table.values, to override this behaviour and only keep the
        last value use:
         - override_duplicate_keys
        the default key/value divider is '=', if you want to change is use
         - key_value_divider

    usage:
    - simple file:
        ini_t ini = ini_parse("file.ini", NULL);

        char *name = ini_dup(ini_get(ini_get_table(&ini, INI_ROOT), "name"));

        initable_t *server = ini_get_table(&ini, "server");
        int port = (int)ini_as_int(ini_get(server, "port"));
        bool use_threads = ini_as_bool(ini_get(server, "use threads"));
        
        ini_free(ini);
        free(name);

    - string:
        const char *ini_str = 
            "name : web-server\n"
            "[server]\n"
            "port : 8080\n"
            "use threads : false";
        ini_t ini = ini_parse_str(ini_str, &(iniopts_t){ .key_value_divider = ':' });

        char *name = ini_dup(ini_get(ini_get_table(&ini, INI_ROOT), "name"));

        initable_t *server = ini_get_table(&ini, "server");
        int port = (int)ini_as_int(ini_get(server, "port"));
        bool use_threads = ini_as_bool(ini_get(server, "use threads"));
        
        ini_free(ini);
        free(name);
*/

#ifndef INI_LIB_HEADER
#define INI_LIB_HEADER

#define _CRT_SECURE_NO_WARNINGS
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define inivec_t(T)                     T *

#define ivec_free(vec)                  ((vec) ? free(_ini_vec_header(vec)), NULL : NULL)
#define ivec_copy(src, dest)            (ivec_free(dest), ivec_reserve(dest, ivec_len(src)), memcpy(dest, src, ivec_len(src)))

#define ivec_push(vec, ...)             (_ini_vec_may_grow(vec, 1), (vec)[_ini_vec_len(vec)] = (__VA_ARGS__), _ini_vec_len(vec)++)
#define ivec_rem(vec, ind)              ((vec) ? (vec)[(ind)] = (vec)[--_ini_vec_len(vec)], NULL : NULL)
#define ivec_rem_it(vec, it)            ivec_rem((vec), (it)-(vec))
#define ivec_len(vec)                   ((vec) ? _ini_vec_len(vec) : 0)
#define ivec_cap(vec)                   ((vec) ? _ini_vec_cap(vec) : 0)

#define ivec_end(vec)                   ((vec) ? (vec) + _ini_vec_len(vec) : NULL)
#define ivec_back(vec)                  ((vec)[_ini_vec_len(vec) - 1])

#define ivec_add(vec, n)                (_ini_vec_may_grow(vec, (n)), _ini_vec_len(vec) += (uint32_t)(n), &(vec)[_ini_vec_len(vec)-(n)])
#define ivec_reserve(vec, n)            (_ini_vec_may_grow(vec, (n)))

#define ivec_clear(vec)                 ((vec) ? _ini_vec_len(vec) = 0 : 0)
#define ivec_pop(vec)                   ((vec)[--_ini_vec_len(vec)])

typedef struct {
    const char *buf;
    size_t len;
} inistrv_t;

typedef struct {
    inistrv_t key;
    inistrv_t value;
} inivalue_t;

typedef struct {
    inistrv_t name;
    inivec_t(inivalue_t) values;
} initable_t;

typedef struct {
    char *text;
    inivec_t(initable_t) tables;
} ini_t;

typedef struct {
    bool merge_duplicate_tables;  // default: false
    bool override_duplicate_keys; // default: false
    char key_value_divider;       // default: =
} iniopts_t;

#define INI_ROOT NULL

// parses a ini file, if options is NULL it uses the defaults options
ini_t ini_parse(const char *filename, const iniopts_t *options);
// parses a ini string, if options is NULL it uses the defaults options
ini_t ini_parse_str(const char *ini_str, const iniopts_t *options);
bool ini_is_valid(ini_t *ctx);
void ini_free(ini_t ctx);

// return a table with name <name>, returns NULL if nothing was found
initable_t *ini_get_table(ini_t *ctx, const char *name);
// return a value with key <key>, returns NULL if nothing was found or if <ctx> is NULL
inivalue_t *ini_get(initable_t *ctx, const char *key);

// returns an allocated vector of values divided by <delim>
// if <delim> is 0 then it defaults to ' ', must be freed with ivec_free
inivec_t(inistrv_t) ini_as_array(const inivalue_t *value, char delim);
uint64_t ini_as_uint(const inivalue_t *value);
int64_t ini_as_int(const inivalue_t *value);
double ini_as_num(const inivalue_t *value);
bool ini_as_bool(const inivalue_t *value);
// copies a value into a c string, must be freed
char *ini_dup(const inivalue_t *value);
// removes escape characters from a string, useful with ini_dup
char *ini_rem_escaped(char *src);

#endif

#ifdef INI_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#ifdef __cplusplus
#define CDECL(type) type
#else
#define CDECL(type) (type)
#endif

#define _ini_vec_header(vec)         ((uint32_t *)(vec) - 2)
#define _ini_vec_cap(vec)            _ini_vec_header(vec)[0]
#define _ini_vec_len(vec)            _ini_vec_header(vec)[1]

#define _ini_vec_need_grow(vec, n)   ((vec) == NULL || _ini_vec_len(vec) + n >= _ini_vec_cap(vec))
#define _ini_vec_may_grow(vec, n)    (_ini_vec_need_grow(vec, (n)) ? _ini_vec_grow(vec, (uint32_t)(n)) : (void)0)
#define _ini_vec_grow(vec, n)        _ini_vec_grow_impl((void **)&(vec), (n), sizeof(*(vec)))

inline static void _ini_vec_grow_impl(void **arr, uint32_t increment, uint32_t itemsize) {
    int newcap = *arr ? 2 * _ini_vec_cap(*arr) + increment : increment + 1;
    void *ptr = realloc(*arr ? _ini_vec_header(*arr) : 0, itemsize * newcap + sizeof(uint32_t) * 2);
    assert(ptr);
    if (ptr) {
        if (!*arr) ((uint32_t *)ptr)[1] = 0;
        *arr = (void *) ((uint32_t *)ptr + 2);
        _ini_vec_cap(*arr) = newcap;
    }
}

static const iniopts_t __ini_default_opts = {
    false, // merge_duplicate_tables
    false, // override_duplicate_keys
    '=',   // key_value_divider
};

typedef struct {
    const char *start;
    const char *cur;
    size_t len;
} __ini_istream_t;

static ini_t __ini_parse_internal(char *text, const iniopts_t *options);
static char *__ini_read_whole_file(const char *filename);
static iniopts_t __ini_set_default_opts(const iniopts_t *options);
static initable_t *__ini_find_table(ini_t *ctx, inistrv_t name);
static inivalue_t *__ini_find_value(initable_t *table, inistrv_t key);
static void __ini_add_table(ini_t *ctx, __ini_istream_t *in, const iniopts_t *options);
static void __ini_add_value(initable_t *table, __ini_istream_t *in, const iniopts_t *options);
static char *__ini_strdup(const char *src, size_t len);

// string stream helper functions
static __ini_istream_t __istr_init(const char *str);
static bool __istr_is_finished(__ini_istream_t *in);
static void __istr_skip_whitespace(__ini_istream_t *in);
static void __istr_ignore(__ini_istream_t *in, char delim);
static void __istr_skip(__ini_istream_t *in);
static inistrv_t __istr_get_view(__ini_istream_t *in, char delim);

// string view helper functions
static inistrv_t __strv_from_str(const char *str);
static inistrv_t __strv_trim(inistrv_t view);
static inistrv_t __strv_sub(inistrv_t view, size_t from, size_t to);
static bool __strv_is_empty(inistrv_t view);
static int __strv_cmp(inistrv_t a, inistrv_t b);

ini_t ini_parse(const char *filename, const iniopts_t *options) {
    return __ini_parse_internal(__ini_read_whole_file(filename), options);
}

ini_t ini_parse_str(const char *ini_str, const iniopts_t *options) {
    return __ini_parse_internal(__ini_strdup(ini_str, strlen(ini_str)), options);
}

bool ini_is_valid(ini_t *ctx) {
    return ctx->text != NULL;
}

void ini_free(ini_t ctx) {
    free(ctx.text);
    for (initable_t *tab = ctx.tables; tab != ivec_end(ctx.tables); ++tab) {
        ivec_free(tab->values);
    }
    ivec_free(ctx.tables);
}

initable_t *ini_get_table(ini_t *ctx, const char *name) {
    if (!name) return ctx->tables;
    else       return __ini_find_table(ctx, __strv_from_str(name));
}

inivalue_t *ini_get(initable_t *ctx, const char *key) {
    return ctx ? __ini_find_value(ctx, __strv_from_str(key)) : NULL;
}   

inivec_t(inistrv_t) ini_as_array(const inivalue_t *value, char delim) {
    if (!value) return NULL;
    if (!delim) delim = ' ';

    inivec_t(inistrv_t) out = NULL;
    inistrv_t v = value->value;

    size_t start = 0;
    for (size_t i = 0; i < v.len; ++i) {
        if (v.buf[i] == delim) {
            inistrv_t arr_val = __strv_trim(__strv_sub(v, start, i));
            if (!__strv_is_empty(arr_val)) {
                ivec_push(out, arr_val);
            }
            start = i + 1;
        }
    }
    inistrv_t last = __strv_trim(__strv_sub(v, start, SIZE_MAX));
    if (!__strv_is_empty(last)) {
        ivec_push(out, last);
    }
    return out;
}

uint64_t ini_as_uint(const inivalue_t *value) {
    if (!value) return 0;
    uint64_t out = strtoull(value->value.buf, NULL, 0);
    if (out == UINT64_MAX) {
        out = 0;
    }
    return out;
}

int64_t ini_as_int(const inivalue_t *value) {
    if (!value) return 0;
    int64_t out = strtoll(value->value.buf, NULL, 0);
    if (out == INT64_MAX || out == INT64_MIN) {
        out = 0;
    }
    return out;
}

double ini_as_num(const inivalue_t *value) {
    if (!value) return 0;
    double out = strtod(value->value.buf, NULL);
    if (out == HUGE_VAL || out == -HUGE_VAL) {
        out = 0;
    }
    return out;
}

bool ini_as_bool(const inivalue_t *value) {
    if (value->value.len == 4 && strncmp(value->value.buf, "true", 4) == 0) {
        return true;
    }
    return false;
}

char *ini_dup(const inivalue_t *value) {
    return __ini_strdup(value->value.buf, value->value.len);
}

char *ini_rem_escaped(char *src) {
    size_t len = strlen(src);
    for (size_t i = 0; i < (len - 1); ++i) {
        if (src[i] == '\\' && 
           (src[i + 1] == ';' || src[i + 1] == '#')
        ) {
            for (size_t k = i; k < (len - 1); ++k) {
                src[k] = src[k + 1];
            }
            src[--len] = '\0';
        }
    }
    return src;
}

static ini_t __ini_parse_internal(char *text, const iniopts_t *options) {
    ini_t ini = { text, NULL };
    if (!text) return ini;
    iniopts_t opts = __ini_set_default_opts(options);
    // add root table
    initable_t root = { CDECL(inistrv_t){ "root", 4 }, NULL };
    ivec_push(ini.tables, root);
    __ini_istream_t in = __istr_init(text);
    while (!__istr_is_finished(&in)) {
        switch (*in.cur) {
            case '[':
                __ini_add_table(&ini, &in, &opts);
                break;
            case '#': case ';':
                __istr_ignore(&in, '\n');
                break;
            default:
                __ini_add_value(ini.tables, &in, &opts);
                break;
        }
        __istr_skip_whitespace(&in);
    }
    return ini;
}

static char *__ini_read_whole_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    size_t len = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    size_t read = fread(buf, 1, len, fp);
    if (read != len) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

static iniopts_t __ini_set_default_opts(const iniopts_t *options) {
    if (!options) return __ini_default_opts;

    iniopts_t opts = __ini_default_opts;

    if (options->override_duplicate_keys)
        opts.override_duplicate_keys = options->override_duplicate_keys;

    if (options->merge_duplicate_tables)
        opts.merge_duplicate_tables = options->merge_duplicate_tables;

    if (options->key_value_divider)
        opts.key_value_divider = options->key_value_divider;

    return opts;
}

static initable_t *__ini_find_table(ini_t *ctx, inistrv_t name) {
    if (__strv_is_empty(name)) return NULL;
    for (uint32_t i = 0; i < ivec_len(ctx->tables); ++i) {
        if (__strv_cmp(ctx->tables[i].name, name) == 0) {
            return ctx->tables + i;
        }
    }
    return NULL;
}

static inivalue_t *__ini_find_value(initable_t *table, inistrv_t key) {
    if (__strv_is_empty(key)) return NULL;
    for (uint32_t i = 0; i < ivec_len(table->values); ++i) {
        if (__strv_cmp(table->values[i].key, key) == 0) {
            return table->values + i;
        }
    }
    return NULL;
}

static void __ini_add_table(ini_t *ctx, __ini_istream_t *in, const iniopts_t *options) {
    __istr_skip(in); // skip [
    inistrv_t name = __istr_get_view(in, ']');
    __istr_skip(in); // skip ]
    initable_t *table = options->merge_duplicate_tables ? __ini_find_table(ctx, name) : NULL;
    if (!table) {
        ivec_push(ctx->tables, CDECL(initable_t){ name, NULL });
        table = &ivec_back(ctx->tables);
    }
    __istr_ignore(in, '\n');
    __istr_skip(in);
    while (!__istr_is_finished(in)) {
        switch (*in->cur) {
            case '\n': case '\r':
                return;
            case '#': case ';':
                __istr_ignore(in, '\n');
                break;
            default:
                __ini_add_value(table, in, options);
                break;
        }
    }
}

static void __ini_add_value(initable_t *table, __ini_istream_t *in, const iniopts_t *options) {
    if (!table) return;

    inistrv_t key = __strv_trim(__istr_get_view(in, options->key_value_divider));
    __istr_skip(in); // skip divider

    inistrv_t val = __istr_get_view(in, '\n');
    // find inline comments
    for (size_t i = 0; i < val.len; ++i) {
        if (val.buf[i] == '#' || val.buf[i] == ';') {
            // can escape # and ; with a backslash (e.g. \#)
            if (i > 0 && val.buf[i-1] == '\\') {
                continue;
            }
            val.len = i;
            break;
        }
    }
    val = __strv_trim(val);

    // value might be until EOF, in that case no use in skipping
    if (!__istr_is_finished(in)) __istr_skip(in); // skip \n
    inivalue_t *new_val = options->override_duplicate_keys ? __ini_find_value(table, key) : NULL;
    if (new_val) {
        new_val->value = val;
    }
    else {
        ivec_push(table->values, CDECL(inivalue_t){ key, val });
    }
}

static char *__ini_strdup(const char *src, size_t len) {
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, src, len);
    buf[len] = '\0';
    return buf;
}

static __ini_istream_t __istr_init(const char *str) {
    return CDECL(__ini_istream_t) { str, str, strlen(str) };
}

static bool __istr_is_finished(__ini_istream_t *in) {
    return (size_t)(in->cur - in->start) >= in->len;
}

static void __istr_skip_whitespace(__ini_istream_t *in) {
    while (*in->cur && isspace(*in->cur)) {
        ++in->cur;
    }
}

static void __istr_ignore(__ini_istream_t *in, char delim) {
    for (; *in->cur && *in->cur != delim; ++in->cur);
}

static void __istr_skip(__ini_istream_t *in) {
    ++in->cur;
}

static inistrv_t __istr_get_view(__ini_istream_t *in, char delim) {
    const char *from = in->cur;
    __istr_ignore(in, delim);
    size_t len = in->cur - from;
    return CDECL(inistrv_t){ from, len };
}

static inistrv_t __strv_from_str(const char *str) {
    return CDECL(inistrv_t){ str, strlen(str) };
}

static inistrv_t __strv_trim(inistrv_t view) {
    inistrv_t out = view;
    // trim left
    for (size_t i = 0; i < view.len && isspace(view.buf[i]); ++i) {
        ++out.buf;
        --out.len;
    }
    // trim right
    for (int64_t i = view.len - 1; i >= 0 && isspace(view.buf[i]); --i) {
        --out.len;
    }
    return out;
}

static inistrv_t __strv_sub(inistrv_t view, size_t from, size_t to) {
    if (to > view.len) to = view.len;
    if (from > to) from = to;
    return CDECL(inistrv_t){ view.buf + from, to - from };
}

static bool __strv_is_empty(inistrv_t view) {
    return view.len == 0;
}

static int __strv_cmp(inistrv_t a, inistrv_t b) {
    if(a.len < b.len) return -1;
    if(a.len > b.len) return  1;
    return memcmp(a.buf, b.buf, a.len);
}

#endif

/*
MIT LICENSE
Copyright 2022 snarmph
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
*/