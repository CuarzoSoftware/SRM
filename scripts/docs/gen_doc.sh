#!/bin/bash

sudo apt install -y doxygen
mkdir -p ../../../srm_tmp
mkdir -p ../../../srm_tmp/html

# Get ENV variables
cd ..
chmod +x env.sh
source env.sh
cd ../doxygen

doxygen Doxyfile

cd ..

