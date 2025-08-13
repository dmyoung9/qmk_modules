#pragma once

#include <stdbool.h>
#include <stdint.h>

// Forward declaration
typedef struct ts_blob ts_blob_t;

// Storage module API
void ts_storage_load(ts_blob_t *blob);
void ts_storage_save(const ts_blob_t *blob);
void ts_storage_mark_dirty(void);
void ts_storage_force_flush(void);
void ts_storage_task(void);  // Call periodically for auto-flush

// Storage status
bool ts_storage_is_dirty(void);
uint32_t ts_storage_get_last_flush_time(void);
