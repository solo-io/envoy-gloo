#!/bin/bash

set -ex

# run static analysis. the analyze-build command will return a status code of 1
# if errors are found. the sed command is necessary because clang does not
# support the `-fno-canonical-system-headers` flag. the reason it shows up is
# because bazel generates its initial configuration with gcc (this should not
# be happening, but we can't fix it right now)

# exclusions:
#   test/ - no need to look for errors here, this will just create noise

tools/gen_compilation_database.py

sed -i 's^ -fno-canonical-system-headers^^' compile_commands.json

PATH=/opt/llvm/bin:$PATH python3 /opt/llvm/bin/analyze-build --cdb compile_commands.json --plist --verbose -o /tmp/analysis --status-bugs \
    --exclude $(bazel info execution_root)/test


