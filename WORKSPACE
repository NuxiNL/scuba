workspace(name = "com_github_nuxinl_scuba")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "com_github_nuxinl_argdata",
    commit = "6299455171a28831876d078c59a6634de6f6700b",
    remote = "https://github.com/NuxiNL/argdata.git",
)

git_repository(
    name = "com_github_nuxinl_arpc",
    commit = "58e54234bb0e493b9d3aca5ddc5f1e216083588e",
    remote = "https://github.com/NuxiNL/arpc.git",
)

git_repository(
    name = "com_github_nuxinl_bazel_toolchains_cloudabi",
    commit = "ecefb7d0ebdd8e02ae8acb4992930cab9c28188a",
    remote = "https://github.com/NuxiNL/bazel-toolchains-cloudabi.git",
)

git_repository(
    name = "com_github_nuxinl_bazel_third_party",
    commit = "dc13aa17e7f71d5ed24fc647f90f761a6a86fd70",
    remote = "https://github.com/NuxiNL/bazel-third-party.git",
)

git_repository(
    name = "com_github_nuxinl_flower",
    commit = "2c9c423791a55c7ed68c687858bacc344eda5676",
    remote = "https://github.com/NuxiNL/flower.git",
)

git_repository(
    name = "com_github_nuxinl_yaml2argdata",
    commit = "7c4026fe5a45f2dfdaedddb45ab1c0db168eaa97",
    remote = "https://github.com/NuxiNL/yaml2argdata.git",
)

git_repository(
    name = "io_bazel_rules_python",
    commit = "e6399b601e2f72f74e5aa635993d69166784dde1",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@com_github_nuxinl_bazel_third_party//:third_party.bzl", "third_party_repositories")

third_party_repositories()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

# TODO(ed): Remove the dependencies below once Bazel supports transitive
# dependency loading. These are from ARPC.
# https://github.com/bazelbuild/proposals/blob/master/designs/2018-11-07-design-recursive-workspaces.md

load("@io_bazel_rules_python//python:pip.bzl", "pip_import", "pip_repositories")

pip_repositories()

pip_import(
    name = "aprotoc_deps",
    requirements = "@com_github_nuxinl_arpc//scripts:requirements.txt",
)

load("@aprotoc_deps//:requirements.bzl", "pip_install")

pip_install()
