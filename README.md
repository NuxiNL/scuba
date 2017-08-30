# Scuba: A Kubernetes CRI for launching CloudABI processes

[Kubernetes](https://kubernetes.io/) is an Open Source cluster
management suite. By default, it can be used to distributed Docker-based
containers across a cluster of Linux servers. As of version 1.5,
Kubernetes has turned its coupling with Docker into a separate protocol,
called the Container Runtime Interface (CRI). This allows Kubernetes to
interface with other container engines, such as rkt, OCI, etc.

Scuba (contraction of 'Secure Kubernetes') is a daemon that implements
the Container Runtime Interface, using [GRPC](https://grpc.io/). Instead
of spawning containers, it simply launches
[CloudABI](https://nuxi.nl/cloudabi/) processes. This allows you to run
one or more nodes in a Kubernetes cluster capable of running strongly
sandboxed applications. These jobs don't depend on network policies or
key exchange to be secure. Instead, pods are simply able to connect to
services if they are configured through their pod specification to do
so.

# Using Scuba

Scuba is still under heavy development, so documentation on using it is
still absent. Stay tuned for updates in the nearby future!
