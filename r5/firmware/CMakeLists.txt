file(GLOB GEN_C  "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/*.pb.c")
file(GLOB GEN_H  "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/*.pb.h")
add_library(calc_proto ${GEN_C})
target_include_directories(calc_proto PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/generated)
# Link calc_proto to your firmware target, e.g.:
# target_link_libraries(r5_firmware PRIVATE calc_proto)
