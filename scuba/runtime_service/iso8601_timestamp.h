// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCUBA_RUNTIME_SERVICE_ISO8601_TIMESTAMP_H
#define SCUBA_RUNTIME_SERVICE_ISO8601_TIMESTAMP_H

#include <ctime>
#include <ostream>

namespace scuba {
namespace runtime_service {

class ISO8601Timestamp {
 public:
  ISO8601Timestamp();

  friend std::ostream& operator<<(std::ostream& stream,
                                  const ISO8601Timestamp& time);

 private:
  std::tm tm_;
  long tv_nsec_;
};

std::ostream& operator<<(std::ostream& stream, const ISO8601Timestamp& time);

}  // namespace runtime_service
}  // namespace scuba

#endif
