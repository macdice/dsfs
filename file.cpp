#include "file.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <unistd.h>

/*
 * Like pwrite(), but with retry and exceptions.
 */
static void
write_all(int fd, const char *data, std::size_t size, off_t offset)
{
	std::size_t written_so_far = 0;

	do {
		ssize_t written = ::pwrite(fd,
								   data + written_so_far,
								   size - written_so_far,
								   offset + written_so_far);
		if (written < 0) {
			std::string error = std::strerror(errno);
			throw std::runtime_error("could not write: " + error);
		}
		written_so_far += written;
	} while (written_so_far < size);
}

/*
 * Like pread(), but with retry and exceptions.
 */
static std::size_t
read_all(int fd, char *data, std::size_t size, off_t offset)
{
	std::size_t read_so_far = 0;

	do {
		ssize_t read = ::pread(fd,
							   data + read_so_far,
							   size - read_so_far,
							   offset + read_so_far);
		if (read < 0) {
			std::string error = std::strerror(errno);
			throw std::runtime_error("could not read: " + error);
		} else if (read == 0) {
			break;
		}
		read_so_far += read;
	} while (read_so_far < size);

	return read_so_far;
}

file::file(std::size_t sector_size, file_writeback_mode writeback_mode) :
	sector_size(sector_size),
	writeback_mode(writeback_mode)
{
}

bool
file::writeback_p(int sector_number)
{
	switch (writeback_mode) {
	case FILE_WRITEBACK_ALL:
		return true;
	case FILE_WRITEBACK_NONE:
		return false;
	case FILE_WRITEBACK_ODD:
		return sector_number % 2 == 1;
	case FILE_WRITEBACK_EVEN:
		return sector_number % 2 == 0;
	case FILE_WRITEBACK_RANDOM:
		return std::rand() % 2 == 0;
	default:
		throw std::runtime_error("unexpected file writeback mode");
	}
}

void
file::write(int fd, const char *data, std::size_t size, off_t offset)
{
	while (size > 0) {
		int sector_number = offset / sector_size;
		std::size_t offset_in_sector = offset % sector_size;
		std::size_t bytes_in_sector = std::min(size, sector_size - offset_in_sector);
		off_t sector_begin = offset - offset_in_sector;

		if (writeback_p(sector_number)) {
			// Dump this one straight into the underlying file system,
			// and drop it from our sector cache if we had it.
			write_all(fd, data, bytes_in_sector, offset);
			unwritten_sectors.erase(sector_begin);
		} else {
			// Buffer this one until fsync(), so we can risk losing
			// it.
			std::vector<char>& sector = unwritten_sectors[sector_begin];

			if (sector.empty() &&
				(offset_in_sector != 0 || bytes_in_sector != sector_size)) {
				// This is a sector we didn't previously have cached,
				// and we're not entirely filling it with new data.
				// So we'll need to read it from the underlying file
				// system before partially updating it.
				sector.resize(sector_size);
				std::size_t read_size = read_all(fd,
												 sector.data(),
												 sector_size,
												 sector_begin);
				sector.resize(std::max(read_size,
									   offset_in_sector + bytes_in_sector));
			} else {
				// Write covers whole sector, so no need to read
				// first.
				sector.resize(sector_size);
			}
			std::memcpy(sector.data() + offset_in_sector,
						data,
						bytes_in_sector);
		}
		data += bytes_in_sector;
		size -= bytes_in_sector;
		offset += bytes_in_sector;
	}
}

void
file::truncate(int fd, std::size_t size)
{
	// XXX TODO
}

void
file::synchronize(int fd)
{
	// XXX what to do if there was a truncate since we wrote?

	for (const auto& [offset, sector] : unwritten_sectors)
		write_all(fd, sector.data(), sector.size(), offset);

	unwritten_sectors.clear();
}

void
file::lose_power()
{
	if (!unwritten_sectors.empty()) {
		std::cout << "inode X "
				  << "forgot " << unwritten_sectors.size()
				  << " sectors due to power loss\n";
	}

	unwritten_sectors.clear();
}
