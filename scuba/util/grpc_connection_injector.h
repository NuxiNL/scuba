// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCUBA_UTIL_GRPC_CONNECTION_INJECTOR_H
#define SCUBA_UTIL_GRPC_CONNECTION_INJECTOR_H

#include <fcntl.h>

#include <arpc++/arpc++.h>
#include <flower/protocol/server.ad.h>
#include <grpc++/grpc++.h>

namespace scuba {
namespace util {

// Bridge for accepting incoming connections from Flower and passing
// them on to an existing GRPC server instance.
class GrpcConnectionInjector
    : public flower::protocol::server::Server::Service {
 public:
  explicit GrpcConnectionInjector(grpc::Server* server) : server_(server) {
  }

  arpc::Status Connect(arpc::ServerContext* context,
                       const flower::protocol::server::ConnectRequest* request,
                       flower::protocol::server::ConnectResponse* response) {
    if (const std::shared_ptr<arpc::FileDescriptor>& fd = request->client();
        fd) {
      // Duplicate the file descriptor, as GRPC wants to take ownership.
      // The file descriptor provided by ARPC cannot be taken over.
      if (int nfd = dup(fd->get()); nfd >= 0) {
        fcntl(nfd, F_SETFL, fcntl(nfd, F_GETFL) | O_NONBLOCK);
        grpc::AddInsecureChannelFromFd(server_, nfd);
      }
    }
    return arpc::Status::OK;
  }

 private:
  grpc::Server* server_;
};

}  // namespace util
}  // namespace scuba

#endif
