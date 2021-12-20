add_library(sylar SHARED log.cpp util.cpp config.cpp thread.cpp fiber.cpp scheduler.cpp iomanager.cpp timer.cpp hook.cpp fd_manager.cpp address.cpp)
target_link_libraries(sylar pthread yaml-cpp dl)
force_redefine_file_macro_for_sources(sylar)
#add_library(sylar_static STATIC log.cpp)
