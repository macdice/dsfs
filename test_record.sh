#!/bin/sh

set -e

test_name="$1"
here=`pwd`
log=output/$test_name.log
mount_point=output/${test_name}_mount_point
underlying=output/${test_name}_underlying
replayed=output/r${test_name}_replayed

echo $test_name

rm -fr $mount_point $underlying $log $replayed
mkdir -p $mount_point $underlying $replayed

# record the I/O of a trivial program that exercises all operations
./dsfs_record $mount_point $here/$underlying $log
sleep 5 # hnnggngng
( cd $mount_point && $here/test_program $test_name )
umount $mount_point

# compare the generated log against the expected output
# XXX

# replay it for good measure
./dsfs_replay $replayed < $log

# directories should have identical contents
diff -a -u -r $underlying $replayed
