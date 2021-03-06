#!/bin/bash

CURL="$HOME/ece750-project/benchmarks/curl"
RESULT="curl-results"

set -x

./ctainthelp.sh -s $CURL/src -i $CURL \
  -i $CURL/lib \
  -i $CURL/include \
  -o $RESULT -c b

./ctainthelp.sh -s $RESULT -o $RESULT -c m

./runOpt.sh -i $RESULT/one.bc 2> $RESULT/analysis-result

