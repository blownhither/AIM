#!/bin/bash
set -x

find | grep -E "$1[^/]*$" | grep -v -E "(arch/mips|arch/armv7a|autom4te|/\.)" | head -n3 | xargs subl
