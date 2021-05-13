#include "operation.hpp"
#include "replayer.hpp"

#include <climits>
#include <iostream>

static int
usage(const char *program_name)
{
	std::cerr << "usage: " << program_name << " target_path\n"
			  << "  [ --sector-size bytes ]  : simulated sector size\n"
			  << "  [ --skip N ]             : skip first N ops\n"
			  << "  [ --take N ]             : only replay N ops\n"
			  << "  [ --start-before-OP N ]  : skip up until Nth OP\n"
			  << "  [ --start-after-OP N ]   : skip up to and including Nth OP\n"
			  << "  [ --stop-before-OP N ]   : replay up to the Nth OP\n"
			  << "  [ --stop-after-OP N ]    : up to and including Nth OP\n"
			  << "  [ --stop-touch PATH ]    : stop after PATH is created\n"
			  << "  [ --start-touch PATH ]   : start after PATH is created\n"
			  << "  [ --writeback MODE ]     : which sectors to write before fsync\n"
			  << "where OP is one of:\n"
			  << "  create, open, write, release, fsync, link unlink, rename, mkdir, rmdir\n"
			  << "where MODE is one of:\n"
			  << "  all, none, odd, even, random\n";
	return EXIT_FAILURE;
}

int
main(int argc, const char *argv[])
{
	std::string target_path;
	operation op;
	int line_number;
	bool skip_until_start_trigger = false;

	std::string start_touch;
	std::string stop_touch;
	off_t sector_size = 512;
	int take = std::numeric_limits<int>::max();
	int skip = 0;
	int operations = 0;
	file_writeback_mode writeback_mode = FILE_WRITEBACK_ALL;

	if (argc < 2)
		return usage(argv[0]);

	target_path = argv[1];
	for (int i = 2; i < argc; ++i) {
		std::string opt = argv[i];
		bool more = i + 1 < argc;
		if (opt == "--sector-size" && more) {
			sector_size = atoi(argv[++i]);
		} else if (opt == "--take" && more) {
			take = atoi(argv[++i]);
		} else if (opt == "--skip" && more) {
			skip = atoi(argv[++i]);
		} else if (opt == "--writeback" && more) {
			std::string mode = argv[++i];
			if (mode == "all")
				writeback_mode = FILE_WRITEBACK_ALL;
			else if (mode == "none")
				writeback_mode = FILE_WRITEBACK_NONE;
			else if (mode == "odd")
				writeback_mode = FILE_WRITEBACK_ODD;
			else if (mode == "even")
				writeback_mode = FILE_WRITEBACK_EVEN;
			else if (mode == "random")
				writeback_mode = FILE_WRITEBACK_RANDOM;
			else
				return usage(argv[0]);
		} else if (opt == "--stop-touch" && more) {
			stop_touch = argv[++i];
		} else if (opt == "--start-touch" && more) {
			start_touch = argv[++i];
			skip_until_start_trigger = true;
		} else {
			return usage(argv[0]);
		}
	}

	try {
		replayer fs(target_path, sector_size, writeback_mode);
		line_number = 0;
		while (operations < take && !!(std::cin >> op)) {
			++line_number;

			if (skip > 0) {
				skip--;
				continue;
			}

			if (op.op == operation::OP_CREATE) {
				if (!stop_touch.empty() && op.path == stop_touch)
					break;
				else if (skip_until_start_trigger &&
						 !start_touch.empty() &&
						 op.path == start_touch)
					skip_until_start_trigger = false;
			}

			fs.replay(op);
			++operations;
		}
		fs.lose_power();
	} catch (const std::exception& e) {
		std::cerr << "while processing line " << line_number << ": "
				  << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
