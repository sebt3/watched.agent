cmake_minimum_required (VERSION 2.8.7)
project (watched.agent)
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

find_package(Threads REQUIRED)

find_package(Boost 1.53.0 COMPONENTS regex system thread filesystem date_time REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

#TODO: utiliser find_package pour trouver jsoncpp (pkgconfig) et mysql
#TODO: utiliser le méthode cmake pour positionner -Wall et -fPIC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include -Wall -I/usr/include/mysql")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L/usr/local/lib -shared -rdynamic -lpthread -lm -ljsoncpp -ldl ")

file (GLOB	      HEADER_FILES "include/*.h")
file (GLOB      AGENT_SOURCE_FILES "src/*.cpp")
file (GLOB COLLECTORS_SOURCE_FILES "collectors/*.cpp" )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/sws)
install(FILES ${HEADER_FILES} DESTINATION include/watched)

set( AGT_SOURCE_FILES ${AGENT_SOURCE_FILES} )
add_executable(watched.agent ${AGT_SOURCE_FILES})
target_link_libraries(watched.agent m jsoncpp pthread dl)
target_link_libraries(watched.agent ${Boost_LIBRARIES})
target_link_libraries(watched.agent ${OPENSSL_LIBRARIES})
target_link_libraries(watched.agent ${CMAKE_THREAD_LIBS_INIT})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
foreach( collector_cpp ${COLLECTORS_SOURCE_FILES} )
	get_filename_component( collector ${collector_cpp} NAME_WE )
	add_library( ${collector} SHARED ${collector_cpp} )
	install(TARGETS ${collector} DESTINATION plugins)
endforeach( collector_cpp ${COLLECTORS_SOURCE_FILES} )
