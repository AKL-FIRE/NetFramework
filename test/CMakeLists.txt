add_executable(test test.cpp)
add_dependencies(test sylar)
target_link_libraries(test sylar)

add_executable(test_config test_config.cpp)
add_dependencies(test_config sylar)
target_link_libraries(test_config sylar yaml-cpp)