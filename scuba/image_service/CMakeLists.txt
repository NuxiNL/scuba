add_aprotoc(scuba/image_service/configuration)

add_executable(
	scuba_image_service
	github.com/gogo/protobuf/gogoproto/gogo.pb.cc
	github.com/gogo/protobuf/gogoproto/gogo.pb.h
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.cc
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.cc
	k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h
	scuba/image_service/configuration.ad.h
	scuba/image_service/image_service.cc
	scuba/image_service/program_main.cc
)

target_link_libraries(
	scuba_image_service
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
