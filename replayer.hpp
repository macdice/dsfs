#include "directory.hpp"
#include "file.hpp"
#include "inode.hpp"
#include "operation.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

/*
 * We maintain a table of file handles, indexed by file handle ID, so
 * that we have a place to keep track of our file descriptor and our
 * inode object.
 */
struct file_handlex {
	file_handlex(int fd, inode *inode) : fd(fd), inode(inode) {}
	file_handlex() : fd(-1), inode(NULL) {}
	int fd;
	struct inode *inode;
};

struct replayer {
	/*
	 * Construct a replayer that will replay operations into a given
	 * directory.  The path doesn't have to be the same as was used
	 * when recording.
	 */
	replayer(const std::string& target_path,
			 off_t sector_size,
			 file_writeback_mode file_writeback_mode);

	/*
	 * Replay one operation into the target directory.  Throws on error.
	 */
	void replay(const operation& op);

	void lose_power();

private:
	const std::string target_path;
	off_t sector_size;
	file_writeback_mode file_mode;
	std::vector<file_handlex> file_handle_table;
	std::unordered_map<ino_t, std::unique_ptr<inode>> inode_table;

	void remap(std::string& remapped, const std::string& path);
	directory& get_directory(const std::string& path);
	const file_handlex& get_file_handle(const operation& op);
	void open_file_handle(const std::string& path,
						  int file_handle_id,
						  int fd);
	void close_file_handle(int file_handle_id);

	/*
	 * Scratch space preserved across replay() calls, for computing
	 * paths without repeated allocation.
	 */
	std::string remapped;
	std::string remapped2;
	std::string parent;
	std::string parent2;
};
