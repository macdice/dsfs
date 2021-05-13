#ifndef INODE_HPP
#define INODE_HPP

#include <cstddef>

#include <sys/types.h>

struct inode {
	virtual ~inode() {}
	virtual void write(int fd, const char *data, std::size_t size, off_t offset) = 0;
	virtual void truncate(int fd, std::size_t size) = 0;
	virtual void synchronize(int fd) = 0;
	virtual void lose_power() = 0;
};

#endif
