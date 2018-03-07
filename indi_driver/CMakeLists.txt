cmake_minimum_required(VERSION 3.6.2)

PROJECT(indi-mupastrocat CXX C)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# http://gcc.gnu.org/wiki/Visibility
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
# Better way for when newer cmake is available 
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
#set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

######################################################################
# Dependencies
######################################################################
  
find_package(INDI REQUIRED)
find_package(WiringPi REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${WiringPi_INCLUDE_DIR})

######################################################################
# MUP Astro CAT INDI Driver
######################################################################

set(MUPASTROCAT_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/indi-mupastrocat/mupastrocat.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/indi-mupastrocat/motorcontroller.cpp
)

add_executable(indi_mupastrocat ${MUPASTROCAT_SOURCES})

target_link_libraries(indi_mupastrocat ${INDI_DRIVER_LIBRARIES} ${M_LIB} ${WiringPi_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_mupastrocat RUNTIME DESTINATION bin)

install(FILES indi-mupastrocat/indi_mupastrocat.xml DESTINATION ${INDI_DATA_DIR})
