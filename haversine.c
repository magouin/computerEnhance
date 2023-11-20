#include <stdio.h>
#include <string.h>
#include <math.h>

#include "json_parser.h"

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

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

    return 0;
}