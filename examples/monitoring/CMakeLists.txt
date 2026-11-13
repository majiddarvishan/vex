foreach(name IN ITEMS basic_example raii_example health_example decorator_example full_example)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Vex::monitoring)
endforeach()
