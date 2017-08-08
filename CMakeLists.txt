cmake_minimum_required(VERSION 3.8.0)

project(scuba)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

include_directories(
	${CMAKE_SOURCE_DIR}
	${CMAKE_BINARY_DIR}
)

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

include(cmake/gRPC.txt)

protobuf_generate_cpp(
	KUBERNETES_PROTOBUF_SRCS
	KUBERNETES_PROTOBUF_HDRS
	github.com/gogo/protobuf/gogoproto/gogo.proto
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.proto
)
grpc_generate_cpp(
	KUBERNETES_GRPC_SRCS
	KUBERNETES_GRPC_HDRS
	github.com/gogo/protobuf/gogoproto/gogo.proto
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.proto
)

add_library(
	kubernetes_cri_lib
	${KUBERNETES_PROTOBUF_SRCS}
	${KUBERNETES_PROTOBUF_HDRS}
	${KUBERNETES_GRPC_SRCS}
	${KUBERNETES_GRPC_HDRS}
)

include(scuba/image_service/CMakeLists.txt)
include(scuba/runtime_service/CMakeLists.txt)
