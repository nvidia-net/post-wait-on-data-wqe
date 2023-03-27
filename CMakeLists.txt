project(post_wod_wqe)
cmake_minimum_required(VERSION 2.8)

option(BUILD_TEST "Build the tests directory" ON)

add_library(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/post_wod_wqe.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} ibverbs)
target_link_libraries(${PROJECT_NAME} mlx5)

if(BUILD_TEST)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test/submodule_device_memory_functions)
    add_executable(${PROJECT_NAME}_test ${CMAKE_CURRENT_SOURCE_DIR}/test/main.c)
    target_include_directories(${PROJECT_NAME}_test PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/test/submodule_device_memory_functions/include)
    target_link_libraries(${PROJECT_NAME}_test 
            ${PROJECT_NAME} 
            ${CMAKE_CURRENT_SOURCE_DIR}/build/test/submodule_device_memory_functions/libdevice_memory_functions.a)
endif()