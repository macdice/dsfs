#include <cstdlib>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CHECK(condition)								\
	if (!(condition)) {									\
		int save_errno = errno;							\
		std::cerr << "check failed: " #condition		\
				  << ": line " << __LINE__				\
				  << " (errno=" << save_errno << ")\n";	\
		exit(EXIT_FAILURE);								\
	}

int
main(int argc, char *argv[])
{
	int rc;
	int fd;
	char buffer[80];

	// Smoke test: generate an example of every kind of operation, and
	// some based failure cases.

	rc = ::mkdir("test_dir", 0744);
	CHECK(rc == 0);

	rc = ::mkdir("test_dir", 0744);
	CHECK(rc == -1);
	CHECK(errno == EEXIST);
	
	fd = ::open("test_dir/x", O_CREAT | O_RDWR, 0644);
	CHECK(fd >= 0);

	rc = ::pwrite(fd, "hello world", 12, 8);
	CHECK(rc == 12);

	rc = ::pread(fd, buffer, 12, 8);
	CHECK(rc == 12);
	CHECK(std::memcmp(buffer, "hello world", 12) == 0);
	
	rc = ::unlink("test_dir/x");
	CHECK(rc == 0);

	rc = ::pwrite(fd, "..... .....", 12, 8);
	CHECK(rc == 12);
	
	rc = ::pread(fd, buffer, 12, 8);
	CHECK(rc == 12);
	CHECK(std::memcmp(buffer, "..... .....", 12) == 0);

	rc = ::close(fd);
	CHECK(rc == 0);

	rc = ::mkdir("test_dir2", 0744);
	CHECK(rc == 0);

	rc = ::rmdir("test_dir2");
	CHECK(rc == 0);

	rc = ::rmdir("test_dir3");
	CHECK(rc == -1);
	CHECK(errno == ENOENT);

	rc = ::rename("test_dir", "test_dir3");
	CHECK(rc == 0);

	// most Unixoid filesystems won't allow multiply-linked dirs (but
	// it's possible that the assumption in this test could fail on
	// some POSIX-compliant system out there)
	rc = ::link("test_dir3", "test_dir4");
	CHECK(rc == -1);
	CHECK(errno == EPERM || errno == EACCES);

	rc = ::symlink("test_dir3", "test_symlink");
	CHECK(rc == 0);
	
	fd = ::open("test_dir3/x", O_CREAT | O_RDWR, 0644);
	CHECK(fd >= 0);
	
	rc = ::pwrite(fd, "hello world", 12, 8);
	CHECK(rc == 12);

	rc = ::close(fd);
	CHECK(rc == 0);

	fd = ::open("test_dir3/x", O_RDWR);
	CHECK(fd >= 0);

	rc = ::pread(fd, buffer, 12, 8);
	CHECK(rc == 12);
	CHECK(std::memcmp(buffer, "hello world", 12) == 0);
	
	
	
	return EXIT_SUCCESS;
}
