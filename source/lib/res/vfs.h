// virtual file system - transparent access to files in archives;
// allows multiple mount points
//
// Copyright (c) 2004 Jan Wassenberg
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// Contact info:
//   Jan.Wassenberg@stud.uni-karlsruhe.de
//   http://www.stud.uni-karlsruhe.de/~urkt/

#ifndef __VFS_H__
#define __VFS_H__

#include "handle.h"	// Handle def
#include "posix.h"	// struct stat
#include "file.h"	// file open flags (renamed)


//
// VFS tree
//

// the VFS doesn't require this length restriction - VFS internal storage
// is not fixed-length. the purpose here is to give an indication of how
// large fixed-size user buffers should be. length includes trailing '\0'.
#define VFS_MAX_PATH 256


// VFS paths are of the form: "(dir/)*file?"
// in English: '/' as path separator; trailing '/' required for dir names;
// no leading '/', since "" is the root dir.

enum VfsMountFlags
{
	// the directory being mounted (but not its subdirs! see impl) will be
	// searched for archives, and their contents added.
	// use only if necessary, since this is slow (we need to check if
	// each file is an archive, which entails reading the header).
	VFS_MOUNT_ARCHIVES = 1,

	// when mounting a directory, all directories beneath it are
	// added recursively as well.
	VFS_MOUNT_RECURSIVE = 2
};

// mount <p_real_dir> into the VFS at <vfs_mount_point>,
//   which is created if it does not yet exist.
// files in that directory override the previous VFS contents if
//   <pri>(ority) is not lower.
// all archives in <p_real_dir> are also mounted, in alphabetical order.
//
// flags determines extra actions to perform; see VfsMountFlags.
//
// p_real_dir = "." or "./" isn't allowed - see implementation for rationale.
extern int vfs_mount(const char* v_mount_point, const char* p_real_dir, int flags = 0, uint pri = 0);

// rebuild the VFS, i.e. re-mount everything. open files are not affected.
// necessary after loose files or directories change, so that the VFS
// "notices" the changes and updates file locations. res calls this after
// dir_watch reports changes; can also be called from the console after a
// rebuild command. there is no provision for updating single VFS dirs -
// it's not worth the trouble.
extern int vfs_rebuild(void);

// unmount a previously mounted item, and rebuild the VFS afterwards.
extern int vfs_unmount(const char* name);

// if <path> or its ancestors are mounted,
// return a VFS path that accesses it.
// used when receiving paths from external code.
extern int vfs_make_vfs_path(const char* path, char* vfs_path);

// write a representation of the VFS tree to stdout.
extern void vfs_display(void);


//
// directory entry
//

// information about a directory entry, returned by vfs_next_dirent.
struct vfsDirEnt
{
	// name of directory entry - does not include path.
	// valid until the directory handle is closed. must not be modified!
	// rationale for pointer and invalidation: see vfs_next_dirent.
	const char* name;
};

// open the directory for reading its entries via vfs_next_dirent.
// <v_dir> need not end in '/'; we add it if not present.
// directory contents are cached here; subsequent changes to the dir
// are not returned by this handle. rationale: see VDir definition.
extern Handle vfs_open_dir(const char* dir);

// close the handle to a directory.
// all vfsDirEnt.name strings are now invalid.
extern int vfs_close_dir(Handle& hd);

// retrieve the next dir entry (in alphabetical order) matching <filter>.
// return 0 on success, ERR_DIR_END if no matching entry was found,
// or a negative error code on failure.
// filter values:
// - 0: any file;
// - "/": any subdirectory
// - anything else: pattern for name (may include '?' and '*' wildcards)
extern int vfs_next_dirent(Handle hd, vfsDirEnt* ent, const char* filter);


//
// file
//

// return actual path to the specified file:
// "<real_directory>/fn" or "<archive_name>/fn".
extern int vfs_realpath(const char* fn, char* realpath);

// does the specified file exist? return false on error.
// useful because a "file not found" warning is not raised, unlike vfs_stat.
extern bool vfs_exists(const char* fn);

// get file status (size, mtime). output param is zeroed on error.
extern int vfs_stat(const char* fn, struct stat*);

// return the size of an already opened file, or a negative error code.
extern ssize_t vfs_size(Handle hf);

// open the file for synchronous or asynchronous IO. write access is
// requested via FILE_WRITE flag, and is not possible for files in archives.
// flags defined in file.h
extern Handle vfs_open(const char* fn, uint flags = 0);

// close the handle to a file.
extern int vfs_close(Handle& h);


//
// asynchronous I/O
//

// low-level file routines - no caching or alignment.
// 

// begin transferring <size> bytes, starting at <ofs>. get result
// with vfs_wait_read; when no longer needed, free via vfs_discard_io.
extern Handle vfs_start_io(Handle hf, size_t size, void* buf);

// indicates if the given IO has completed.
// return value: 0 if pending, 1 if complete, < 0 on error.
extern int vfs_io_complete(Handle hio);

// wait until the transfer <hio> completes, and return its buffer.
// output parameters are zeroed on error.
extern int vfs_wait_io(Handle hio, void*& p, size_t& size);

// finished with transfer <hio> - free its buffer (returned by vfs_wait_read).
extern int vfs_discard_io(Handle& hio);


//
// synchronous I/O
//

// transfer the next <size> bytes to/from the given file.
// (read or write access was chosen at file-open time).
//
// if non-NULL, <cb> is called for each block transferred, passing <ctx>.
// it returns how much data was actually transferred, or a negative error
// code (in which case we abort the transfer and return that value).
// the callback mechanism is useful for user progress notification or
// processing data while waiting for the next I/O to complete
// (quasi-parallel, without the complexity of threads).
//
// p (value-return) indicates the buffer mode:
// - *p == 0: read into buffer we allocate; set *p.
//   caller should mem_free it when no longer needed.
// - *p != 0: read into or write into the buffer *p.
// - p == 0: only read into temp buffers. useful if the callback
//   is responsible for processing/copying the transferred blocks.
//   since only temp buffers can be added to the cache,
//   this is the preferred read method.
//
// return number of bytes transferred (see above), or a negative error code.
extern ssize_t vfs_io(Handle hf, size_t size, void** p, FileIOCB cb = 0, uintptr_t ctx = 0);


// convenience functions that replace vfs_open / vfs_io / vfs_close:

// load the entire file <fn> into memory; return a memory handle to the
// buffer and its address/size. output parameters are zeroed on failure.
// in addition to the regular file cache, the entire buffer is kept in memory
// if flags & FILE_CACHE.
extern Handle vfs_load(const char* fn, void*& p, size_t& size, uint flags = 0);

extern int vfs_store(const char* fn, void* p, size_t size, uint flags = 0);


//
// memory mapping
//

// useful for files that are too large to be loaded into memory,
// or if only (non-sequential) portions of a file are needed at a time.
//
// this is of course only possible for uncompressed files - compressed files
// would have to be inflated sequentially, which defeats the point of mapping.


// map the entire (uncompressed!) file <hf> into memory. if currently
// already mapped, return the previous mapping (reference-counted).
// output parameters are zeroed on failure.
//
// the mapping will be removed (if still open) when its file is closed.
// however, map/unmap calls should still be paired so that the mapping
// may be removed when no longer needed.
extern int vfs_map(Handle hf, uint flags, void*& p, size_t& size);

// decrement the reference count for the mapping belonging to file <f>.
// fail if there are no references; remove the mapping if the count reaches 0.
//
// the mapping will be removed (if still open) when its file is closed.
// however, map/unmap calls should still be paired so that the mapping
// may be removed when no longer needed.
extern int vfs_unmap(Handle hf);



extern void vfs_enable_file_listing(bool want_enabled);

extern void vfs_shutdown(void);

#endif	// #ifndef __VFS_H__
