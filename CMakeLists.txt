cmake_minimum_required(VERSION 2.8)
project( product-flaw-detector )
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )
add_executable( product-flaw-detector product-flaw-detector.cpp )
target_link_libraries( product-flaw-detector ${OpenCV_LIBS}) 


