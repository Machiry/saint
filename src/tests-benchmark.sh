#!/bin/bash

OPT="$LLVM_BIN/opt"
TESTS="$LLVM_HOME/lib/Analysis/saint/tests"
RESULT="results"
IRTYPE="s"

set -x

rm -rf $RESULT

./ctainthelp.sh -s $TESTS \
  -i $TESTS \
  -o $RESULT -c g -t "$IRTYPE"

./ctainthelp.sh -s $RESULT -o $RESULT -c m -t "$IRTYPE"

./runOpt.sh -o "$OPT" -s -d -t -p "TESTS" -i $RESULT/one.bc 2> $RESULT/analysis-result
