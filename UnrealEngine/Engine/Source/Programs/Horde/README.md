# Horde

**Horde** is a scalable, cloud-ready, distributed build framework and content-addressable storage platform 
designed for Unreal Engine. 

Goals of the project include:

* Remote execution of arbitrary workloads, including compilation of source code and building assets (like SN-DBS, 
  FastBuild, IncrediBuild et al).
* Storage of bulk data for source assets.
* Caching of assets and build artifacts through Unreal Engine's DDC system.
* Storage for final build artifacts (eg. packaged builds).
* Replication of data between studios on a planet scale.
* A CI/CD system for teams to test, build and publish their projects.

Horde is written primarily in C# (NET Core), and full source is included with Unreal Engine. The server can be run 
on Windows or Linux, via local installations or through containers.

### Status

As of writing, Horde is in heavy use within Epic as an CI/CD system, and published builds of Unreal Engine and 
Fortnite have been made by Horde since late 2020. Other applications listed above are still in heavy development 
on top of the core framework.

We currently **do not recommend Horde for production use** by other teams. The source code is provided as-is, and
we do not support external deployments of it. We hope to make it fully available for other teams in a future engine
release, with a streamlined setup experience and complete documentation.

### FAQ

#### Why are all these use cases being muddled together?

Most of our target use cases are traditionally handled as distinct components, but bringing them all together
gives us many opportunities for optimization:

* Storage is a key component of any data pipeline for caching. 
* Remote execution needs data close to compute nodes where it can be retrieved quickly.
* Scalable build automation systems can make use of the same scheduling, management tools and auto-scaling
  functionality as a remote execution platform, and require a storage backend for intermediate and final
  artifacts.

We want Unreal Engine to allow developers to focus on making awesome products, and fast, reliable iteration is a 
key component of that. Sharing battle-tested infrastructure that works seamlessly with the engine reduces the 
barrier to entry for other teams.

#### Why would I use Horde rather than some other off-the-shelf build automation system, like Jenkins or TeamCity?

Horde is built from the ground up to support development of Unreal Engine projects.

While it is possible to customize a more generic build automation tool, Horde is built to serve the specific needs of UE
developers - supporting heavy throughput, easy parallelism, integration with tools like Unreal Editor and UnrealGameSync,
and with richer, more context-aware interface choices.

#### Will I need to deploy Horde to use Unreal Engine?

No, Horde will not required to use Unreal Engine. We have been developing with an eye to Epic's needs and believe 
it can provide similar benefits to others.

### Further Reading

* [Horde Build](Horde.Build/README.md)
* [Horde Storage](HordeStorage/README.md)
