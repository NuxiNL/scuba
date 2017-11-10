// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <scuba/runtime_service/ip_address_allocator.h>

using scuba::runtime_service::IPAddressAllocator;
using scuba::runtime_service::IPAddressLease;

IPAddressLease::IPAddressLease(IPAddressLease&& lease) {
  allocator_ = lease.allocator_;
  address_ = lease.address_;
  lease.allocator_ = nullptr;
}

IPAddressLease::~IPAddressLease() {
  if (allocator_ != nullptr)
    allocator_->Deallocate(address_);
}

IPAddressLease& IPAddressLease::operator=(IPAddressLease&& lease) {
  if (allocator_ != nullptr)
    allocator_->Deallocate(address_);
  allocator_ = lease.allocator_;
  address_ = lease.address_;
  lease.allocator_ = nullptr;
  return *this;
}

std::string IPAddressLease::GetString() const {
  assert(allocator_ != nullptr && "Attempted to access stale address");
  std::ostringstream ss;
  ss << (address_ >> 24) << '.' << (address_ >> 16 & 0xff) << '.'
     << (address_ >> 8 & 0xff) << '.' << (address_ & 0xff);
  return ss.str();
}

bool IPAddressAllocator::SetRange(std::string_view range) {
  // Parse an IPv4 address with prefix length.
  // TODO(ed): Support IPv6 as soon as Kubernetes does as well.
  // TODO(ed): Is there a way to avoid copying?
  std::string range_copy(range);
  std::istringstream ss(range_copy);
  std::uint32_t a, b, c, d;
  unsigned int prefixlen;
  char s1, s2, s3, s4;
  ss >> std::noskipws >> a >> s1 >> b >> s2 >> c >> s3 >> d >> s4 >> prefixlen;
  if (!ss.eof() || ss.fail() || a > 255 || s1 != '.' || b > 255 || s2 != '.' ||
      c > 255 || s3 != '.' || d > 255 || s4 != '/' || prefixlen > 32)
    return false;

  if (prefixlen > 30) {
    // Prefix length of 31 or 32, meaning there are no network and
    // broadcast addresses.
    std::uint32_t mask = std::uint32_t(~0) << (32 - prefixlen);
    std::uint32_t base = (a << 24 | b << 16 | c << 8 | d) & mask;
    first_ = base;
    last_ = base | ~mask;
  } else {
    // Prefix length of 30 or less, meaning we should keep the network
    // and broadcast addresses free.
    std::uint32_t mask = ~(std::uint32_t(~0) >> prefixlen);
    std::uint32_t base = (a << 24 | b << 16 | c << 8 | d) & mask;
    first_ = base | 0x00000001;
    last_ = base | (0xfffffffe & ~mask);
  }
  return true;
}

IPAddressLease IPAddressAllocator::Allocate() {
  if (first_ > last_)
    throw std::runtime_error("No IP address range configured");

  // First try to allocate an address at random.
  std::uint32_t address;
  std::uniform_int_distribution<std::uint32_t> dist(first_, last_);
  for (int i = 0; i < 100; ++i) {
    address = dist(random_device_);
    if (used_.find(address) == used_.end()) {
      used_.insert(address);
      return IPAddressLease(this, address);
    }
  }

  // Fall back to doing a full sweep of the address range.
  for (address = first_; address >= first_ && address <= last_; ++address) {
    if (used_.find(address) == used_.end()) {
      used_.insert(address);
      return IPAddressLease(this, address);
    }
  }
  throw std::runtime_error("No unused IP addresses available");
}

void IPAddressAllocator::Deallocate(std::uint32_t address) {
  used_.erase(address);
}
