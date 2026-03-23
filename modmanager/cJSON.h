/*
 * cJSON — minimal JSON library (MIT License)
 *
 * Handles the subset used by AoQModManager config files:
 *   - Objects, arrays, strings, numbers (double), booleans, null
 *
 * Based on the cJSON public API by Dave Gamble (https://github.com/DaveGamble/cJSON).
 * This is a standalone re-implementation trimmed for Quest mod use.
 */
#ifndef CJSON_H
#define CJSON_H

#ifdef __cplusplus
extern "C" {
#endif

/* cJSON item types */
#define cJSON_Invalid  (0)
#define cJSON_False    (1 << 0)
#define cJSON_True     (1 << 1)
#define cJSON_NULL     (1 << 2)
#define cJSON_Number   (1 << 3)
#define cJSON_String   (1 << 4)
#define cJSON_Array    (1 << 5)
#define cJSON_Object   (1 << 6)

typedef struct cJSON {
    struct cJSON *next;       /* linked list of siblings */
    struct cJSON *prev;
    struct cJSON *child;      /* for Array / Object: first child */
    int    type;
    char  *valuestring;       /* for cJSON_String */
    int    valueint;          /* for cJSON_Number (integer shorthand) */
    double valuedouble;       /* for cJSON_Number */
    char  *string;            /* object key name */
} cJSON;

/* Parse a JSON string. Returns root item or NULL on error. Caller must cJSON_Delete(). */
cJSON *cJSON_Parse(const char *value);

/* Render a cJSON item to a string. Caller must free() the result. */
char  *cJSON_Print(const cJSON *item);

/* Delete a cJSON tree (frees all memory). */
void   cJSON_Delete(cJSON *c);

/* Object field access */
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);

/* Array helpers */
int    cJSON_GetArraySize(const cJSON *array);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);

/* Create items */
cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateBool(int b);
cJSON *cJSON_CreateNull(void);

/* Add items to object/array */
void   cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
void   cJSON_AddItemToArray(cJSON *array, cJSON *item);

/* Convenience constructors for adding to objects */
cJSON *cJSON_AddStringToObject(cJSON *object, const char *name, const char *string);
cJSON *cJSON_AddNumberToObject(cJSON *object, const char *name, double number);
cJSON *cJSON_AddBoolToObject(cJSON *object, const char *name, int b);

/* Modify existing items */
void   cJSON_SetNumberValue(cJSON *object, double number);

#ifdef __cplusplus
}
#endif

#endif /* CJSON_H */
