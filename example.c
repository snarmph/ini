#define INI_IMPLEMENTATION
#include "ini.h"

void print_vec(inivec_t(inistrv_t) vec) {
    printf("[ ");
    ivec_foreach(inistrv_t, v, vec) {
        printf("%.*s, ", (int)v->len, v->buf);
    }
    printf("]\n");
}

bool write_to_buf(const inivalue_t *val, char *buf, size_t buflen) {
    int len = ini_to_str(val, buf, buflen, false);

    if (len < 0) {
        printf("(err) couldn't copy buffer: %s\n", ini_explain(len));
        return false;
    }

    printf("%s, len: %d\n", buf, len);
    return true;
}

int main(int argc, char **argv) {
    const char *file = "example.ini";
    if (argc > 1) {
        file = argv[1];
    }

    ini_t ini = ini_parse(file, NULL);
    if (ini.error) {
        printf("ini file %s failed to parse: %s\n", file, ini_explain(ini.error));
        return 1;
    }
    
    ivec_foreach(initable_t, tab, ini.tables) {
        printf("%.*s\n", (int)tab->name.len, tab->name.buf);
        ivec_foreach(inivalue_t, val, tab->values) {
            printf("\t(%.*s) = (%.*s)\n",
                (int)val->key.len, val->key.buf,
                (int)val->value.len, val->value.buf);
        }
    }

    initable_t *root = ini_get_table(&ini, INI_ROOT);
    inivec_t(inistrv_t) arr = ini_as_array(ini_get(root, "arr"), ' ');
    inivec_t(inistrv_t) arr_delim = ini_as_array(ini_get(root, "arr delim"), ',');
    inivec_t(inistrv_t) non_existent = ini_as_array(ini_get(root, "non-existent"), 0);

    printf("arr:          "); print_vec(arr);
    printf("arr delim:    "); print_vec(arr_delim);
    printf("non-existent: "); print_vec(non_existent);

    ivec_free(arr);
    ivec_free(arr_delim);
    ivec_free(non_existent);

    initable_t *tab = ini_get_table(&ini, "table");
    long long val_int = ini_as_int(ini_get(tab, "int"));
    double val_num  = ini_as_num(ini_get(tab, "num"));
    char *str = ini_as_str(ini_get(root, "str"), true);
    printf("int: %lld\nnum: %.3f\nstr: %s\n", val_int, val_num, str);

    free(str);
    
    initable_t *err_tab = ini_get_table(&ini, "non-existent");
    inivalue_t *err_val = ini_get(err_tab, "non-existent");
    printf("tab: %p, val: %p\n", err_tab, err_val);

    char hello[32] = {0}, too_small[3] = {0};
    printf("hello -> ");
    write_to_buf(ini_get(tab, "hello"), hello, sizeof(hello));
    printf("too-small -> ");
    write_to_buf(ini_get(tab, "hello"), too_small, sizeof(too_small));

    ini_free(&ini);
}
