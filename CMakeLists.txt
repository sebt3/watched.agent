cmake_minimum_required (VERSION 2.8.7)
project (watched.agent)

####
## C++11 Compiler 
#
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

####
## Compiler option
#
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")


####
## Dependencies
#
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Threads REQUIRED)
find_package(Boost 1.53.0 COMPONENTS regex system thread filesystem date_time REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Jsoncpp REQUIRED)
include_directories(SYSTEM ${JSONCPP_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

####
## Sources
#
file (GLOB	      HEADER_FILES "include/*.h")
file (GLOB      AGENT_SOURCE_FILES "src/*.cpp")
file (GLOB COLLECTORS_SOURCE_FILES "collectors/*.cpp" )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/sws)
install(FILES ${HEADER_FILES} DESTINATION include/watched)

####
## Targets
#
set( AGT_SOURCE_FILES ${AGENT_SOURCE_FILES} )
add_executable(watched.agent ${AGT_SOURCE_FILES})
target_link_libraries(watched.agent dl ${JSONCPP_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS watched.agent DESTINATION sbin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
foreach( collector_cpp ${COLLECTORS_SOURCE_FILES} )
	get_filename_component( collector ${collector_cpp} NAME_WE )
	add_library( ${collector} SHARED ${collector_cpp} )
	install(TARGETS ${collector} DESTINATION lib/watched/collectors.cpp)
endforeach( collector_cpp ${COLLECTORS_SOURCE_FILES} )


####
## Usefull debug stuff
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

