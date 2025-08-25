
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "s_util/err.h"
#include "s_util/hash.h"
#include "s_util/datetime.h"
#include <stdbool.h>

/*
 * Below lies a generic file system interface.
 *
 * NOTE: implementations of this interface are meant to assume sensible sequential usage.
 *
 * For example, we'd expect the user to never attempt to write to a file which has been deleted.
 * 
 * Protecting the user for funky error cases stemming from concurrent usage is meant to be
 * handled higher up the stack. NOT HERE.
 *
 * Also, while not really strictly referenced in this interface, it will be assumed that 
 * "." refers to "this" directory and ".." refers to the parent directory.
 * Tests will assume all directories have a "." entry and that all non-root directories have
 * a ".." entry.
 */

/**
 * Maximum length of a filename. (Not including NT)
 */
#define FS_MAX_FILENAME_LEN (127U)

/**
 * Maximum length of a path. (Including "/"'s, Not including NT)
 */
#define FS_MAX_PATH_LEN (4096 - 1)

/*
 * NOTE: The below two functions just check characters and lengths are expected.
 * A filename/path being "valid" has nothing to do with whether or not said
 * filename/path actually points to an existing location.
 * 
 *  For a path to be valid:
 * The length of the entire path string must not exceed FS_MAX_PATH_LEN.
 * A path is just a list of valid filenames separated by SINGLE "/"'s
 * A path can optionally both end and start with "/"'s
 * "" is an invalid path. "/" is a valid path.
 */

/**
 * Returns true if the given filename is valid. False otherwise.
 */
bool is_valid_filename(const char *fn);

/**
 * Returns true if the given path is valid. False otherwise.
 */
bool is_valid_path(const char *path);

/**
 * Given a VALID OR EMPTY path, write it's next filename part into `*dest`.
 * `dest` should point to a buffer with length at least (FS_MAX_FILENAME_LEN + 1).
 *
 * Returns the index into path of the first character of the rest path.
 *
 * For example `next_filename("/a/b", d)` would write "a" (with NT) into `dest` and
 * return 2. If the path has no filenames (i.e. an empty string or just "/"), 
 * '\0' is written to `dest` and 0 is returned.
 *
 * Undefined behavior if the path is Non-empty and invalid.
 */
size_t next_filename(const char *path, char *dest);

/**
 * A Node key is an immutable piece of data which can be used to efficiently reference a 
 * file or directory.
 * 
 * Values of this type should always be passed around via pointers. (With actual data likely
 * living on some sort of heap)
 */
typedef const void *fs_node_key_t;

/**
 * Information which is retrieveable for any node (i.e. files or directories alike)
 */
typedef struct _fs_node_info_t {
    /**
     * Datetime of when the node was created.
     *
     * OPTIONAL
     */
    fernos_datetime_t creation_dt;

    /**
     * Datetime of when the node was last edited.
     *
     * For a file, this is when the file was last written to.
     * For a directory, this is the last time of file within the directory was created or deleted.
     *
     * OPTIONAL
     */
    fernos_datetime_t last_edited_dt;

    /**
     * True iff this points to a directory node.
     *
     * REQUIRED
     */
    bool is_dir;

    /**
     * For a file, this is the length of the file in bytes.
     * For a directory, this is the number of entries within the directory.
     *
     * NOTE: For directories, this does NOT its relative entries. ("." and "..")
     *
     * REQUIRED
     */
    size_t len;
} fs_node_info_t;

typedef struct _file_sys_impl_t file_sys_impl_t;

typedef struct _file_sys_t {
    const file_sys_impl_t * const impl;
} file_sys_t;

struct _file_sys_impl_t {
    // OPTIONAL
    fernos_error_t (*fs_flush)(file_sys_t *fs, fs_node_key_t key);

    void (*delete_file_sys)(file_sys_t *fs);
    fernos_error_t (*fs_new_key)(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key);
    void (*fs_delete_key)(file_sys_t *fs, fs_node_key_t key);
    equator_ft (*fs_get_key_equator)(file_sys_t *fs);
    hasher_ft (*fs_get_key_hasher)(file_sys_t *fs);
    fernos_error_t (*fs_get_node_info)(file_sys_t *fs, fs_node_key_t key, fs_node_info_t *info);
    fernos_error_t (*fs_touch)(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key);
    fernos_error_t (*fs_mkdir)(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key);
    fernos_error_t (*fs_remove)(file_sys_t *fs, fs_node_key_t parent_dir, const char *name);
    fernos_error_t (*fs_get_child_name)(file_sys_t *fs, fs_node_key_t parent_dir, size_t index, char *name);
    fernos_error_t (*fs_read)(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, void *dest);
    fernos_error_t (*fs_write)(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, const void *src);
    fernos_error_t (*fs_resize)(file_sys_t *fs, fs_node_key_t file_key, size_t bytes);
};

/**
 * Flush! (What this means/does is up to the implementor)
 *
 * If `key` is NULL, this should flush the entire file system.
 * Otherwise, this should at least flush the file pointed to by `key`.
 */
static inline fernos_error_t fs_flush(file_sys_t *fs, fs_node_key_t key) {
    if (fs->impl->fs_flush) {
        return fs->impl->fs_flush(fs, key);
    }

    return FOS_SUCCESS;
}

/**
 * Delete the file system.
 *
 * As I want this to always succeed, this DOES NOT FLUSH. Make sure to always flush before deleting!
 */
static inline void delete_file_sys(file_sys_t *fs) {
    if (fs) {
        fs->impl->delete_file_sys(fs);
    }
}

/**
 * Get a new key which references the described file/directory.
 *
 * `cwd` will be the "current working directory" key. If `path` is relative, the node search will
 * start at this directory. 
 * If you know `path` is absolute, `cwd` can be NULL signifying no relative starting point.
 *
 * On success, FOS_SUCCESS is returned and the new key is written to `*key`.
 * If `path` does not point to an existing file, FOS_EMPTY is returned.
 * If `path` is relative and `cwd` is NULL, FOS_BAD_ARGS is returned.
 * If `cwd` is given, and doesn't point to a directory, FOS_STATE_MISMATCH is returned.
 * If the given `path` points to a file mid-path, FOS_STATE_MISMATCH is returned.
 */
static inline fernos_error_t fs_new_key(file_sys_t *fs, fs_node_key_t cwd, 
        const char *path, fs_node_key_t *key) {
    return fs->impl->fs_new_key(fs, cwd, path, key);
}

/**
 * Delete a key which was allocated using the `fs_new_key` function.
 *
 * This should always succeed!
 */
static inline void fs_delete_key(file_sys_t *fs, fs_node_key_t key) {
    if (key) {
        fs->impl->fs_delete_key(fs, key);
    }
}

/**
 * Get a function pointer which can be used to tell if two keys are semantically equal!
 */
static inline equator_ft fs_get_key_equator(file_sys_t *fs) {
    return fs->impl->fs_get_key_equator(fs);
}

/**
 * Get a function pointer which can be used to calculate a key's hash value.
 */
static inline hasher_ft fs_get_key_hasher(file_sys_t *fs) {
    return fs->impl->fs_get_key_hasher(fs);
}

/**
 * Retrieve information about a specific node.
 */
static inline fernos_error_t fs_get_node_info(file_sys_t *fs, fs_node_key_t key, fs_node_info_t *info) {
    return fs->impl->fs_get_node_info(fs, key, info);
}

/**
 * Create a new file within a directory.
 *
 * Returns FOS_BAD_ARGS if `name` is not a valid filename.
 * Returns FOS_STATE_MISMATCH if `parent_dir` is not a key to a directory.
 * Returns FOS_IN_USE if `name` already appears in `parent_dir`.
 *
 * On success, FOS_SUCCESS is returned. If `key` is non-NULL, a key for the new file will be created
 * and stored at `*key`.
 */
static inline fernos_error_t fs_touch(file_sys_t *fs, fs_node_key_t parent_dir, 
        const char *name, fs_node_key_t *key) {
    return fs->impl->fs_touch(fs, parent_dir, name, key);
}

/**
 * Create a subdiretory.
 *
 * Returns FOS_BAD_ARGS if `name` is not a valid filename.
 * Returns FOS_STATE_MISMATCH if `parent_dir` is not a key to a directory.
 * Returns FOS_IN_USE if `name` already appears in `parent_dir`.
 *
 * On success, FOS_SUCCESS is returned. If `key` is non-NULL, a key for the new subdirecotry will 
 * be created and stored at `*key`.
 *
 * NOTE: The created directory should have entries "." and "..".
 */
static inline fernos_error_t fs_mkdir(file_sys_t *fs, fs_node_key_t parent_dir, 
        const char *name, fs_node_key_t *key) {
    return fs->impl->fs_mkdir(fs, parent_dir, name, key);
}

/**
 * Remove a file or subdirectory.
 *
 * Returns FOS_STATE_MISMATCH if `parent_dir` is not a key to a directory.
 * Returns FOS_INVALID_INDEX if `name` cannot be found within the given directory.
 * Returns FOS_IN_USE if the node being deleted is a subdirectory and has 
 * entries other than "." and "..".
 *
 * On success, FOS_SUCCESS is returned and the specified node is deleted from the file system.
 */
static inline fernos_error_t fs_remove(file_sys_t *fs, fs_node_key_t parent_dir, const char *name) {
    return fs->impl->fs_remove(fs, parent_dir, name);
}

/**
 * Retrieve the name of a child node within a directory.
 *
 * `name` must have size at least `FS_MAX_FILENAME_LEN + 1`.
 *
 * Returns FOS_STATE_MISMATCH if `parent_dir` is not a key to a directory.
 * Returns FOS_INVALID_INDEX if `index` >= the number of child nodes within the directory.
 *
 * On sucess, FOS_SUCCESS is returned, the `index`'th child's name is written out to `name`.
 *
 * NOTE: Special directory entries "." and ".." should NOT be returned by this function.
 */
static inline fernos_error_t fs_get_child_name(file_sys_t *fs, fs_node_key_t parent_dir, 
        size_t index, char *name) {
    return fs->impl->fs_get_child_name(fs, parent_dir, index, name);
}

/**
 * Read bytes from a file.
 *
 * `dest` must have size at least `bytes`.
 *
 * Returns FOS_STATE_MISTMATCH if `file_key` points to a directory.
 * Returns FOS_INVALID_RANGE if `offset` or `offset + bytes` overshoot the end of the file.
 *
 * On success, FOS_SUCCESS is returned, exactly `bytes` bytes will be written to dest.
 */
static inline fernos_error_t fs_read(file_sys_t *fs, fs_node_key_t file_key, size_t offset, 
        size_t bytes, void *dest) {
    return fs->impl->fs_read(fs, file_key, offset, bytes, dest);
}

/**
 * Write bytes to a file.
 *
 * `src` must have size at least `bytes`.
 *
 * Returns FOS_STATE_MISTMATCH if `file_key` points to a directory.
 * Returns FOS_INVALID_RANGE if `offset` or `offset + bytes` overshoot the end of the file.
 *
 * On success, FOS_SUCCESS is returned, exactly `bytes` bytes will be written to the given file
 * from `src` starting at offset `offset` within the file.
 */
static inline fernos_error_t fs_write(file_sys_t *fs, fs_node_key_t file_key, size_t offset, 
        size_t bytes, const void *src) {
    return fs->impl->fs_write(fs, file_key, offset, bytes, src);
}

/**
 * Resize a file to exactly `bytes` bytes.
 *
 * If `bytes` is 0, the file isn't actually deleted, it just is given a size of 0. This is 
 * to encourage correct handling of node keys. That is, all node keys for a file should be deleted
 * before a file is deleted. 
 *
 * On success, FOS_SUCCESS is returned, the given file will have exact size `bytes`.
 */
static inline fernos_error_t fs_resize(file_sys_t *fs, fs_node_key_t file_key, size_t bytes) {
    return fs->impl->fs_resize(fs, file_key, bytes);
}
