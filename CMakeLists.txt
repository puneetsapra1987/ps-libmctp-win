cmake_minimum_required (VERSION 3.10 FATAL_ERROR)

project (libmctp VERSION 1.0.0 LANGUAGES C)

# Configuration options
set(MCTP_MAX_MESSAGE_SIZE 33554432 CACHE INTEGER "Maximum MCTP message size")
set(MCTP_REASSEMBLY_CTXS 16 CACHE INTEGER "Number of concurrent reassembly contexts")
set(MCTP_REQ_TAGS 16 CACHE INTEGER "Number of outbound request tags")

option(DEV "Option for developer testing" OFF)

if(DEV)
	set(CMAKE_C_FLAGS
	"${CMAKE_C_FLAGS} \
	-Werror \
	-Wall \
	-Wextra \
	-Wnull-dereference \
	-Wformat-security \
	-Wno-type-limits \
	-fsanitize=address,leak,undefined \
	-ggdb \
	")
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# Definitions
add_definitions (-DMCTP_LOG_STDERR)
add_definitions (-DMCTP_HAVE_STDIO)
add_definitions (-DMCTP_DEFAULT_ALLOC)
add_definitions (-DMCTP_DEFAULT_CLOCK_GETTIME=0)
add_definitions (-DMCTP_MAX_MESSAGE_SIZE=${MCTP_MAX_MESSAGE_SIZE})
add_definitions (-DMCTP_REASSEMBLY_CTXS=${MCTP_REASSEMBLY_CTXS})
add_definitions (-DMCTP_REQ_TAGS=${MCTP_REQ_TAGS})

# MCTP library
add_library (mctp STATIC src/alloc.c src/core.c src/log.c src/control.c src/mmbi.c)

target_include_directories (mctp PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>)

enable_testing ()

add_executable (test_eid tests/test_eid.c tests/test-utils.c)
target_link_libraries (test_eid mctp)
add_test (NAME eid COMMAND test_eid)

add_executable (test_seq tests/test_seq.c tests/test-utils.c)
target_link_libraries (test_seq mctp)
add_test (NAME seq COMMAND test_seq)

add_executable (test_bridge tests/test_bridge.c tests/test-utils.c)
target_link_libraries (test_bridge mctp)
add_test (NAME bridge COMMAND test_bridge)

# Serial test removed

add_executable (test_cmds tests/test_cmds.c tests/test-utils.c)
target_link_libraries (test_cmds mctp)
add_test (NAME control_commands COMMAND test_cmds)

add_executable (test_core tests/test_core.c tests/test-utils.c)
target_link_libraries (test_core mctp)
add_test (NAME core COMMAND test_core)

add_executable (test_mmbi tests/test_mmbi.c tests/test-utils.c)
target_link_libraries (test_mmbi mctp)
add_test (NAME mmbi COMMAND test_mmbi)

add_executable (test_mmbi_host tests/test_mmbi_host.c tests/test-utils.c)
target_link_libraries (test_mmbi_host mctp)

add_executable (test_mmbi_bmc tests/test_mmbi_bmc.c tests/test-utils.c)
target_link_libraries (test_mmbi_bmc mctp)

add_executable (test_file_transfer tests/test_file_transfer.c tests/test-utils.c)
target_link_libraries (test_file_transfer mctp)

add_executable (test_stress_mmbi tests/test_stress_mmbi.c tests/test-utils.c)
target_link_libraries (test_stress_mmbi mctp)

add_test (NAME stress_mmbi COMMAND test_stress_mmbi)

add_executable (test_mmbi_log tests/test_mmbi_log.c)
target_link_libraries (test_mmbi_log mctp)
add_test (NAME mmbi_log COMMAND test_mmbi_log)

add_executable (test_mmbi_full_duplex tests/test_mmbi_full_duplex.c tests/test-utils.c)
target_link_libraries (test_mmbi_full_duplex mctp)
add_test (NAME mmbi_full_duplex COMMAND test_mmbi_full_duplex)

# Transport API Example Apps
add_executable (host_transport tests/host_transport.c)
target_link_libraries (host_transport mctp)

add_executable (bmc_transport tests/bmc_transport.c)
target_link_libraries (bmc_transport mctp)

install (TARGETS mctp DESTINATION lib)
install (FILES include/libmctp.h include/libmctp-mmbi.h DESTINATION include)

