add_library(sylar SHARED log.cpp util.cpp config.cpp)
force_redefine_file_macro_for_sources(sylar)
#add_library(sylar_static STATIC log.cpp)
