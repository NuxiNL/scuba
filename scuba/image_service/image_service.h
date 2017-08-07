// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_IMAGE_SERVICE_IMAGE_SERVICE_H
#define SCUBA_IMAGE_SERVICE_IMAGE_SERVICE_H

#include <string_view>

#include <arpc++/arpc++.h>
#include <grpc++/grpc++.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h>

namespace scuba {
namespace image_service {

class ImageService final : public runtime::ImageService::Service {
 public:
  ImageService(const arpc::FileDescriptor* image_directory)
      : image_directory_(image_directory) {
  }

  grpc::Status ListImages(grpc::ServerContext* context,
                          const runtime::ListImagesRequest* request,
                          runtime::ListImagesResponse* response) override;
  grpc::Status ImageStatus(grpc::ServerContext* context,
                           const runtime::ImageStatusRequest* request,
                           runtime::ImageStatusResponse* response) override;
  grpc::Status PullImage(grpc::ServerContext* context,
                         const runtime::PullImageRequest* request,
                         runtime::PullImageResponse* response) override;
  grpc::Status RemoveImage(grpc::ServerContext* context,
                           const runtime::RemoveImageRequest* request,
                           runtime::RemoveImageResponse* response) override;
  grpc::Status ImageFsInfo(grpc::ServerContext* context,
                           const runtime::ImageFsInfoRequest* request,
                           runtime::ImageFsInfoResponse* response) override;

 private:
  const arpc::FileDescriptor* const image_directory_;

  // Returns whether the name of an image corresponds with a locally
  // stored image, i.e., it is named "sha256:....".
  static bool IsLocalImageName_(std::string_view image_name);

  ImageService(ImageService&) = delete;
  void operator=(ImageService) = delete;
};

}  // namespace image_service
}  // namesapce scuba

#endif
