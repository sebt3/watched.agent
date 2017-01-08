find_path(DBUSCPP_INCLUDE_DIR dbus-c++/dbus.h
                 PATH_SUFFIXES include/dbus-c++-1)




find_library(DBUSCPP_LIBRARIES dbus-c++-1
                 PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DBusCpp DEFAULT_MSG
                                        DBUSCPP_LIBRARIES
                                        DBUSCPP_INCLUDE_DIR)
