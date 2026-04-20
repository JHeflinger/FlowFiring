cmake_minimum_required(VERSION 3.18)
project(flowfiring_native LANGUAGES CXX)

option(WITH_CUDA "Enable CUDA support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 REQUIRED)
find_package(OpenMP QUIET)
find_package(Eigen3 REQUIRED NO_MODULE)

set(SOURCES
    bindings.cpp
    firing_cpu.cpp
    lattice.cpp
    simplicial.cpp
    coloring.cpp
    builders.cpp
)

if(WITH_CUDA)
    enable_language(CUDA)
    list(APPEND SOURCES firing_cuda.cu)
endif()

pybind11_add_module(_native ${SOURCES})

target_include_directories(_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(_native PRIVATE Eigen3::Eigen)

install(TARGETS _native DESTINATION flowfiring/_native)

if(WITH_CUDA)
    target_compile_definitions(_native PRIVATE WITH_CUDA)
    set_target_properties(_native PROPERTIES CUDA_ARCHITECTURES "70;75;80;86;89;90")
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(_native PRIVATE OpenMP::OpenMP_CXX)
endif()
