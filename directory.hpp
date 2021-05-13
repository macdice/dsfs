#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include "inode.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <sys/types.h>

struct directory_change {
	enum { UNLINK, LINK, RENAME } type;
	std::string name;
	std::string name2;
};

struct directory : inode {
	directory(const std::string& path) : path(path) {}

	void link(ino_t inode_number, const std::string& name);
	void unlink(const std::string& name);
	void rename(const std::string& old_name,
				const std::string& new_name);

	void write(int fd, const char *data, std::size_t size, off_t offset) override;
	void truncate(int fd, std::size_t size) override;
	void synchronize(int fd) override;
	void lose_power() override;

private:
	std::string path;
	std::vector<directory_change> undo_log;
};

#endif
