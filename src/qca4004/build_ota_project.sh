#!/bin/bash
mkdir output_tmp
make
rm -rf output
mv output_tmp output

