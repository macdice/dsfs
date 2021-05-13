#ifndef DSFS_OPERATION_HPP
#define DSFS_OPERATION_HPP

#include <string>

/*
 * Files are referenced in the log by "handles" (these were the file
 * descriptor number used in by dsfs_record).
 */
typedef int file_handle_id_t;

/*
 * One operation that was logged by dsfs_record, has been read from
 * the log file, and is ready to be replayed.
 */
struct operation {
	enum op_type {
		OP_MKDIR,
		OP_UNLINK,
		OP_RMDIR,
		OP_SYMLINK,
		OP_RENAME,
		OP_LINK,
		OP_CHMOD,
		OP_CHOWN,
		OP_TRUNCATE,
		OP_FTRUNCATE,
		OP_CREATE,
		OP_OPEN,
		OP_WRITE,
		OP_RELEASE,
		OP_FSYNC,
		OP_UTIMENS
	} op;
	std::string path;
	std::string path2;
	std::string data;
	int uid;
	int gid;
	int mode;
	int flags;
	off_t offset;
	size_t size;
	int datasync;
	struct timespec utime[2];

	/*
	 * The file handle used during recording.  This was the file
	 * descriptor in the fsfs_record process, but we'll avoid
	 * confusion by not referring to it as an fd or similar during
	 * replay (we have our own file descriptors to worry about).
	 */
	file_handle_id_t file_handle_id;
};

std::string
stringify(operation::op_type op);

std::istream& operator>>(std::istream& stream, operation& out);

#endif
