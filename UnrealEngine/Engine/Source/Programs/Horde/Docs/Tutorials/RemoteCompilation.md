[Horde](../../README.md) > Getting Started: Remote Compilation

# Getting Started: Remote Compilation

## Introduction

Horde implements a platform for generic remote execution workloads, allowing clients to leverage idle CPU cycles on
other machines to accelerate workloads that would otherwise be executed locally. With Horde's remote execution platform,
you can issue explicit commands to remote agents sequentially, such as "upload these files", "run this process",
"send these files back", and so on.

**Unreal Build Accelerator** is a tool that implements lightweight virtualization for
third-party programs (such as C++ compilers), allowing it to run on a remote machine - requesting information from the
initiating machine as required. The remotely executed process behaves as if it's executing
on the local machine, seeing the same view of the file system and so on, and files are transferred to and from the
remote machine behind the scenes as necessary.

**Unreal Build Tool** can use Unreal Build Accelerator with Horde to offload compilation tasks to connected agents,
spreading the workload over multiple machines.

> **Note:** Unreal Build Accelerator only supports Windows in Unreal Engine 5.4. Support for Mac and Linux are planned for 
  a future release.

## Prerequisites

* Horde Server and one or more Horde Agents (see [Getting Started: Install Horde](InstallHorde.md)).
* A workstation with a UE project under development.
* Network connectivity between your workstation and Horde Agents on port range 7000-7010.

## Steps

1. On the machine initiating the build, ensure your UE project is synced and builds locally.
2. Configure UnrealBuildTool to use your Horde Server by updating
   `Engine/Saved/UnrealBuildTool/BuildConfiguration.xml` with the following:

   ```xml
   <?xml version="1.0" encoding="utf-8" ?>
   <Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">

       <BuildConfiguration>
           <!-- Enable support for UnrealBuildAccelerator -->
           <bAllowUBAExecutor>true</bAllowUBAExecutor>
       </BuildConfiguration>

       <Horde>
           <!-- Address of the Horde server -->
           <Server>http://{{ SERVER_HOST_NAME }}:13340</Server>

           <!-- Pool of machines to offload work to. Horde configures Win-UE5 by default. -->
           <WindowsPool>Win-UE5</WindowsPool>
       </Horde>

       <UnrealBuildAccelerator>
           <!-- Enable for visualizing UBA's progress (optional) -->
           <bLaunchVisualizer>true</bLaunchVisualizer>
       </UnrealBuildAccelerator>

   </Configuration>
   ```

   Replace `SERVER_HOST_NAME` with the address associated with your Horde server installation.

   * `BuildConfiguration.xml` can be sourced from many locations in the filesystem, depending on your preference,
     including locations typically under source control. See
     [Build Configuration](https://docs.unrealengine.com/en-US/build-configuration-for-unreal-engine/).
     in the UnrealBuildTool documentation for more details.

3. Compile your project through your IDE as normal. You should observe log lines such as:

   ```text
   [Worker0] Connected to AGENT-1 (10.0.10.172) under lease 65d48fe1eb6ff84c8197a9b0
   ...
   [17/5759] Compile [x64] Module.CoreUObject.2.cpp [RemoteExecutor: AGENT-1]
   ```

   This indicates work is being spread to multiple agents. If you enabled the UBA visualizer, you can also see
   a graphical overview of how the build progresses over multiple machines.

   For debugging and tuning purposes, it can be useful to force remote execution all compile workloads. To do
   so, enable the following option in your `BuildConfiguration.xml` file or pass `-UBAForceRemote` on the
   UnrealBuildTool command line:

   ```xml
   <UnrealBuildAccelerator>
       <bForceBuildAllRemote>true</bForceBuildAllRemote>
   </UnrealBuildAccelerator>
   ```

> **Note:** It is not recommended to run a Horde Agent on the same machine as the Horde Server for performance reasons.

> **Note:** When using Horde's build automation functionality, be mindful of mixing pools of agents for UBA and 
  pools of agents for build automation. Agents used for build automation typically have higher requirements 
  and are a more scarce resource than compute helpers.

