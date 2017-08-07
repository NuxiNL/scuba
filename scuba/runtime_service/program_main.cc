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

#include <scuba/image_service/image_service.h>  // TODO(ed): Remove!
#include <scuba/runtime_service/configuration.ad.h>
#include <scuba/runtime_service/ip_address_allocator.h>
#include <scuba/runtime_service/runtime_service.h>

using arpc::ArgdataParser;
using arpc::FileDescriptor;
using flower::protocol::switchboard::Switchboard;
using grpc::Server;
using grpc::ServerBuilder;
using scuba::image_service::ImageService;
using scuba::runtime_service::Configuration;
using scuba::runtime_service::IPAddressAllocator;
using scuba::runtime_service::RuntimeService;

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
  const std::shared_ptr<FileDescriptor>& root_directory =
      configuration.root_directory();
  if (!root_directory)
    std::exit(1);
  const std::shared_ptr<FileDescriptor>& switchboard_servers_fd =
      configuration.switchboard_servers();
  if (!switchboard_servers_fd)
    std::exit(1);
  std::unique_ptr<Switchboard::Stub> switchboard_servers =
      Switchboard::NewStub(CreateChannel(switchboard_servers_fd));

  ServerBuilder builder;

  ImageService image_service(image_directory.get());
  builder.RegisterService(&image_service);

  IPAddressAllocator ip_address_allocator;
  RuntimeService runtime_service(root_directory.get(), image_directory.get(),
                                 switchboard_servers.get(),
                                 &ip_address_allocator);
  builder.RegisterService(&runtime_service);

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
