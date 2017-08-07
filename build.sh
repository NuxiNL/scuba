#!/bin/sh

set -ex

rm -Rf tmp
mkdir tmp

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

for i in image_service runtime_service; do
  mkdir -p tmp/scuba/${i}
  /usr/local/x86_64-unknown-cloudabi/bin/aprotoc \
      < scuba/${i}/configuration.proto \
      > tmp/scuba/${i}/configuration.ad.h

  x86_64-unknown-cloudabi-c++ \
    -Wall -Werror \
    -std=c++1z -O2 \
    -Itmp \
    -I. \
    $(find scuba/${i} tmp -name '*.cc') -o scuba_${i}\
    -larpc -lgrpc++_unsecure -lgrpc_unsecure -lgpr -lcares -lprotobuf \
    -lpthread -lyaml-cpp -lz
done

find scuba \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} +
