# ini.h

Simple single-header ini parser in c99

## Usage
In just one c/c++ file do this:
```c
#define INI_IMPLEMENTATION
#include "ini.h"
```

In every other file you can simply include ini.h

## Options

- merge_duplicate_tables:

    By default, when there are multiple tables with the same name, the
    parser simply keeps adding to the tables list, it wastes memory
    but it is also much faster, especially for bigger files.
    if you want the parser to merge all the tables together use this option
- override_duplicate_keys:

    when adding keys, if a table has two same keys, the parser
    adds it to the table anyway, meaning that when it searches for the
    key with ini_get it will get the first one but it will still have
    both in table.values, to override this behaviour and only keep the
    last value use this option
- key_value_divider:

    the default key/value divider is '=', if you want to change it use this option

## Simple example

From file:
```c
ini_t ini = ini_parse("file.ini", NULL);

char *name = ini_dup(ini_get(ini_get_table(&ini, INI_ROOT), "name"));

initable_t *server = ini_get_table(&ini, "server");
int port = (int)ini_as_int(ini_get(server, "port"));
bool use_threads = ini_as_bool(ini_get(server, "use threads"));

ini_free(ini);
free(name);
```

From string:
```c
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
```
