workspace(name = "org_cloudabi_scuba")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "org_cloudabi_argdata",
    commit = "6299455171a28831876d078c59a6634de6f6700b",
    remote = "https://github.com/NuxiNL/argdata.git",
)

git_repository(
    name = "org_cloudabi_arpc",
    commit = "81305e311c0559fe7a64a98ce8ac1aa7051c7a4d",
    remote = "https://github.com/NuxiNL/arpc.git",
)

git_repository(
    name = "org_cloudabi_bazel_toolchains_cloudabi",
    commit = "6bafd065d82909c9a5db4b8ef6619855ac3408f1",
    remote = "https://github.com/NuxiNL/bazel-toolchains-cloudabi.git",
)

git_repository(
    name = "org_cloudabi_bazel_third_party",
    commit = "8d51abda2299d5fe26ca7c55f182b6562f440979",
    remote = "https://github.com/NuxiNL/bazel-third-party.git",
)

git_repository(
    name = "org_cloudabi_flower",
    commit = "f23a1bfc9a1d3f16920b9183b967a057d8a21183",
    remote = "https://github.com/NuxiNL/flower.git",
)

git_repository(
    name = "org_cloudabi_yaml2argdata",
    commit = "678be75c7a9bb23c80a8de4bbbccf26bba9570aa",
    remote = "https://github.com/NuxiNL/yaml2argdata.git",
)

git_repository(
    name = "io_bazel_rules_python",
    commit = "e6399b601e2f72f74e5aa635993d69166784dde1",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@org_cloudabi_bazel_toolchains_cloudabi//:toolchains.bzl", "toolchains_cloudabi_dependencies")

toolchains_cloudabi_dependencies()

load("@org_cloudabi_bazel_third_party//:third_party.bzl", "third_party_repositories")

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
    requirements = "@org_cloudabi_arpc//scripts:requirements.txt",
)

load("@aprotoc_deps//:requirements.bzl", "pip_install")

pip_install()
