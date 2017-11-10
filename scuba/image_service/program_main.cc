// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <program.h>
#include <stdio.h>

#include <cstdlib>
#include <memory>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <grpc++/grpc++.h>

#include <scuba/image_service/configuration.ad.h>
#include <scuba/image_service/image_service.h>
#include <scuba/util/grpc_connection_injector.h>

using arpc::ArgdataParser;
using arpc::ClientContext;
using arpc::FileDescriptor;
using arpc::Status;
using flower::protocol::switchboard::ServerStartRequest;
using flower::protocol::switchboard::ServerStartResponse;
using flower::protocol::switchboard::Switchboard;
using scuba::image_service::Configuration;
using scuba::image_service::ImageService;
using scuba::util::GrpcConnectionInjector;

void program_main(const argdata_t* ad) {
  Configuration configuration;
  ArgdataParser argdata_parser;
  configuration.Parse(*ad, &argdata_parser);

  // Enable logging of failed assertions.
  if (const std::shared_ptr<FileDescriptor>& logger_output =
          configuration.logger_output();
      logger_output) {
    FILE* fp = fdopen(logger_output->get(), "w");
    if (fp != nullptr) {
      setvbuf(fp, nullptr, _IONBF, 0);
      fswap(fp, stderr);
      fclose(fp);
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

  // Start the CRI service using GRPC.
  ImageService image_service(image_directory.get());
  grpc::ServerBuilder cri_builder;
  cri_builder.RegisterService(&image_service);
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
