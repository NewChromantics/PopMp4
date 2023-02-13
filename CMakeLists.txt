add_library(PopMp4 STATIC)
target_sources(PopMp4 PRIVATE Source/Mp4Parser.cpp)

target_include_directories(
  PopMp4 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Source>
                $<INSTALL_INTERFACE:Source>)

set_target_properties(
  PopMp4
  PROPERTIES OUTPUT_NAME popmp4
             CXX_STANDARD ${CXX_VERSION}
             CXX_EXTENSIONS OFF)
