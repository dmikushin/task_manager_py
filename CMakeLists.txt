cmake_minimum_required(VERSION 3.5)

project(task_manager)

add_subdirectory(ThirdParty/googletest)
add_subdirectory(ThirdParty/pybind11)

add_library(${PROJECT_NAME} src/${PROJECT_NAME}.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

add_executable(test_${PROJECT_NAME} src/test/test_${PROJECT_NAME}.cpp)
set_property(TARGET test_${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
target_link_libraries(test_${PROJECT_NAME} ${PROJECT_NAME} gtest gtest_main)

pybind11_add_module(${PROJECT_NAME}_py src/python/${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME}_py PRIVATE ${PROJECT_NAME})

