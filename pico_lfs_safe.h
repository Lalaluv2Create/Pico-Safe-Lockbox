#ifndef PICO_LFS_SAFE_H
#define PICO_LFS_SAFE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MUST be called on Core 1 once at the top of its entry function
// to register Core 1 as a lockout victim during flash erase/programming.
void safe_fs_core1_init(void);

// Mounts the partition (formats automatically if clean or unformatted)
bool safe_fs_init(void);

// Safe file operations (halts Core 1, disables interrupts during writes)
int  safe_fs_write(const char *filename, const void *data, size_t len);
int  safe_fs_read(const char *filename, void *buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // PICO_LFS_SAFE_H
