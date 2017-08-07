#!/bin/sh

set -ex

rm -Rf tmp
mkdir -p tmp/scuba/runtime_service
find contrib -name '*.proto' | while read file; do
  protoc \
    -Icontrib \
    -I/usr/local/x86_64-unknown-cloudabi/include \
    --cpp_out=tmp \
    "${file}"
  protoc \
    -Icontrib \
    -I/usr/local/x86_64-unknown-cloudabi/include \
    --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin \
    --grpc_out=tmp \
    "${file}"
done
/usr/local/x86_64-unknown-cloudabi/bin/aprotoc \
    < scuba/runtime_service/configuration.proto \
    > tmp/scuba/runtime_service/configuration.ad.h

x86_64-unknown-cloudabi-c++ \
  -Wall -Werror \
  -std=c++1z -O2 \
  -Itmp \
  -I. \
  $(find scuba tmp -name '*.cc') -o scuba_runtime_service \
  -larpc -lgrpc++_unsecure -lgrpc_unsecure -lgpr -lcares -lprotobuf \
  -lpthread -lyaml-cpp -lz

find scuba -name '*.cc' -o -name '*.h' -exec clang-format -i {} +
