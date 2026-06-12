cmake_minimum_required(VERSION 3.20)
project(ff7kor LANGUAGES CXX)

add_library(ff7kor SHARED src/ff7kor.cpp)
target_compile_features(ff7kor PRIVATE cxx_std_17)
target_compile_definitions(ff7kor PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(ff7kor PRIVATE /W4 /permissive-)
  target_link_options(ff7kor PRIVATE /DYNAMICBASE:NO /BASE:0x10000000)
endif()
