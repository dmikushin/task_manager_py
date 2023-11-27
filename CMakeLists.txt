cmake_minimum_required(VERSION 3.5)

project(task_manager)

add_library(${PROJECT_NAME} src/${PROJECT_NAME}.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

add_executable(test_${PROJECT_NAME} src/test/test_${PROJECT_NAME}.cpp)
target_include_directories(test_${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_property(TARGET test_${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

