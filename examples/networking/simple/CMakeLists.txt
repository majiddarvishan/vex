set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic -Wno-unused-parameter -DASIO_NO_DEPRECATED=1)

find_package(fmt)

add_executable(server server.cpp)
target_link_libraries(server pinex pthread fmt::fmt boost_thread)

add_executable(client client.cpp)
target_link_libraries(client pinex pthread fmt::fmt)

add_executable(client_list client_list.cpp)
target_link_libraries(client_list pinex pthread fmt::fmt)
