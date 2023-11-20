#ifndef JSON_PARSER_H
# define JSON_PARSER_H

enum e_json_type {
    LIST,
    BOOL,
    KEYVAL,
    DOUBLE,
    STR,
    EMPTY
};

struct s_keyvals {
    struct s_keyval *vals;
    int size;
};

struct s_list {
    struct s_json *vals;
    int size;
};

struct s_json {
    union {
        struct s_keyvals keyvals;
        struct s_list list;
        int boolean;
        long double n_double;
        char *str;
    };
    enum e_json_type type;
};

struct s_keyval {
    char *key;
    struct s_json val;
};

struct s_json_file {
    struct s_json json;
    char *filename;
    char *memory;
    int mmap_size;
    int size;
    int fd;
};

struct s_json_parser_ctx {
    struct s_json *json;
    char *start;
    char *end;
};

struct s_list_parser {
    char *start;
    char *end;
    struct s_list_parser *next;
};

int json_map(struct s_json_file *jfile);
void json_file_free(struct s_json_file *jfile);
void json_unmap(struct s_json_file *jfile);
void json_print(struct s_json_file *jfile);
int json_parse(struct s_json_file *jfile);

#endif