// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_IP_ADDRESS_ALLOCATOR_H
#define SCUBA_RUNTIME_SERVICE_IP_ADDRESS_ALLOCATOR_H

#include <random>
#include <set>
#include <string>
#include <string_view>

namespace scuba {
namespace runtime_service {

class IPAddressAllocator;

class IPAddressLease {
 public:
  IPAddressLease() : allocator_(nullptr) {
  }

  IPAddressLease(IPAddressAllocator* allocator, std::uint32_t address)
      : allocator_(allocator), address_(address) {
  }

  IPAddressLease(IPAddressLease&& lease);
  IPAddressLease& operator=(IPAddressLease&& lease);
  ~IPAddressLease();

  std::string GetString() const;

 private:
  IPAddressAllocator* allocator_;
  std::uint32_t address_;

  IPAddressLease(const IPAddressLease&) = delete;
  IPAddressLease& operator=(const IPAddressLease&) = delete;
};

class IPAddressAllocator {
 public:
  IPAddressAllocator() : first_(1), last_(0) {
  }

  bool SetRange(std::string_view range);
  IPAddressLease Allocate();
  void Deallocate(std::uint32_t address);

 private:
  std::uint32_t first_;  // First allocatable address.
  std::uint32_t last_;   // Last allocatable address.

  std::random_device random_device_;  // Random address picker.
  std::set<std::uint32_t> used_;      // Addresses currently in use.

  IPAddressAllocator(IPAddressAllocator&) = delete;
  void operator=(IPAddressAllocator) = delete;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
