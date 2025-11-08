find ../include/ -iname *.h | xargs clang-format-16 -i
find ../include/ -iname *.hpp | xargs clang-format-16 -i
find ../src/ -iname *.cpp | xargs clang-format-16 -i

find ../tests/ -iname *.cpp | xargs clang-format-16 -i
find ../examples/ -iname *.cpp | xargs clang-format-16 -i
