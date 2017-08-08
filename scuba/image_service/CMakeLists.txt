add_aprotoc(scuba/image_service/configuration)

add_executable(
	scuba_image_service
	${KUBERNETES_GRPC_HDRS}
	${KUBERNETES_PROTOBUF_HDRS}
	scuba/image_service/configuration.ad.h
	scuba/image_service/image_service.cc
	scuba/image_service/program_main.cc
)

target_link_libraries(
	scuba_image_service
	kubernetes_cri_lib
	Threads::Threads
	arpc
	cares
	gpr
	grpc++_unsecure
	grpc_unsecure
	protobuf
	z
)

install(
	TARGETS scuba_image_service
	DESTINATION ${CMAKE_INSTALL_BINDIR}
)
