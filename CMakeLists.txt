cmake_minimum_required(VERSION 3.5)

project(SRDP VERSION 0.2.0)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Boost CONFIG)
find_package(Catch2 3 REQUIRED)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cmake_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_config.h
)

add_library(srdp SHARED
  src/sql.cpp
  src/project.cpp
  src/experiment.cpp
  src/files.cpp
  src/config.cpp
  src/srdp.cpp
)

install(TARGETS srdp
  LIBRARY DESTINATION lib )


target_link_libraries(srdp PRIVATE -lscas)
target_include_directories(srdp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(base_test
  src/sql_test.cpp
  src/project_test.cpp
  src/experiment_test.cpp
  src/files_test.cpp
  src/config_test.cpp
  src/srdp_test.cpp
)
target_link_libraries(base_test PRIVATE Catch2::Catch2WithMain srdp ${SQLite3_LIBRARIES} -lscas)
target_include_directories(base_test PRIVATE ${CATCH2_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME base_test COMMAND base_test)

add_executable(dp src/main.cpp)
target_link_libraries(dp PRIVATE srdp ${SQLite3_LIBRARIES} -lscas -lboost_program_options )
target_include_directories(dp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS dp RUNTIME DESTINATION bin )

