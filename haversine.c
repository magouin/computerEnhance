#include <stdio.h>
#include <string.h>
#include <math.h>

#include "json_parser.h"

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define _GNU_SOURCE
#define __STDC_WANT_LIB_EXT1__ 1
// #include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "json_parser.h"

int json_value_parse(struct s_json_parser_ctx jpctx);

void print_parsed_string(char *prefix, char *start, char *end, char *suffix){
    if (start < end)
        printf("%s\"%.*s\"%s", prefix, end - start + 1, start, suffix);
    else {
        printf ("start is after end\n");
    }
}

void json_parser_fail(struct s_json_parser_ctx jpctx) {
    if (jpctx.end - jpctx.start < 20)
        fprintf(stderr, "fail to parse the json at \"%.*s\"\n", jpctx.end - jpctx.start + 1, jpctx.start);
    else
        fprintf(stderr, "fail to parse the json at \"%.10s...%.10s\"\n",
                jpctx.start, jpctx.end - 9);
}

int json_map(struct s_json_file *jfile) {
    struct stat st;

    set_profile;

    jfile->fd = open(jfile->filename, O_RDONLY);
    if (jfile->fd == -1) {
        fprintf(stderr, "Failed to open\n");
        return -1;
    }
    if (fstat(jfile->fd, &st) < 0)
    {
        fprintf(stderr, "Error in fstat\n");
        return -1;
    }
    jfile->size = st.st_size - 1;
    jfile->mmap_size = ((st.st_size / sysconf(_SC_PAGESIZE)) + 1);
    jfile->mmap_size *= sysconf(_SC_PAGESIZE);

    if ((jfile->memory = mmap(NULL, jfile->mmap_size, PROT_READ,
                          MAP_SHARED, jfile->fd, 0)) == MAP_FAILED)
    {
        fprintf(stderr, "Error in mmap\n");
        return -1;
    }
    return 0;
}

void remove_whitspace_be(struct s_json_parser_ctx *jpctx){
    char e;

    while (jpctx->end > jpctx->start) {
        e = *jpctx->end;
        if (e != ' ' && e != '\t' && e != '\n' && e != '\r')
            break;
        jpctx->end--;
    }
}
void remove_whitspace_as(struct s_json_parser_ctx *jpctx) {
    char s;

    while (jpctx->start < jpctx->end) {
        s = *jpctx->start;
        if (s != ' ' && s != '\t' && s != '\n' && s != '\r')
            break;
        jpctx->start++;
    }
}

void remove_whitspace(struct s_json_parser_ctx *jpctx) {
    remove_whitspace_be(jpctx);
    remove_whitspace_as(jpctx);
}

struct s_list_parser *value_split_coma(struct s_json_parser_ctx jpctx, int *size) {
    struct s_list_parser *list = NULL, *end;
    int struct_char[3] = {0};
    char *start;

    *size = 1;
    start = jpctx.start + 1;
    list = malloc(sizeof(struct s_list_parser));
    list->next = NULL;
    end = list;
    while (++jpctx.start < jpctx.end) {
        switch (*jpctx.start)
        {
        case '[':
            struct_char[0]++;
            break;
        case ']':
            struct_char[0]--;
            break;
        case '{':
            struct_char[1]++;
            break;
        case '}':
            struct_char[1]--;
            break;
        case '"':
            struct_char[2] = !struct_char[2];
            break;
        case ',':
            if (struct_char[0] == 0 && struct_char[1] == 0 && struct_char[2] == 0) {
                end->next = malloc(sizeof(struct s_list_parser));
                end->next->next = NULL;
                end->next->start = start;
                end->next->end = jpctx.start;
                start = jpctx.start + 1;
                end = end->next;
                (*size)++;
            }
            break;
        default:
            break;
        }
    }
    list->start = start;
    list->end = jpctx.end;
    return list;
}

void free_list_parser(struct s_list_parser *list) {
    struct s_list_parser *end;

    while (list) {
        end = list->next;
        free(list);
        list = end;
    }
}

char *string_parse(struct s_json_parser_ctx jpctx) {
    return strndup(jpctx.start + 1, jpctx.end - jpctx.start - 1);
}

int keyvals_parse(struct s_json_parser_ctx jpctx) {
    struct s_json_parser_ctx tmp;
    struct s_list_parser *list;
    struct s_list_parser *start;
    char *dpoint;
    int ret = 0;

    list = value_split_coma(jpctx, &jpctx.json->keyvals.size);
    start = list;
    jpctx.json->keyvals.vals = malloc(sizeof(struct s_keyval) * jpctx.json->keyvals.size);
    for (int i = 0; i < jpctx.json->keyvals.size; i++) {
        dpoint = strchr(list->start, ':');
        if (!dpoint) {
            if (jpctx.end - list->start < 20)
                fprintf(stderr, "fail to find ':' in \"%.20s\"\n",
                        list->start);
            else
                fprintf(stderr, "fail to find ':' in \"%.10s...%.10s\"\n",
                        list->start, jpctx.end - 9);
            return -1;
        }
        tmp.end = dpoint - 1;
        tmp.start = list->start;
        remove_whitspace(&tmp);
        if (tmp.start >= tmp.end || *tmp.start != '"' || *tmp.end != '"') {
            if (tmp.end - tmp.start < 20)
                print_parsed_string("", tmp.start, tmp.end, " should be a string\n");
            else
                fprintf(stderr, "%.10s...%.10s should be a string\n",
                        tmp.start, tmp.end - 9);
            return -1;
        }
        jpctx.json->keyvals.vals[i].key = string_parse(tmp);

        tmp.start = dpoint + 1;
        tmp.end = list->end - 1;
        tmp.json = &jpctx.json->keyvals.vals[i].val;
        ret |= json_value_parse(tmp);
        list = list->next;
    }
    free_list_parser(start);
    return ret;
}

int list_parse(struct s_json_parser_ctx jpctx) {
    struct s_json_parser_ctx tmp;
    struct s_list_parser *list;
    struct s_list_parser *start;
    int ret = 0;

    list = value_split_coma(jpctx, &jpctx.json->list.size);
    start = list;

    jpctx.json->list.vals = malloc(sizeof(struct s_json) * jpctx.json->list.size);
    for (int i = 0; i < jpctx.json->list.size; i++) {
        tmp.end = list->end - 1;
        tmp.start = list->start;
        tmp.json = &jpctx.json->list.vals[i];
        ret |= json_value_parse(tmp);
        list = list->next;
    }
    free_list_parser(start);
    return ret;
}

int number_parse(struct s_json_parser_ctx jpctx) {
    double num = 0;
    char *end;

    num = strtod(jpctx.start, &end);
    if (end == jpctx.end + 1)
        jpctx.json->n_double = num;
    else {
        fprintf(stderr, "In number parse // received %lf: ", num);
        json_parser_fail(jpctx);
        return -1;
    }
    return 0;
}

int json_value_parse(struct s_json_parser_ctx jpctx) {
    char s, e;
    int ret = 0;

    remove_whitspace(&jpctx);
    if (jpctx.start > jpctx.end) {
        fprintf(stderr, "No value found ! Do you mean to put an empty value ? then right \"null\"");
        return -1;
    }
    s = *jpctx.start;
    e = *jpctx.end;

    if (s == '{' && e == '}') {
        jpctx.json->type = KEYVAL;
        ret = keyvals_parse(jpctx);
        jpctx.json->type = KEYVAL;
    } else if (s == '[' && e == ']') {
        ret = list_parse(jpctx);
        jpctx.json->type = LIST;
    } else if (s == '"' && e == '"') {
        jpctx.json->str = string_parse(jpctx);
        jpctx.json->type = STR;
    } else if ((s >= '0' && s <= '9') || s == '-' || s == '+') {
        ret = number_parse(jpctx);
        jpctx.json->type = DOUBLE;
    } else if (jpctx.end == jpctx.start) {
        jpctx.json->type = EMPTY;
    } else if (jpctx.end - jpctx.start == 3 && !strncmp(jpctx.start, "null", 4)) {
        jpctx.json->type = EMPTY;
    } else if (jpctx.end - jpctx.start == 3 && !strncmp(jpctx.start, "true", 4)) {
        jpctx.json->boolean = 1;
        jpctx.json->type = BOOL;
    } else if (jpctx.end - jpctx.start == 4 && !strncmp(jpctx.start, "false", 5)) {
        jpctx.json->boolean = 0;
        jpctx.json->type = BOOL;
    } else {
        json_parser_fail(jpctx);
        return -1;
    }
    return ret;
}

int json_parse(struct s_json_file *jfile) {
    struct s_json_parser_ctx jpctx;

    set_profile;

    jpctx.start = jfile->memory;
    jpctx.end = jfile->memory + jfile->size;
    jpctx.json = &jfile->json;
    return json_value_parse(jpctx);
}

void json_free(struct s_json json) {
    struct s_keyvals keyvals;
    struct s_list list;

    set_profile;

    switch (json.type)
    {
    case DOUBLE:
    case BOOL:
    case EMPTY:
        break;
    case KEYVAL:
        keyvals = json.keyvals;
        for (int i = 0; i < keyvals.size; i++) {
            json_free(keyvals.vals[i].val);
            free(keyvals.vals[i].key);
        }
        free(keyvals.vals);
        break;
    case LIST:
        list = json.list;
        for (int i = 0; i < list.size; i++)
            json_free(list.vals[i]);
        free(list.vals);
        break;
    case STR:
        free(json.str);
        break;
    }
}

void json_file_free(struct s_json_file *jfile) {
    set_profile;

    json_free(jfile->json);
}

void json_unmap(struct s_json_file *jfile) {
    set_profile;

    munmap(jfile->memory, jfile->mmap_size);
}

static inline void indent(int n)
{
    putchar('\n');
    for (int i = 0; i < n; i++)
        putchar('\t');
}

void json_value_print(struct s_json json, int tabs) {
    struct s_keyvals keyvals;
    struct s_list list;
    int tabs_ = ((tabs >= 0) ? tabs + 1 : -1);

    set_profile;

    switch (json.type)
    {
    case DOUBLE:
        printf("%lf", json.n_double);
        break;
    case BOOL:
        printf("%d", json.boolean);
        break;
    case EMPTY:
        printf("null");
        break;
    case KEYVAL:
        printf("{");
        keyvals = json.keyvals;
        for (int i = 0; i < keyvals.size; i++) {
            indent(tabs_);
            printf("\"%s\": ", keyvals.vals[i].key);
            json_value_print(keyvals.vals[i].val, tabs_);
            if (i + 1 < keyvals.size)
                printf(", ");
        }
        indent(tabs);
        printf("}");
        break;
    case LIST:
        printf("[");
        list = json.list;
        for (int i = 0; i < list.size; i++) {
            json_value_print(list.vals[i], tabs);
            if (i + 1 < list.size)
                printf(", ");
        }
        printf("]");
        break;
    case STR:
        printf("\"%s\"", json.str);
        break;
    }
}

void json_print(struct s_json_file *jfile) {
    json_value_print(jfile->json, 0);
    printf("\n");
}


double earth_radius = 6372.8;

double radians(double degres) {
    double radconv = M_PI / 180;

    return (radconv * degres);
}

double haversine(double x0, double x1, double y0, double y1, double radius) {
    double ret;
    double dx = radians(x1 - x0);
    double dy = radians(y1 - y0);
    double a, c;

    x0 = radians(x0);
    x1 = radians(x1);

    a = (pow(sin(dx / 2.0), 2)) + cos(x0) * cos(x1) * ((pow(sin(dy / 2.0), 2)));
    c = 2.0 * asin(sqrt(a));
    return radius * c;
}

enum e_points {
    X0 = 1 << 0,
    X1 = 1 << 1,
    Y0 = 1 << 2,
    Y1 = 1 << 3
};

int parse_point(struct s_json *ps, double *ret) {
    struct s_json *val;
    double points[4];
    int check = 0;
    char *key;
    int index;

    if (unlikely(ps->type != KEYVAL))
        fprintf(stderr, "Points has to be a keyval\n");
    else if (unlikely(ps->keyvals.size != 4))
        fprintf(stderr, "Points has to contain 4 coordinates\n");
    else {
        for (int i = 0; i < 4; i++) {
            key = ps->keyvals.vals[i].key;
            val = &ps->keyvals.vals[i].val;
            if (unlikely(strlen(key) != 2 ||
                (key[0] != 'x' && key[0] != 'y') ||
                (key[1] != '0' && key[1] != '1'))) {
                fprintf(stderr, "Points has to contain 4 coordinates \"x0\" \"y0\" \"x1\" and \"y1\"\n");
                return -1;
            }
            index = ((key[0] == 'x') ? 0 : 2) + ((key[1] == '0') ? 0 : 1);
            check += 1 << index;
            if (unlikely(val->type != DOUBLE)) {
                fprintf(stderr, "The coordinates has to be a number\n");
                return (-1);
            }
            points[index] = val->n_double;
        }
        if (unlikely(check != 15))
        {
            fprintf(stderr, "Points has to contain 4 coordinates \"x0\" \"y0\" \"x1\" and \"y1\"\n");
            return (-1);
        }
        *ret = haversine(points[0], points[1], points[2], points[3], earth_radius);
        return 0;
    }
    return -1;
}

double haversine_from_json(struct s_json_file *jfile) {
    struct s_json *obj;
    double ret;
    double values = 0;

    set_profile;

    if (unlikely(jfile->json.type != KEYVAL))
        fprintf(stderr, "Json has to be a keyval\n");
    else if (unlikely(jfile->json.keyvals.size != 1))
        fprintf(stderr, "Json has to containe a uniq key val\n");
    else if (unlikely(strcmp(jfile->json.keyvals.vals[0].key, "pairs")))
        fprintf(stderr, "Missing \"pairs\" key in json keyval\n");
    else if (unlikely(jfile->json.keyvals.vals[0].val.type != LIST))
        fprintf(stderr, "\"pairs\" has to be a list of points\n");
    else {
        obj = &jfile->json.keyvals.vals[0].val;
        for (int i = 0; i < obj->list.size; i++) {
            if (unlikely(parse_point(&obj->list.vals[i], &ret)))
                return -1;
            values += ret;
        }
        return values / obj->list.size;
    }
    return -1;
}

int main(int argc, char **argv) {
    struct s_json_file jfile;
    int ret;

    set_profile;

    if (argc != 2) {
        fprintf(stderr, "Usage %s: \"json file\"\n", argv[0]);
        return -1;
    }

    jfile.filename = argv[1];

    if ((ret = json_map(&jfile)))
        return ret;
    if ((ret = json_parse(&jfile)))
        return ret;
    json_unmap(&jfile);
    printf("haversine average is: %lf\n", haversine_from_json(&jfile));
    // json_print(&jfile);
    json_file_free(&jfile);

    print_profiles();

    return 0;
}