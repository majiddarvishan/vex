set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic -Wno-unused-parameter)

find_package(fmt)

add_executable(integrity integrity.cpp)
target_link_libraries(integrity pinex pthread fmt::fmt)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(code_analysis code_analysis.cpp)
target_link_libraries(code_analysis pinex pthread)
