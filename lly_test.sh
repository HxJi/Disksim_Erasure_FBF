#!/bin/bash

# raidsim read in all the .trace file in trace directory

trace=/home/rainie/Desktop/Recovery-Trace-Generator/trace/star_p=5_error=0_stripe=1_origin.trace
data=/home/rainie/Desktop/disksim-erasure/raidsim_out

./src/raidsim -n 8 -c raid0 -i $trace -a raidsim_out -p valid/ascii.parv -o disksim_out


