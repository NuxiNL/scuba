// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <string_view>

#include <grpc++/grpc++.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h>

#include <scuba/image_service/image_service.h>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using runtime::Image;
using runtime::ImageFsInfoRequest;
using runtime::ImageFsInfoResponse;
using runtime::ImageStatusRequest;
using runtime::ImageStatusResponse;
using runtime::ListImagesRequest;
using runtime::ListImagesResponse;
using runtime::PullImageRequest;
using runtime::PullImageResponse;
using runtime::RemoveImageRequest;
using runtime::RemoveImageResponse;
using scuba::image_service::ImageService;

namespace {

class DirDeleter {
 public:
  void operator()(DIR* directory) const {
    if (directory != nullptr)
      closedir(directory);
  }
};

}  // namespace

Status ImageService::ListImages(ServerContext* context,
                                const ListImagesRequest* request,
                                ListImagesResponse* response) {
  std::unique_ptr<DIR, DirDeleter> directory(
      opendirat(image_directory_->get(), "."));
  if (!directory)
    return {StatusCode::INTERNAL, std::strerror(errno)};

  for (dirent* entry = readdir(directory.get()); entry != nullptr;
       entry = readdir(directory.get())) {
    // TODO(ed): Respect filter.
    if (IsLocalImageName_(entry->d_name)) {
      stat sb;
      if (fstatat(image_directory_->get(), entry->d_name, &sb,
                  AT_SYMLINK_NOFOLLOW) == 0 &&
          S_ISREG(sb.st_mode)) {
        Image* image = response->add_images();
        image->set_id(entry->d_name);
        // TODO(ed): Set repo_tags.
        image->set_size(sb.st_size);
      }
    } else {
      // Filename doesn't match a supported image name pattern. It
      // could be a temporary file left by the image downloading
      // process. Remove it if it's too old, as it's likely stale.
      // TODO(ed): Implement.
    }
  }
  return Status::OK;
}

Status ImageService::ImageStatus(ServerContext* context,
                                 const ImageStatusRequest* request,
                                 ImageStatusResponse* response) {
  const std::string& image_name = request->image().image();
  if (!IsLocalImageName_(image_name)) {
    // TODO(ed): Implement.
    return {StatusCode::UNIMPLEMENTED, "ImageStatus by URL not implemented"};
  }

  stat sb;
  if (fstatat(image_directory_->get(), image_name.c_str(), &sb,
              AT_SYMLINK_NOFOLLOW) == 0 &&
      S_ISREG(sb.st_mode)) {
    Image* image = response->mutable_image();
    image->set_id(image_name);
    // TODO(ed): Set repo_tags.
    image->set_size(sb.st_size);
  }
  return Status::OK;
}

Status ImageService::PullImage(ServerContext* context,
                               const PullImageRequest* request,
                               PullImageResponse* response) {
  const std::string& image_name = request->image().image();
  if (IsLocalImageName_(image_name)) {
    return {StatusCode::INVALID_ARGUMENT,
            "Images can only be pulled by URL, not by checksum. Try placing "
            "the image in the image directory manually."};
  }
  return {StatusCode::UNIMPLEMENTED, "PullImage by URL not implemented"};
}

Status ImageService::RemoveImage(ServerContext* context,
                                 const RemoveImageRequest* request,
                                 RemoveImageResponse* response) {
  const std::string& image_name = request->image().image();
  if (!IsLocalImageName_(image_name)) {
    // TODO(ed): Look up canonical name.
    return {StatusCode::UNIMPLEMENTED, "RemoveImage by URL not implemented"};
  }

  if (unlinkat(image_directory_->get(), image_name.c_str(), 0) != 0 &&
      errno != ENOENT)
    return {StatusCode::INTERNAL, std::strerror(errno)};
  return Status::OK;
}

Status ImageService::ImageFsInfo(ServerContext* context,
                                 const ImageFsInfoRequest* request,
                                 ImageFsInfoResponse* response) {
  return {StatusCode::UNIMPLEMENTED, "ImageFsInfo not implemented"};
}

bool ImageService::IsLocalImageName_(std::string_view image_name) {
  return image_name.size() == 71 && image_name.substr(0, 7) == "sha256:" &&
         std::all_of(image_name.begin() + 7, image_name.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
