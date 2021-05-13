/*
 * Deathstation 9000 file system recorder.
 *
 * This is a trivial passthrough filesystem using the high level FUSE API, that
 * just logs all changes for analysis and replay.  This program is standalone
 * (doesn't share code with the replayer), because it's so simple.  The output
 * that it generates needs to be kept in sync with the stream reading code in
 * operation.cpp.
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#define HAVE_POSIX_FALLOCATE
#define DSFS_MAX_PATH 256

#include <fuse/fuse.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>

#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

static std::fstream log_file;
static const char *workdir_path;

static int
dsfs_remap(char *output, const char *path)
{
	assert(path[0] == '/');
	if (snprintf(output, DSFS_MAX_PATH, "%s%s", workdir_path, path) >= DSFS_MAX_PATH)
		return 0;
	return 1;
}

static void
dsfs_log_begin(const char *type)
{
	log_file << '(' << type;
}

static void
dsfs_log_buffer(const char *buffer, std::size_t size)
{
	const char *hex = "0123456789abcdef";

	log_file << " \"";
	for (std::size_t i = 0; i < size; ++i) {
		char c = buffer[i];
		if (c == '\\')
			log_file << "\\\\";
		else if (c == '"')
			log_file << "\\\"";
		else if (c == '\n')
			log_file << "\\n";
		else if (c >= 32 && c <= 126)
			log_file << c;
		else
			log_file << "\\x" << hex[((unsigned char) c) >> 4] << hex[c & 0xf];
	}
	log_file << '\"';
}

static void
dsfs_log_string(const char *value)
{
	dsfs_log_buffer(value, std::strlen(value));
}

template <typename T>
void
dsfs_log_number(T value)
{
	log_file << " " << value;
}

static void
dsfs_log_end()
{
	log_file << ")" << std::endl;
}

extern "C" {

static int
dsfs_getattr(const char *path, struct stat *stbuf)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = lstat(remapped, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int
dsfs_access(const char *path, int mask)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = access(remapped, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int
dsfs_readlink(const char *path, char *buf, std::size_t size)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = readlink(remapped, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

static int
dsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	DIR *dp;
	struct dirent *de;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	dp = opendir(remapped);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);

	return 0;
}

static int
dsfs_mkdir(const char *path, mode_t mode)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = mkdir(remapped, mode);
	if (res == -1)
		return -errno;

	dsfs_log_begin("mkdir");
	dsfs_log_string(path);
	dsfs_log_number(mode);
	dsfs_log_end();

	return 0;
}

static int
dsfs_unlink(const char *path)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = unlink(remapped);
	if (res == -1)
		return -errno;

	dsfs_log_begin("unlink");
	dsfs_log_string(path);
	dsfs_log_end();

	return 0;
}

static int
dsfs_rmdir(const char *path)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = rmdir(remapped);
	if (res == -1)
		return -errno;

	dsfs_log_begin("rmdir");
	dsfs_log_string(path);
	dsfs_log_end();

	return 0;
}

static int
dsfs_symlink(const char *from, const char *to)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, to))
		return -ENAMETOOLONG;

	res = symlink(from, remapped);
	if (res == -1)
		return -errno;

	dsfs_log_begin("symlink");
	dsfs_log_string(from);
	dsfs_log_string(to);
	dsfs_log_end();

	return 0;
}

static int
dsfs_rename(const char *from, const char *to)
{
	char remapped_from[DSFS_MAX_PATH];
	char remapped_to[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped_from, from))
		return -ENAMETOOLONG;
	if (!dsfs_remap(remapped_to, to))
		return -ENAMETOOLONG;

	res = rename(remapped_from, remapped_to);
	if (res == -1)
		return -errno;

	dsfs_log_begin("rename");
	dsfs_log_string(from);
	dsfs_log_string(to);
	dsfs_log_end();

	return 0;
}

static int
dsfs_link(const char *from, const char *to)
{
	char remapped_from[DSFS_MAX_PATH];
	char remapped_to[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped_from, from))
		return -ENAMETOOLONG;
	if (!dsfs_remap(remapped_to, to))
		return -ENAMETOOLONG;

	res = link(remapped_from, remapped_to);
	if (res == -1)
		return -errno;

	dsfs_log_begin("link");
	dsfs_log_string(from);
	dsfs_log_string(to);
	dsfs_log_end();

	return 0;
}

static int
dsfs_chmod(const char *path, mode_t mode)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = chmod(remapped, mode);
	if (res == -1)
		return -errno;

	dsfs_log_begin("chmod");
	dsfs_log_string(path);
	dsfs_log_number(mode);
	dsfs_log_end();

	return 0;
}

static int
dsfs_chown(const char *path, uid_t uid, gid_t gid)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = lchown(remapped, uid, gid);
	if (res == -1)
		return -errno;

	dsfs_log_begin("chown");
	dsfs_log_string(path);
	dsfs_log_number(uid);
	dsfs_log_number(gid);
	dsfs_log_end();

	return 0;
}

static int
dsfs_truncate(const char *path, off_t size)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = truncate(remapped, size);
	if (res == -1)
		return -errno;

	dsfs_log_begin("truncate");
	dsfs_log_string(path);
	dsfs_log_number(size);
	dsfs_log_end();

	return 0;
}

static int
dsfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	if (fi != NULL)
		res = ftruncate(fi->fh, size);
	else
		res = truncate(remapped, size);
	if (res == -1)
		return -errno;

	dsfs_log_begin("ftruncate");
	dsfs_log_string(path);
	dsfs_log_number(size);
	dsfs_log_number(fi ? fi->fh : -1);
	dsfs_log_end();

	return 0;
}


static int
dsfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = open(remapped, fi->flags, mode);
	if (res == -1)
		return -errno;

	dsfs_log_begin("create");
	dsfs_log_string(path);
	dsfs_log_number(fi->flags);
	dsfs_log_number(mode);
	dsfs_log_number(res);
	dsfs_log_end();

	fi->fh = res;

	return 0;
}

static int
dsfs_open(const char *path, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = open(remapped, fi->flags);
	if (res == -1)
		return -errno;

	dsfs_log_begin("open");
	dsfs_log_string(path);
	dsfs_log_number(fi->flags);
	dsfs_log_number(res);
	dsfs_log_end();

	fi->fh = res;

	return 0;
}

static int
dsfs_read(const char *path, char *buf, std::size_t size, off_t offset,
		  struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int fd;
	int res;


	if (fi == NULL) {
		if (!dsfs_remap(remapped, path))
			return -ENAMETOOLONG;
		fd = open(remapped, O_RDONLY);
	} else {
		fd = fi->fh;
	}
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if (fi == NULL)
		close(fd);

#if 0
	if (res >= 1) {
		dsfs_log_begin("read");
		dsfs_log_string(path);
		dsfs_log_buffer(buf, res);
		dsfs_log_number(offset);
		dsfs_log_number(fi ? fi->fh : -1);
		dsfs_log_end();
	}
#endif

	return res;
}

static int
dsfs_write(const char *path, const char *buf, std::size_t size,
		   off_t offset, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int fd;
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	if (fi == NULL)
		fd = open(remapped, O_WRONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if (fi == NULL)
		close(fd);

	if (res >= 1) {
		dsfs_log_begin("write");
		dsfs_log_string(path);
		dsfs_log_buffer(buf, res);
		dsfs_log_number(offset);
		dsfs_log_number(fi ? fi->fh : -1);
		dsfs_log_end();
	}

	return res;
}

static int
dsfs_statfs(const char *path, struct statvfs *stbuf)
{
	char remapped[DSFS_MAX_PATH];
	int res;

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	res = statvfs(remapped, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int
dsfs_release(const char *path, struct fuse_file_info *fi)
{
	close(fi->fh);

	dsfs_log_begin("release");
	dsfs_log_number(fi->fh);
	dsfs_log_end();

	return 0;
}

static int
dsfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	dsfs_log_begin("fsync");
	dsfs_log_string(path);
	dsfs_log_number(isdatasync);
	dsfs_log_number(fi ? fi->fh : -1);
	dsfs_log_end();

	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int
dsfs_fallocate(const char *path, int mode,
			   off_t offset, off_t length, struct fuse_file_info *fi)
{
	char remapped[DSFS_MAX_PATH];
	int fd;
	int res;

	if (mode)
		return -EOPNOTSUPP;

	if (fi == NULL) {
		if (!dsfs_remap(remapped, path))
			return -ENAMETOOLONG;
		fd = open(remapped, O_WRONLY);
	} else {
		fd = fi->fh;
	}
	
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	if (fi == NULL)
		close(fd);

	return res;
}
#endif

static int
dsfs_utimens(const char *path, const struct timespec tv[2])
{
	char remapped[DSFS_MAX_PATH];

	if (!dsfs_remap(remapped, path))
		return -ENAMETOOLONG;

	if (utimensat(AT_FDCWD, remapped, tv, 0) < 0)
		return -errno;

	dsfs_log_begin("utimens");
	dsfs_log_string(path);
	dsfs_log_number(tv[0].tv_sec);
	dsfs_log_number(tv[0].tv_nsec);
	dsfs_log_number(tv[1].tv_sec);
	dsfs_log_number(tv[1].tv_nsec);
	dsfs_log_end();

	return 0;
}

} // extern "C"

static struct fuse_operations dsfs_operations = {
	.getattr		= dsfs_getattr,
	.readlink		= dsfs_readlink,
	.mkdir			= dsfs_mkdir,
	.unlink			= dsfs_unlink,
	.rmdir			= dsfs_rmdir,
	.symlink		= dsfs_symlink,
	.rename			= dsfs_rename,
	.link			= dsfs_link,
	.chmod			= dsfs_chmod,
	.chown			= dsfs_chown,
	.truncate		= dsfs_truncate,
	.open			= dsfs_open,
	.read			= dsfs_read,
	.write			= dsfs_write,
	.statfs			= dsfs_statfs,
	.release		= dsfs_release,
	.fsync			= dsfs_fsync,
	.readdir		= dsfs_readdir,
	.access			= dsfs_access,
	.create			= dsfs_create,
	.ftruncate		= dsfs_ftruncate,
	.utimens		= dsfs_utimens,
	.fallocate		= dsfs_fallocate
};

int
main(int argc, char *argv[])
{
	char *fuse_argv[3];
	char serial_please[] = "-s";

	if (argc != 4) {
		std::cerr << argv[0] << " mount_point underlying_dir log_file\n";
		return EXIT_FAILURE;
	}

	log_file.open(argv[3], std::fstream::out);
	if (!log_file) {
		std::cerr << "can't open log file" << std::endl;
		return EXIT_FAILURE;
	}

	fuse_argv[0] = argv[0];
	fuse_argv[1] = serial_please;
	fuse_argv[2] = argv[1];
	workdir_path = argv[2];

	return fuse_main(3, fuse_argv, &dsfs_operations, NULL);
}
