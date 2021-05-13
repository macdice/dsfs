#include "directory.hpp"

#include <stdexcept>

void
directory::write(int fd, const char *data, std::size_t size, off_t offset)
{
	throw std::runtime_error("cannot write to directory");
}

void
directory::truncate(int fd, std::size_t size)
{
	throw std::runtime_error("cannot truncate a directory");
}

void
directory::synchronize(int fd)
{
	// Forget about our undo log, as all changes are now committed.
	undo_log.clear();
}

void
directory::lose_power()
{
	// It's all fun and games until somebody loses and inode

	// XXX Apply some of our undo log to simulate those changes not
	// having hit the disk.
}

void
directory::rename(const std::string& from, const std::string& to)
{
}
