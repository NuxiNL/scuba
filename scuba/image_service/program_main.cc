// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <sys/socket.h>

#include <fcntl.h>
#include <program.h>

#include <cstdlib>
#include <memory>

#include <arpc++/arpc++.h>
#include <grpc++/grpc++.h>

#include <scuba/image_service/configuration.ad.h>
#include <scuba/image_service/image_service.h>

using arpc::ArgdataParser;
using arpc::FileDescriptor;
using grpc::Server;
using grpc::ServerBuilder;
using scuba::image_service::Configuration;
using scuba::image_service::ImageService;

void program_main(const argdata_t* ad) {
  Configuration configuration;
  ArgdataParser argdata_parser;
  configuration.Parse(*ad, &argdata_parser);

  const std::shared_ptr<FileDescriptor>& cri_socket =
      configuration.cri_socket();
  if (!cri_socket)
    std::exit(1);
  const std::shared_ptr<FileDescriptor>& image_directory =
      configuration.image_directory();
  if (!image_directory)
    std::exit(1);

  ImageService image_service(image_directory.get());

  ServerBuilder builder;
  builder.RegisterService(&image_service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  if (!server)
    std::exit(1);

  for (;;) {
    int fd = accept(cri_socket->get(), nullptr, nullptr);
    if (fd < 0)
      std::exit(1);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    AddInsecureChannelFromFd(server.get(), fd);
  }
}
