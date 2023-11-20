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
    long double num = 0;
    char *end;

    num = strtold(jpctx.start, &end);
    if (end == jpctx.end + 1)
        jpctx.json->n_double = num;
    else {
        fprintf(stderr, "In number parse // received %Lf: ", num);
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

    jpctx.start = jfile->memory;
    jpctx.end = jfile->memory + jfile->size;
    jpctx.json = &jfile->json;
    return json_value_parse(jpctx);
}

void json_free(struct s_json json) {
    struct s_keyvals keyvals;
    struct s_list list;

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
    json_free(jfile->json);
}

void json_unmap(struct s_json_file *jfile) {
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

    switch (json.type)
    {
    case DOUBLE:
        printf("%Lf", json.n_double);
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
