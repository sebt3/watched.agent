find_path(ASIO_INCLUDE_DIR asio.hpp HINTS /usr/include /usr/local/include )
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio FOUND_VAR ASIO_FOUND REQUIRED_VARS ASIO_INCLUDE_DIR)
mark_as_advanced(ASIO_INCLUDE_DIR )
set(ASIO_INCLUDE_DIRS ${ASIO_INCLUDE_DIR} )
