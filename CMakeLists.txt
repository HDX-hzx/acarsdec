cmake_minimum_required (VERSION 3.22)
cmake_policy(SET CMP0167 OLD)

project (acarsdec C)
set(Boost_NO_BOOST_CMAKE ON)
add_compile_options(-O2)

add_executable(acarsdec acars.c  acarsdec.c  cJSON.c  label.c  msk.c  output.c
		usrp.h
		usrp.c
		pkt.c
		modulator.c
		util.c)
add_library(acarsrec SHARED testing.c acars.c   cJSON.c  label.c  msk.c  output.c )
set_target_properties(acarsrec PROPERTIES output_name "acarsrec")

find_package(PkgConfig)
find_package(Boost REQUIRED)
find_package(UHD REQUIRED)

target_compile_definitions(acarsdec
		PRIVATE
		WITH_USRP
)
target_link_libraries(acarsdec uhd)

add_definitions(-DWITH_USRP)

if(PKG_CONFIG_FOUND)
pkg_check_modules(LIBACARS libacars-2>=2.0.0)
if(LIBACARS_FOUND)
message ( STATUS "Using libacars")
add_definitions(-DHAVE_LIBACARS )


target_link_libraries(acarsdec ${LIBACARS_LIBRARIES} uhd)
target_include_directories(acarsdec PUBLIC ${LIBACARS_INCLUDE_DIRS})
target_link_libraries(acarsrec ${LIBACARS_LIBRARIES} uhd)
target_include_directories(acarsrec PUBLIC ${LIBACARS_INCLUDE_DIRS})
link_directories(${LIBACARS_LIBRARY_DIRS})
else()
message ( STATUS "Not using libacars")
endif()
endif()

#option(rtl "Compiling for rtl sdr" )
if(rtl)
find_library(LIBRTL rtlsdr)
if(NOT LIBRTL)
message (FATAL_ERROR "librtlsdr path not found")
endif()
add_definitions(-DWITH_RTL -DWITH_USRP)
target_sources( acarsdec PRIVATE rtl.c)
target_sources( acarsrec PRIVATE rtl.c)
target_link_libraries( acarsdec ${LIBRTL})
target_link_libraries( acarsrec ${LIBRTL})
endif()
if(NOT rtl)
message ("No sdr option set ! are you sure ?")
endif()

target_link_libraries( acarsdec pthread m )
target_link_libraries( acarsrec pthread m )

install(TARGETS acarsdec
	RUNTIME DESTINATION bin
)
