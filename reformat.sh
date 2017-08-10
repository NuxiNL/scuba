#!/bin/sh
exec find scuba \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} +
