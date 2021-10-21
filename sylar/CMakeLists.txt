add_library(sylar SHARED log.cpp util.cpp config.cpp thread.cpp fiber.cpp scheduler.cpp)
target_link_libraries(sylar pthread yaml-cpp)
force_redefine_file_macro_for_sources(sylar)
#add_library(sylar_static STATIC log.cpp)
