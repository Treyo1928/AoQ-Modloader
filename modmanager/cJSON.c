/*
 * cJSON — minimal JSON library (MIT License)
 * Standalone re-implementation for AoQModManager Quest mod.
 */
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

/* ── Allocation helpers ──────────────────────────────────────────────── */

static cJSON *cJSON_New_Item(void)
{
    cJSON *node = (cJSON *)calloc(1, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *c)
{
    cJSON *next;
    while (c) {
        next = c->next;
        if (c->child)       cJSON_Delete(c->child);
        if (c->valuestring) free(c->valuestring);
        if (c->string)      free(c->string);
        free(c);
        c = next;
    }
}

/* ── String helpers ───────────────────────────────────────────────────── */

static char *cJSON_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *out = (char *)malloc(len);
    if (out) memcpy(out, s, len);
    return out;
}

/* ── Parser ───────────────────────────────────────────────────────────── */

static const char *skip_whitespace(const char *in)
{
    while (in && *in && (unsigned char)*in <= ' ')
        in++;
    return in;
}

/* Forward declarations */
static const char *parse_value(cJSON *item, const char *value);
static const char *parse_string(cJSON *item, const char *str);
static const char *parse_number(cJSON *item, const char *num);
static const char *parse_array(cJSON *item, const char *value);
static const char *parse_object(cJSON *item, const char *value);

static const char *parse_string(cJSON *item, const char *str)
{
    const char *ptr = str + 1; /* skip opening quote */
    char *out;
    char *ptr2;
    int   len = 0;

    if (*str != '"') return NULL;

    /* Count length first */
    const char *p = ptr;
    while (*p && *p != '"') {
        if (*p == '\\') { p++; if (!*p) return NULL; }
        len++;
        p++;
    }
    if (!*p) return NULL;

    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    ptr2 = out;

    while (*ptr && *ptr != '"') {
        if (*ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            ptr++;
            switch (*ptr) {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                default:  *ptr2++ = *ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = '\0';
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr + 1; /* skip closing quote */
}

static const char *parse_number(cJSON *item, const char *num)
{
    double n = 0;
    char *endptr = NULL;
    n = strtod(num, &endptr);
    if (endptr == num) return NULL;
    item->valuedouble = n;
    item->valueint    = (int)n;
    item->type        = cJSON_Number;
    return endptr;
}

static const char *parse_array(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '[') return NULL;
    item->type = cJSON_Array;
    value = skip_whitespace(value + 1);
    if (*value == ']') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;

    value = skip_whitespace(parse_value(child, skip_whitespace(value)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
    }

    if (*value == ']') return value + 1;
    return NULL;
}

static const char *parse_object(cJSON *item, const char *value)
{
    cJSON *child;
    if (*value != '{') return NULL;
    item->type = cJSON_Object;
    value = skip_whitespace(value + 1);
    if (*value == '}') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!item->child) return NULL;

    /* parse key */
    value = skip_whitespace(parse_string(child, skip_whitespace(value)));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;

    if (*value != ':') return NULL;

    /* parse value */
    value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;

        value = skip_whitespace(parse_string(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;

        if (*value != ':') return NULL;
        value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
    }

    if (*value == '}') return value + 1;
    return NULL;
}

static const char *parse_value(cJSON *item, const char *value)
{
    if (!value) return NULL;
    value = skip_whitespace(value);
    if (!value) return NULL;

    if (!strncmp(value, "null",  4)) { item->type = cJSON_NULL;  return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; item->valueint = 0; return value + 5; }
    if (!strncmp(value, "true",  4)) { item->type = cJSON_True;  item->valueint = 1; return value + 4; }
    if (*value == '"') return parse_string(value[0] == '"' ? item : item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);

    return NULL;
}

cJSON *cJSON_Parse(const char *value)
{
    cJSON *c = cJSON_New_Item();
    if (!c) return NULL;
    if (!parse_value(c, skip_whitespace(value))) {
        cJSON_Delete(c);
        return NULL;
    }
    return c;
}

/* ── Printer ─────────────────────────────────────────────────────────── */

/* Simple growable buffer */
typedef struct {
    char *buf;
    int   len;
    int   cap;
} PrintBuf;

static int pb_init(PrintBuf *b, int initial)
{
    b->buf = (char *)malloc(initial);
    if (!b->buf) return 0;
    b->buf[0] = '\0';
    b->len = 0;
    b->cap = initial;
    return 1;
}

static int pb_grow(PrintBuf *b, int need)
{
    while (b->len + need + 1 > b->cap) {
        int newcap = b->cap * 2 + 64;
        char *nb = (char *)realloc(b->buf, newcap);
        if (!nb) return 0;
        b->buf = nb;
        b->cap = newcap;
    }
    return 1;
}

static void pb_append(PrintBuf *b, const char *s)
{
    int slen = (int)strlen(s);
    if (!pb_grow(b, slen)) return;
    memcpy(b->buf + b->len, s, slen + 1);
    b->len += slen;
}

static void pb_append_char(PrintBuf *b, char c)
{
    if (!pb_grow(b, 1)) return;
    b->buf[b->len++] = c;
    b->buf[b->len]   = '\0';
}

static void print_value(PrintBuf *b, const cJSON *item, int depth);

static void print_string_ptr(PrintBuf *b, const char *s)
{
    pb_append_char(b, '"');
    if (s) {
        for (const char *p = s; *p; p++) {
            switch (*p) {
                case '"':  pb_append(b, "\\\""); break;
                case '\\': pb_append(b, "\\\\"); break;
                case '\n': pb_append(b, "\\n");  break;
                case '\r': pb_append(b, "\\r");  break;
                case '\t': pb_append(b, "\\t");  break;
                default:
                    pb_append_char(b, *p);
                    break;
            }
        }
    }
    pb_append_char(b, '"');
}

static void print_indent(PrintBuf *b, int depth)
{
    for (int i = 0; i < depth; i++)
        pb_append(b, "    ");
}

static void print_object(PrintBuf *b, const cJSON *item, int depth)
{
    pb_append(b, "{\n");
    cJSON *child = item->child;
    while (child) {
        print_indent(b, depth + 1);
        print_string_ptr(b, child->string);
        pb_append(b, ": ");
        print_value(b, child, depth + 1);
        if (child->next) pb_append_char(b, ',');
        pb_append_char(b, '\n');
        child = child->next;
    }
    print_indent(b, depth);
    pb_append_char(b, '}');
}

static void print_array(PrintBuf *b, const cJSON *item, int depth)
{
    pb_append(b, "[\n");
    cJSON *child = item->child;
    while (child) {
        print_indent(b, depth + 1);
        print_value(b, child, depth + 1);
        if (child->next) pb_append_char(b, ',');
        pb_append_char(b, '\n');
        child = child->next;
    }
    print_indent(b, depth);
    pb_append_char(b, ']');
}

static void print_value(PrintBuf *b, const cJSON *item, int depth)
{
    if (!item) { pb_append(b, "null"); return; }
    char tmp[64];
    switch (item->type) {
        case cJSON_NULL:   pb_append(b, "null");  break;
        case cJSON_False:  pb_append(b, "false"); break;
        case cJSON_True:   pb_append(b, "true");  break;
        case cJSON_Number:
            if (item->valuedouble == (double)(int)item->valuedouble)
                snprintf(tmp, sizeof(tmp), "%d", (int)item->valuedouble);
            else
                snprintf(tmp, sizeof(tmp), "%g", item->valuedouble);
            pb_append(b, tmp);
            break;
        case cJSON_String:
            print_string_ptr(b, item->valuestring);
            break;
        case cJSON_Array:
            print_array(b, item, depth);
            break;
        case cJSON_Object:
            print_object(b, item, depth);
            break;
        default: pb_append(b, "null"); break;
    }
}

char *cJSON_Print(const cJSON *item)
{
    PrintBuf b;
    if (!pb_init(&b, 256)) return NULL;
    print_value(&b, item, 0);
    return b.buf; /* caller must free() */
}

/* ── Accessors ───────────────────────────────────────────────────────── */

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    if (!object || !string) return NULL;
    cJSON *c = object->child;
    while (c && strcmp(c->string, string) != 0)
        c = c->next;
    return c;
}

int cJSON_GetArraySize(const cJSON *array)
{
    int n = 0;
    cJSON *c = array ? array->child : NULL;
    while (c) { n++; c = c->next; }
    return n;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index)
{
    cJSON *c = array ? array->child : NULL;
    while (c && index-- > 0) c = c->next;
    return c;
}

/* ── Constructors ────────────────────────────────────────────────────── */

cJSON *cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Object;
    return item;
}

cJSON *cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Array;
    return item;
}

cJSON *cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_String;
        item->valuestring = cJSON_strdup(string ? string : "");
    }
    return item;
}

cJSON *cJSON_CreateNumber(double num)
{
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint    = (int)num;
    }
    return item;
}

cJSON *cJSON_CreateBool(int b)
{
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = b ? cJSON_True : cJSON_False;
        item->valueint = b ? 1 : 0;
    }
    return item;
}

cJSON *cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_NULL;
    return item;
}

/* ── Container modifiers ──────────────────────────────────────────────── */

static void suffix_object(cJSON *prev, cJSON *item)
{
    prev->next = item;
    item->prev = prev;
}

static cJSON *get_last(cJSON *first)
{
    while (first && first->next) first = first->next;
    return first;
}

void cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    if (!array || !item) return;
    cJSON *last = get_last(array->child);
    if (!last) { array->child = item; }
    else        { suffix_object(last, item); }
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    if (!object || !item) return;
    if (item->string) free(item->string);
    item->string = cJSON_strdup(string);
    cJSON_AddItemToArray(object, item);
}

cJSON *cJSON_AddStringToObject(cJSON *object, const char *name, const char *string)
{
    cJSON *item = cJSON_CreateString(string);
    if (item) cJSON_AddItemToObject(object, name, item);
    return item;
}

cJSON *cJSON_AddNumberToObject(cJSON *object, const char *name, double number)
{
    cJSON *item = cJSON_CreateNumber(number);
    if (item) cJSON_AddItemToObject(object, name, item);
    return item;
}

cJSON *cJSON_AddBoolToObject(cJSON *object, const char *name, int b)
{
    cJSON *item = cJSON_CreateBool(b);
    if (item) cJSON_AddItemToObject(object, name, item);
    return item;
}

void cJSON_SetNumberValue(cJSON *object, double number)
{
    if (!object) return;
    object->valuedouble = number;
    object->valueint    = (int)number;
}
