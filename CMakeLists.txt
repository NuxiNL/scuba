cmake_minimum_required(VERSION 3.8.0)

project(scuba)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

set(APROTOC_NAME "aprotoc" CACHE STRING "Name of the aprotoc command")
find_program(APROTOC_COMMAND ${APROTOC_NAME})
mark_as_advanced(APROTOC_COMMAND)
if(NOT APROTOC_COMMAND)
	message(FATAL_ERROR "Could not find aprotoc, set APROTOC_NAME")
endif()

function(add_aprotoc base)
	get_filename_component(base_dir ${base} DIRECTORY)
	add_custom_command(
		OUTPUT ${base}.ad.h
		COMMAND mkdir -p ${CMAKE_BINARY_DIR}/${base_dir} && ${APROTOC_COMMAND} <${CMAKE_SOURCE_DIR}/${base}.proto >${CMAKE_BINARY_DIR}/${base}.ad.h
		DEPENDS ${CMAKE_SOURCE_DIR}/${base}.proto
	)

	install(
		FILES ${base}.proto ${CMAKE_BINARY_DIR}/${base}.ad.h
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${base_dir}
	)
endfunction()

set(PROTOC_NAME "protoc" CACHE STRING "Name of the protoc command")
find_program(PROTOC_COMMAND ${PROTOC_NAME})
mark_as_advanced(PROTOC_COMMAND)
if(NOT PROTOC_COMMAND)
	message(FATAL_ERROR "Could not find protoc, set PROTOC_NAME")
endif()
set(GRPC_CPP_PLUGIN_NAME "grpc_cpp_plugin" CACHE STRING "Name of the grpc_cpp_plugin command")
find_program(GRPC_CPP_PLUGIN_COMMAND ${GRPC_CPP_PLUGIN_NAME})
mark_as_advanced(GRPC_CPP_PLUGIN_COMMAND)
if(NOT GRPC_CPP_PLUGIN_COMMAND)
	message(FATAL_ERROR "Could not find grpc_cpp_plugin, set GRPC_CPP_PLUGIN_NAME")
endif()

# TODO(ed): Remove references to /usr/local/*!
function(add_protoc base)
	add_custom_command(
		OUTPUT
			${base}.pb.cc
			${base}.pb.h
		COMMAND ${PROTOC_COMMAND} -I/usr/local/x86_64-unknown-cloudabi/include -I${CMAKE_SOURCE_DIR}/contrib --cpp_out=${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/contrib/${base}.proto
		DEPENDS ${CMAKE_SOURCE_DIR}/contrib/${base}.proto
	)
	add_custom_command(
		OUTPUT ${base}.grpc.pb.cc ${base}.grpc.pb.h
		COMMAND ${PROTOC_COMMAND} -I/usr/local/x86_64-unknown-cloudabi/include -I${CMAKE_SOURCE_DIR}/contrib --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_COMMAND} --grpc_out=${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/contrib/${base}.proto
		DEPENDS ${CMAKE_SOURCE_DIR}/contrib/${base}.proto
	)
endfunction()

add_protoc(github.com/gogo/protobuf/gogoproto/gogo)
add_protoc(k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

include(scuba/image_service/CMakeLists.txt)
include(scuba/runtime_service/CMakeLists.txt)
