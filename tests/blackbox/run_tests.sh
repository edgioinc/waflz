#!/bin/bash
# ==============================================================================
# wipe out cache files
# ==============================================================================
find . -type f -name '*.pyc' -delete
find . -type d -name '__pycache__' -exec rm -rf {} +
# ==============================================================================
# Run tests with pytest
# ==============================================================================
export CMAKE_BINARY_DIR=$1
TESTS_LOCATION=$(dirname $0)
if [ -n "$1" ]; then
    pytest -v $1
else
    #--durations=2 ==> find out which tests are the slowest. 
    pytest -v -s --durations=2 ${TESTS_LOCATION}
fi
