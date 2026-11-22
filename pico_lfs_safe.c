#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "lfs.h"
#include "pico_lfs_safe.h"

// Reserve the last 512 KB of flash for LittleFS.
// The Pico 2 W standard flash size is 4 MB (PICO_FLASH_SIZE_BYTES).
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

#define FS_SIZE         (512 * 1024)
#define FS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FS_SIZE)

static lfs_t lfs;

// --- Flash Driver Callbacks with Core Lockout Protection ---

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size) {
    // Reading flash over memory-mapped XIP does not modify flash,
    // so no core lockout is required.
    uint32_t flash_addr = XIP_BASE + FS_FLASH_OFFSET + (block * c->block_size) + off;
    memcpy(buffer, (const void *)flash_addr, size);
    return LFS_ERR_OK;
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size) {
    uint32_t offset = FS_FLASH_OFFSET + (block * c->block_size) + off;

    // 1. Pause Core 1 so it cannot fetch instructions from XIP flash
    multicore_lockout_start_blocking();

    // 2. Disable local interrupts on this core
    uint32_t ints = save_and_disable_interrupts();

    // 3. Program the flash page
    flash_range_program(offset, (const uint8_t *)buffer, size);

    // 4. Restore interrupts and resume Core 1
    restore_interrupts(ints);
    multicore_lockout_end_blocking();

    return LFS_ERR_OK;
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block) {
    uint32_t offset = FS_FLASH_OFFSET + (block * c->block_size);

    // 1. Pause Core 1
    multicore_lockout_start_blocking();

    // 2. Disable local interrupts on this core
    uint32_t ints = save_and_disable_interrupts();

    // 3. Erase the sector (4096 bytes)
    flash_range_erase(offset, FLASH_SECTOR_SIZE);

    // 4. Restore interrupts and resume Core 1
    restore_interrupts(ints);
    multicore_lockout_end_blocking();

    return LFS_ERR_OK;
}

static int lfs_flash_sync(const struct lfs_config *c) {
    (void)c;
    return LFS_ERR_OK;
}

// Buffers required by LittleFS
static uint8_t read_buffer[256];
static uint8_t prog_buffer[256];
static uint8_t lookahead_buffer[128];

static const struct lfs_config cfg = {
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = lfs_flash_sync,

    .read_size      = 1,
    .prog_size      = FLASH_PAGE_SIZE,     // 256 bytes
    .block_size     = FLASH_SECTOR_SIZE,   // 4096 bytes
    .block_count    = FS_SIZE / FLASH_SECTOR_SIZE,
    .cache_size     = 256,
    .lookahead_size = 128,
    .block_cycles   = 500,

    .read_buffer      = read_buffer,
    .prog_buffer      = prog_buffer,
    .lookahead_buffer = lookahead_buffer,
};

// --- Student API Functions ---

void safe_fs_core1_init(void) {
    // Configures Core 1 to yield when Core 0 initiates a lockout
    multicore_lockout_victim_init();
}

bool safe_fs_init(void) {
    int err = lfs_mount(&lfs, &cfg);
    if (err) {
        printf("[FS] Partition unformatted or corrupt. Formatting...\n");
        err = lfs_format(&lfs, &cfg);
        if (err) return false;
        err = lfs_mount(&lfs, &cfg);
        if (err) return false;
    }
    printf("[FS] LittleFS mounted successfully.\n");
    return true;
}

int safe_fs_write(const char *filename, const void *data, size_t len) {
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err < 0) return err;

    lfs_ssize_t written = lfs_file_write(&lfs, &file, data, len);
    lfs_file_close(&lfs, &file);

    return (written < 0) ? (int)written : 0;
}

int safe_fs_read(const char *filename, void *buffer, size_t max_len) {
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
    if (err < 0) return err;

    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, buffer, max_len);
    lfs_file_close(&lfs, &file);

    return (int)bytes_read; // Returns bytes read or negative error code
}
