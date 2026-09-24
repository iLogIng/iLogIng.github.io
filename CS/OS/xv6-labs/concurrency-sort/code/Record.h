#ifndef __RECORD_H__
#define __RECORD_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INT_TO_PTR(x) ((void *)(uintptr_t)(x))

typedef uint32_t RecordKey;

#define RECORD_SIZE 100
#define RECORD_KEY_SIZE sizeof(RecordKey)
#define RECORD_VALUE_SIZE (RECORD_SIZE - RECORD_KEY_SIZE)

typedef union {
    struct {
        RecordKey key;
        char value[RECORD_VALUE_SIZE];
    } fields;
    const char bytes[RECORD_SIZE];
} Record;

static inline
void set_Record_value(Record *record, const char *value) {
    strncpy(record->fields.value, value, RECORD_VALUE_SIZE);
}

#endif
