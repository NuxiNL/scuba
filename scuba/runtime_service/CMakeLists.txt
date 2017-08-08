add_aprotoc(scuba/runtime_service/configuration)

add_executable(
	scuba_runtime_service
	github.com/gogo/protobuf/gogoproto/gogo.pb.cc
	github.com/gogo/protobuf/gogoproto/gogo.pb.h
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.cc
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.cc
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h
	scuba/runtime_service/configuration.ad.h
	scuba/runtime_service/container.cc
	scuba/runtime_service/ip_address_allocator.cc
	scuba/runtime_service/iso8601_timestamp.cc
	scuba/runtime_service/naming_scheme.cc
	scuba/runtime_service/pod_sandbox.cc
	scuba/runtime_service/program_main.cc
	scuba/runtime_service/runtime_service.cc
	scuba/runtime_service/yaml_argdata_factory.cc
	scuba/runtime_service/yaml_file_descriptor_factory.cc
)

target_link_libraries(
	scuba_runtime_service
	Threads::Threads
	arpc
	cares
	gpr
	grpc++_unsecure
	grpc_unsecure
	protobuf
	yaml-cpp
	z
)

install(
	TARGETS scuba_runtime_service
	DESTINATION ${CMAKE_INSTALL_BINDIR}
)
