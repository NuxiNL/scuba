// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <time.h>

#include <iomanip>
#include <ostream>

#include <scuba/runtime_service/iso8601_timestamp.h>

using scuba::runtime_service::ISO8601Timestamp;

ISO8601Timestamp::ISO8601Timestamp() {
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  localtime_r(&ts.tv_sec, &tm_);
  tv_nsec_ = ts.tv_nsec;
}

namespace scuba {
namespace runtime_service {

std::ostream& operator<<(std::ostream& stream, const ISO8601Timestamp& time) {
  stream << std::put_time(&time.tm_, "%FT%T") << '.' << std::setfill('0')
         << std::setw(9) << time.tv_nsec_ << 'Z';
  return stream;
}

}  // namespace runtime_service
}  // namespace scuba
