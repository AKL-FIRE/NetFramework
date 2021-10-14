add_executable(test test.cpp)
add_dependencies(test sylar)
target_link_libraries(test sylar)
force_redefine_file_macro_for_sources(test)

add_executable(test_config test_config.cpp)
add_dependencies(test_config sylar)
target_link_libraries(test_config sylar)
force_redefine_file_macro_for_sources(test_config)

add_executable(test_thread test_thread.cpp)
add_dependencies(test_thread sylar)
target_link_libraries(test_thread sylar)
force_redefine_file_macro_for_sources(test_thread)
