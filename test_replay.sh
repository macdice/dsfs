#!/bin/sh

set -e

test_name="$1"
log="tests/$1.log"
operations="` wc -l < $log `"

mkdir -p output

echo $test_name
for i in ` seq 0 $operations ` ; do
	target_dir=output/$test_name.$i
	expected_dir=expected/$test_name.$i
	rm -fr $target_dir && mkdir -p $target_dir $expected_dir
	./dsfs_replay $target_dir --sector-size 3 --writeback even --take $i < $log > output/$test_name.stdout
done
for i in ` seq 0 $operations ` ; do
	target_dir=output/$test_name.$i
	expected_dir=expected/$test_name.$i
	diff -a -u -r -x .empty $target_dir $expected_dir
done
