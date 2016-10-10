find_package(PkgConfig)
pkg_check_modules(PC_LIBJSONCPP QUIET jsoncpp)
set(JSONCPP_DEFINITIONS ${PC_LIBJSONCPP_CFLAGS_OTHER})

find_path(JSONCPP_INCLUDE_DIR json/json.h
          HINTS ${PC_LIBJSONCPP_INCLUDEDIR} ${PC_LIBJSONCPP_INCLUDEDIR} )

find_library(JSONCPP_LIBRARY NAMES jsoncpp libjsoncpp
             HINTS ${PC_LIBJSONCPP_LIBDIR} ${PC_LIBJSONCPP_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Libjsoncpp  DEFAULT_MSG
                                  JSONCPP_LIBRARY JSONCPP_INCLUDE_DIR)

mark_as_advanced(JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY )

set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY} )
set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR} )
