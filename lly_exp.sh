#!/bin/bash

# raidsim read in all the .trace file in trace directory

trace=/home/rainie/Desktop/Recovery-Trace-Generator/trace/
data=/home/rainie/Desktop/disksim-erasure/raidsim_out

rm $data

for file in $trace*
    do
	#echo $file
        case "$file" in *.trace)
	    ./src/raidsim -n 8 -c raid0 -i $file -a raidsim_out -p valid/ascii.parv -o disksim_out
	esac
    done
	

