#!/bin/sh

find ./include \( -name "*.h" -or -name "*.c" \) -print0 | xargs -0 clang-format -i
find ./src \( -name "*.h" -or -name "*.c" \) -print0 | xargs -0 clang-format -i
find ./tests \( -name "*.h" -or -name "*.c" \) -print0 | xargs -0 clang-format -i

astyle --recursive "./include/*.h" "./src/*.c" "./tests/*.c" "./tests/*.h" \
-n --indent-namespaces --max-code-length=80 --min-conditional-indent=0 \
--pad-oper --align-pointer=name
