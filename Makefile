CXXFLAGS=-Wall -g -I/usr/local/include -std=c++17
LDFLAGS=-L/usr/local/lib -lfuse

REPLAY_OBJS= \
	dsfs_replay.o \
	directory.o \
	file.o \
	operation.o \
	replayer.o

all: dsfs_record dsfs_replay test_program

dsfs_record: dsfs_record.o
	$(CXX) -o $@ dsfs_record.o $(CXXFLAGS) $(LDFLAGS)

dsfs_replay: $(REPLAY_OBJS)
	$(CXX) -o $@ $(REPLAY_OBJS) $(CXXFLAGS) $(LDFLAGS)

test_program: test_program.o
	$(CXX) -o $@ test_program.o $(CXXFLAGS) $(LDFLAGS)

check: check-record check-replay

check-record: test_program
	@echo "=== record tests (requires fuse) ==="
	@for test in record1 ; do ./test_record.sh $$test ; done

check-replay:
	@echo "=== replay tests ==="
	@for test in tests/replay*.log ; do ./test_replay.sh $$(basename $$test | cut -f1 -d'.') ; done

clean:
	rm -fr dsfs_record dsfs_replay dsfs_record.o test_program test_program.o $(REPLAY_OBJS)

check-syntax:
	$(CXX) -o /dev/null -S ${CHK_SOURCES} ${CXXFLAGS} || true
