// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <program.h>
#include <stdio.h>

#include <cstdlib>
#include <memory>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <grpc++/grpc++.h>

#include <scuba/runtime_service/configuration.ad.h>
#include <scuba/runtime_service/ip_address_allocator.h>
#include <scuba/runtime_service/runtime_service.h>
#include <scuba/util/grpc_connection_injector.h>

using arpc::ArgdataParser;
using arpc::ClientContext;
using arpc::FileDescriptor;
using arpc::Status;
using flower::protocol::switchboard::ServerStartRequest;
using flower::protocol::switchboard::ServerStartResponse;
using flower::protocol::switchboard::Switchboard;
using scuba::runtime_service::Configuration;
using scuba::runtime_service::IPAddressAllocator;
using scuba::runtime_service::RuntimeService;
using scuba::util::GrpcConnectionInjector;

void program_main(const argdata_t* ad) {
  Configuration configuration;
  ArgdataParser argdata_parser;
  configuration.Parse(*ad, &argdata_parser);

  // Enable logging of failed assertions.
  const std::shared_ptr<FileDescriptor>& logger_output =
      configuration.logger_output();
  if (logger_output) {
    FILE* fp = fdopen(logger_output->get(), "w");
    if (fp != nullptr) {
      setvbuf(fp, nullptr, _IONBF, 0);
      fswap(fp, stderr);
    }
  }

  // Extract file descriptors.
  const std::shared_ptr<FileDescriptor>& cri_switchboard_handle_fd =
      configuration.cri_switchboard_handle();
  if (!cri_switchboard_handle_fd)
    std::exit(1);
  std::unique_ptr<Switchboard::Stub> cri_switchboard_handle =
      Switchboard::NewStub(CreateChannel(cri_switchboard_handle_fd));
  const std::shared_ptr<FileDescriptor>& image_directory =
      configuration.image_directory();
  if (!image_directory)
    std::exit(1);
  const std::shared_ptr<FileDescriptor>& root_directory =
      configuration.root_directory();
  if (!root_directory)
    std::exit(1);
  const std::shared_ptr<FileDescriptor>& containers_switchboard_handle_fd =
      configuration.containers_switchboard_handle();
  if (!containers_switchboard_handle_fd)
    std::exit(1);
  std::unique_ptr<Switchboard::Stub> containers_switchboard_handle =
      Switchboard::NewStub(CreateChannel(containers_switchboard_handle_fd));

  // Start the CRI service using GRPC.
  IPAddressAllocator ip_address_allocator;
  RuntimeService runtime_service(root_directory.get(), image_directory.get(),
                                 containers_switchboard_handle.get(),
                                 &ip_address_allocator);
  grpc::ServerBuilder cri_builder;
  cri_builder.RegisterService(&runtime_service);
  std::unique_ptr<grpc::Server> cri_server(cri_builder.BuildAndStart());
  if (!cri_server)
    std::exit(1);

  // Listen for incoming connections for the CRI service.
  ClientContext context;
  ServerStartRequest request;
  ServerStartResponse response;
  if (Status status =
          cri_switchboard_handle->ServerStart(&context, request, &response);
      !status.ok())
    std::exit(1);
  if (!response.server())
    std::exit(1);

  // Forward incoming connections to GRPC.
  GrpcConnectionInjector injector(cri_server.get());
  arpc::ServerBuilder injector_builder(response.server());
  injector_builder.RegisterService(&injector);
  std::unique_ptr<arpc::Server> injector_server(injector_builder.Build());
  while (injector_server->HandleRequest() == 0) {
  }
  std::exit(1);
}
