#include "operation.hpp"

#include <istream>

static bool
read_symbol(std::istream& stream, std::string& out)
{
	int c;

	while ((c = stream.get()) == ' ')
		;

	for (;;) {
		if (c == EOF || c == ' ' || c == '\t' || c == ')')
			break;
		out.push_back(c);
		c = stream.get();
	}

	return out.length() > 0;
}

static int
decode_hex_digit(int x)
{
	if (x >= '0' && x <= '9')
		return x - '0';
	if (x >= 'a' && x <= 'f')
		return (x - 'a') + 10;
	return -1;
}

/*
 * Read a quoted, escaped string literal from a stream.  Used for both
 * text and binary data, so needs to tolerate NUL characters.
 * Non-printable characters are expected to be escaped as \xHH where
 * HH is a pair of lower-case hexidecimal digits.
 */
static bool
read_string(std::istream& stream, std::string& out)
{
	int c;

	out.clear();
	while ((c = stream.get()) == ' ')
		;
	if (c == '"') {
		for (;;) {
			c = stream.get();
			if (c == EOF)
				return false;
			else if (c == '"')
				return true;
			else if (c == '\\') {
				c = stream.get();
				if (c == EOF)
					return false;
				else if (c == '"')
					out.push_back('"');
				else if (c == 'n')
					out.push_back('\n');
				else if (c == '\\')
					out.push_back('\\');
				else if (c == 'x') {
					int h = stream.get();
					int l = stream.get();
					if (h == EOF || l == EOF)
						return false;
					h = decode_hex_digit(h);
					l = decode_hex_digit(l);
					if (h == -1 || l == -1)
						return false;
					out.push_back(h * 16 + l);
				}
				else
					return false;
			}
			else
				out.push_back(c);
		}
	}
	return false;
}

std::istream&
operator>>(std::istream& stream, operation& out)
{
	for (;;) {
		int c = stream.get();
		if (c == EOF)
			return stream;
		if (c == ' ' || c == '\t' || c == '\n')
			continue;
		if (c == '(') {
			bool ok = true;
			std::string op;

			if (!read_symbol(stream, op)) {
				stream.setstate(std::ios_base::badbit);
				return stream;
			}

			if (op == "mkdir") {
				out.op = operation::OP_MKDIR;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.mode);
			} else if (op == "unlink") {
				out.op = operation::OP_UNLINK;
				ok = read_string(stream, out.path);
			} else if (op == "rmdir") {
				out.op = operation::OP_RMDIR;
				ok = read_string(stream, out.path);
			} else if (op == "symlink") {
				out.op = operation::OP_SYMLINK;
				ok = read_string(stream, out.path);
				if (ok)
					ok = read_string(stream, out.path2);
			} else if (op == "rename") {
				out.op = operation::OP_RENAME;
				ok = read_string(stream, out.path);
				if (ok)
					ok = read_string(stream, out.path2);
			} else if (op == "link") {
				out.op = operation::OP_LINK;
				ok = read_string(stream, out.path);
				if (ok)
					ok = read_string(stream, out.path2);
			} else if (op == "chmod") {
				out.op = operation::OP_CHMOD;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.mode);
			} else if (op == "chown") {
				out.op = operation::OP_CHOWN;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.uid);
				if (ok)
					ok = !!(stream >> out.gid);
			} else if (op == "truncate") {
				out.op = operation::OP_TRUNCATE;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.size);
			} else if (op == "ftruncate") {
				out.op = operation::OP_FTRUNCATE;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.size);
				if (ok)
					ok = !!(stream >> out.file_handle_id);
			} else if (op == "create") {
				out.op = operation::OP_CREATE;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.flags);
				if (ok)
					ok = !!(stream >> out.mode);
				if (ok)
					ok = !!(stream >> out.file_handle_id);
			} else if (op == "open") {
				out.op = operation::OP_OPEN;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.flags);
				if (ok)
					ok = !!(stream >> out.file_handle_id);
			} else if (op == "write") {
				out.op = operation::OP_WRITE;
				ok = read_string(stream, out.path);
				if (ok)
					ok = read_string(stream, out.data);
				if (ok)
					ok = !!(stream >> out.offset);
				if (ok)
					ok = !!(stream >> out.file_handle_id);
			} else if (op == "release") {
				out.op = operation::OP_RELEASE;
				ok = !!(stream >> out.file_handle_id);
			} else if (op == "fsync") {
				out.op = operation::OP_FSYNC;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.datasync);
				if (ok)
					ok = !!(stream >> out.file_handle_id);
			} else if (op == "utimens") {
				out.op = operation::OP_UTIMENS;
				ok = read_string(stream, out.path);
				if (ok)
					ok = !!(stream >> out.utime[0].tv_sec);
				if (ok)
					ok = !!(stream >> out.utime[0].tv_nsec);
				if (ok)
					ok = !!(stream >> out.utime[1].tv_sec);
				if (ok)
					ok = !!(stream >> out.utime[1].tv_nsec);
			} else {
				stream.setstate(std::ios_base::badbit);
				ok = false;
			}
			// if anything went wrong, return an ERROR
			if (!ok)
				return stream;
			// expect the end of the list
			while ((c = stream.get()) == ' ')
				;
			if (c == ')')
				return stream;
			stream.setstate(std::ios_base::badbit);
			return stream;
		}
		stream.setstate(std::ios_base::badbit);
		return stream;
	}
}

std::string
stringify(operation::op_type op)
{
	switch (op) {
	case operation::OP_MKDIR:
		return "mkdir";
	case operation::OP_UNLINK:
		return "unlink";
	case operation::OP_RMDIR:
		return "rmdir";
	case operation::OP_SYMLINK:
		return "symlink";
	case operation::OP_RENAME:
		return "rename";
	case operation::OP_LINK:
		return "link";
	case operation::OP_CHMOD:
		return "chmod";
	case operation::OP_CHOWN:
		return "chmod";
	case operation::OP_TRUNCATE:
		return "truncate";
	case operation::OP_FTRUNCATE:
		return "ftruncate";
	case operation::OP_CREATE:
		return "create";
	case operation::OP_OPEN:
		return "open";
	case operation::OP_WRITE:
		return "write";
	case operation::OP_RELEASE:
		return "release";
	case operation::OP_UTIMENS:
		return "utimens";
	default:
		return "<unknown>";
	}
}
