########
# Find systemd service dir

pkg_check_modules(SYSTEMD "systemd")
if (SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
		--variable=systemdsystemunitdir systemd
		OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
	string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
		"${SYSTEMD_SERVICES_INSTALL_DIR}")
elseif (NOT SYSTEMD_FOUND AND SYSTEMD_SERVICES_INSTALL_DIR)
	message (FATAL_ERROR "Variable SYSTEMD_SERVICES_INSTALL_DIR is\
		defined, but we can't find systemd using pkg-config")
endif()

if (SYSTEMD_FOUND)
	set(WITH_SYSTEMD "ON")
	message(STATUS "systemd services install dir: ${SYSTEMD_SERVICES_INSTALL_DIR}")
	if (SYSTEMD_VERSION GREATER 220 )
		set(HAVE_SDBUS true)
	else()
		set(HAVE_SDBUS false)
	endif (SYSTEMD_VERSION GREATER 220 )
else()
	set(WITH_SYSTEMD "OFF")
endif (SYSTEMD_FOUND)


########
# Find libsystemd information
pkg_check_modules(LIBSYSTEMD QUIET libsystemd)
if (SYSTEMD_FOUND AND LIBSYSTEMD_FOUND)
	find_library(SYSTEMD_LIBRARIES NAMES systemd ${LIBSYSTEMD_LIBRARY_DIRS})
	find_path(SYSTEMD_INCLUDE_DIRS systemd/sd-bus.h HINTS ${LIBSYSTEMD_INCLUDE_DIRS})
	if (SYSTEMD_VERSION GREATER 220 )
		set(HAVE_SDBUS true)
	else()
		set(HAVE_SDBUS false)
	endif (SYSTEMD_VERSION GREATER 220 )
endif (SYSTEMD_FOUND AND LIBSYSTEMD_FOUND)
