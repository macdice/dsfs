#ifndef FILE_HPP
#define FILE_HPP

#include "inode.hpp"

#include <cstddef>
#include <map>
#include <vector>

#include <sys/types.h>

enum file_writeback_mode {
	FILE_WRITEBACK_ALL,
	FILE_WRITEBACK_NONE,
	FILE_WRITEBACK_ODD,
	FILE_WRITEBACK_EVEN,
	FILE_WRITEBACK_RANDOM
};

struct file : inode {
	file(std::size_t sector_size, file_writeback_mode writeback_mode);
	void write(int fd, const char *data, std::size_t size, off_t offset) override;
	void truncate(int fd, std::size_t size) override;
	void synchronize(int fd) override;
	void lose_power() override;

private:
	bool writeback_p(int sector_number);

	std::size_t size;
	std::size_t sector_size;
	int writeback_mode;
	std::map<off_t, std::vector<char>> unwritten_sectors;
};

#endif
