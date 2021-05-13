#include "replayer.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Compute the parent directory.
 */
static void
get_parent(std::string& parent, const std::string& path)
{
	if (path[0] != '/')
		throw std::runtime_error("get_parent -- unexpected relative path " + path);
	parent = path;
	while (parent.size() > 0 && parent[parent.size() - 1] != '/')
		parent.resize(parent.size() - 1);
}

#include <iostream>
/*
 * Given a remapped path, get the inode number so that we can look it
 * up in our inode table.
 */
static ino_t
get_inode_number(const std::string& path)
{
	struct stat stat_data;
	int fd;
	int rc;

	fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		std::string error = std::strerror(errno);
		throw std::runtime_error("could not open directory " + path + ": " + error);
	}
	rc = ::fstat(fd, &stat_data);
	if (rc < 0) {
		std::string error = std::strerror(errno);
		::close(fd);
		throw std::runtime_error("could not stat directory " + path + ": " + error);
	}
	::close(fd);

	return stat_data.st_ino;
}

replayer::replayer(const std::string& target_path,
				   off_t sector_size,
				   file_writeback_mode file_mode) :
	target_path(target_path),
	sector_size(sector_size),
	file_mode(file_mode)
{
}

void
replayer::remap(std::string& remapped, const std::string& path)
{
	remapped = target_path;
	assert(path[0] == '/');
	remapped.append(path);
}

void
replayer::open_file_handle(const std::string& path,
						   int file_handle_id,
						   int fd)
{
	struct stat stat_data;
	bool is_dir;

	// Get the inode number and type in the target directory.
	int rc = fstat(fd, &stat_data);
	if (rc < 0)
		throw std::runtime_error("could not stat file " + path);

	// What kind of inode is this?
	if (S_ISDIR(stat_data.st_mode)) {
		is_dir = true;
	} else if (S_ISREG(stat_data.st_mode)) {
		is_dir = false;
	} else if (S_ISLNK(stat_data.st_mode)) {
		throw std::runtime_error("unsupported: log opens symlink directly");
	} else {
		throw std::runtime_error("unsupported: log opens unsupported file type");
	}

	// Look up our inode object, and create it if needed
	std::unique_ptr<inode>& inode = inode_table[stat_data.st_ino];

	if (!inode) {
		if (is_dir)
			inode = std::make_unique<directory>(path);
		else
			inode = std::make_unique<file>(sector_size, file_mode);
	}

	// Sanity check that the inode hasn't changed type underneath us
	if (is_dir)
		assert(dynamic_cast<directory *>(inode.get()));
	else
		assert(dynamic_cast<file *>(inode.get()));

	// Record that we have this file_handle_id open
	if (file_handle_table.size() < std::size_t(file_handle_id) + 1)
		file_handle_table.resize(std::size_t(file_handle_id) + 1);
	if (file_handle_table[file_handle_id].inode)
		throw std::runtime_error("log opens the same file handle ID twice");
	file_handle_table[file_handle_id] = file_handlex(fd, inode.get());
}

void
replayer::close_file_handle(int file_handle_id)
{
	// Validate the file handle
	if (file_handle_id < 0 ||
		std::size_t(file_handle_id) >= file_handle_table.size() ||
		file_handle_table[file_handle_id].fd == -1)
		throw std::runtime_error("log closes unknown file handle ID");

	// This file handle table slot is now empty
	file_handle_table[file_handle_id].inode = NULL;
	file_handle_table[file_handle_id].fd = -1;
}

const file_handlex&
replayer::get_file_handle(const operation& op)
{
	if (op.file_handle_id >= 0) {
		// This operation claims to refer to a file handle that we
		// already opened earlier, by ID.
		if (std::size_t(op.file_handle_id) >= file_handle_table.size() ||
			file_handle_table[op.file_handle_id].fd == -1)
			throw std::runtime_error("log references unknown file handle ID");
		return file_handle_table[op.file_handle_id];
	} else {
		// We have to open a temporary fd, and then somehow close it
		// later...
		throw std::runtime_error("XXX -- unimplemented -- open temporary");
	}
}

void
replayer::lose_power()
{
	for (auto& [inode_number, inode] : inode_table)
		inode->lose_power();
}

directory&
replayer::get_directory(const std::string& path)
{
	std::string remapped;

	remap(remapped, path);

	auto& inode = inode_table[get_inode_number(remapped)];

	// Make a new one if we haven't heard of it before.
	if (!inode)
		inode = std::make_unique<directory>(path);

	if (auto result = dynamic_cast<directory *>(inode.get()))
		return *result;

	throw std::runtime_error("expected " + path + " to be a directory, but it's a file");
}

void
replayer::replay(const operation& op)
{
	int rc = 0;
	int fd;

	switch (op.op) {
	case operation::OP_MKDIR:
		remap(remapped, op.path);
		rc = ::mkdir(remapped.c_str(), op.mode);
		break;
	case operation::OP_UNLINK:
		remap(remapped, op.path);
		rc = ::unlink(remapped.c_str());
		break;
	case operation::OP_RMDIR:
		remap(remapped, op.path);
		rc = ::rmdir(remapped.c_str());
		break;
	case operation::OP_SYMLINK:
		remap(remapped, op.path2);
		rc = ::symlink(op.path.c_str(), remapped.c_str());
		// XXX forget symnlink if parent dir not synced!
		break;
	case operation::OP_RENAME:
		remap(remapped, op.path);
		remap(remapped2, op.path2);
		rc = ::rename(remapped.c_str(), remapped2.c_str());
		// If the parent directory is the same (we just renamed, we
		// didn't move) then we might potentially undo it on crash.
		// If it's a move, it's not yet clear how to do that, so we'll
		// leave it committed.
		get_parent(parent, op.path);
		get_parent(parent2, op.path2);
		if (parent == parent2)
			get_directory(parent).rename(op.path, op.path2);
		break;
	case operation::OP_LINK:
		remap(remapped, op.path);
		remap(remapped2, op.path2);
		rc = ::link(remapped.c_str(), remapped2.c_str());
		break;
	case operation::OP_CHMOD:
		remap(remapped, op.path);
		rc = ::chmod(remapped.c_str(), op.mode);
		break;
	case operation::OP_CHOWN:
		remap(remapped, op.path);
		rc = ::chown(remapped.c_str(), op.uid, op.gid);
		break;
	case operation::OP_TRUNCATE:
		remap(remapped, op.path);
		rc = ::truncate(remapped.c_str(), op.size);
		break;
	case operation::OP_FTRUNCATE:
		// XXX simulate delayed commit of truncate!
		rc = ::ftruncate(get_file_handle(op).fd, op.size);
		break;
	case operation::OP_CREATE:
		remap(remapped, op.path);
		fd = ::open(remapped.c_str(), O_RDWR | O_CREAT, op.mode);
		if (fd < 0) {
			std::string error = std::strerror(errno);
			throw std::runtime_error("could not create file " + remapped + ": " + error);
		}
		open_file_handle(op.path, op.file_handle_id, fd);
		break;
	case operation::OP_OPEN:
		remap(remapped, op.path);
		fd = ::open(remapped.c_str(), O_RDWR);
		if (fd < 0) {
			std::string error = std::strerror(errno);
			throw std::runtime_error("could not open file " + remapped + ": " + error);
		}
		open_file_handle(op.path, op.file_handle_id, fd);
		break;
	case operation::OP_WRITE:
		{
			auto& fh = get_file_handle(op);

			fh.inode->write(fh.fd,
							op.data.data(),
							op.data.size(),
							op.offset);
		}
		break;
	case operation::OP_RELEASE:
		close_file_handle(op.file_handle_id);
		break;
	case operation::OP_UTIMENS:
		remap(remapped, op.path);
		rc = ::utimensat(AT_FDCWD, remapped.c_str(), op.utime, 0);
		break;
	case operation::OP_FSYNC:
		{
			auto& fh = get_file_handle(op);
			fh.inode->synchronize(fh.fd);
		}
		break;
	default:
		throw std::runtime_error("unknown operation");
	}
	if (rc < 0) {
		std::string error = std::strerror(errno);
		throw std::runtime_error(stringify(op.op) + " failed: " + error);
	}
}
