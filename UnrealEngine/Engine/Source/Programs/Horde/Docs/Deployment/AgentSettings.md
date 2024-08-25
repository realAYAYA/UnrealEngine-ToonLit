[Horde](../../README.md) > [Deployment](../Deployment.md) > [Agent](Agent.md) > appsettings.json (Agent)

# appsettings.json (Agent)

All Horde-specific settings are stored in a root object called `Horde`. Other .NET functionality may be configured using properties in the root of this file.

Name | Description
---- | -----------
`serverProfiles` | `string` `->` [`ServerProfile`](#serverprofile)<br>Known servers to connect to
`server` | `string`<br>The default server, unless overridden from the command line
`name` | `string`<br>Name of agent to report as when connecting to server. By default, the computer's hostname will be used.
`installed` | `boolean`<br>Whether the server is running in 'installed' mode. In this mode, on Windows, the default data directory will use the common application data folder (C:\ProgramData\Epic\Horde), and configuration data will be read from here and the registry. This setting is overridden to false for local builds from appsettings.Local.json.
`ephemeral` | `boolean`<br>Whether agent should register as being ephemeral. Doing so will not persist any long-lived data on the server and once disconnected it's assumed to have been deleted permanently. Ideal for short-lived agents, such as spot instances on AWS EC2.
`executor` | `string`<br>The executor to use for jobs
`localExecutor` | [`LocalExecutorSettings`](#localexecutorsettings)<br>Settings for the local executor
`perforceExecutor` | [`PerforceExecutorSettings`](#perforceexecutorsettings)<br>Settings for the perforce executor
`workingDir` | [`DirectoryReference`](#directoryreference)<br>Working directory
`shareMountingEnabled` | `boolean`<br>Whether to mount the specified list of network shares
`shares` | [`MountNetworkShare`](#mountnetworkshare)`[]`<br>List of network shares to mount
`processNamesToTerminate` | `string[]`<br>List of process names to terminate after a job
`processesToTerminate` | [`ProcessToTerminate`](#processtoterminate)`[]`<br>List of process names to terminate after a lease completes, but not after a job step
`wineExecutablePath` | `string`<br>Path to Wine executable. If null, execution under Wine is disabled
`containerEngineExecutablePath` | `string`<br>Path to container engine executable, such as /usr/bin/podman. If null, execution of compute workloads inside a container is disabled
`writeStepOutputToLogger` | `boolean`<br>Whether to write step output to the logging device
`enableAwsEc2Support` | `boolean`<br>Queries information about the current agent through the AWS EC2 interface
`useLocalStorageClient` | `boolean`<br>Option to use a local storage client rather than connecting through the server. Primarily for convenience when debugging / iterating locally.
`computePort` | `integer`<br>Incoming port for listening for compute work. Needs to be tied with a lease.
`enableTelemetry` | `boolean`<br>Whether to send telemetry back to Horde server
`telemetryReportInterval` | `integer`<br>How often to report telemetry events to server in milliseconds
`bundleCacheSize` | `integer`<br>Maximum size of the bundle cache, in megabytes.
`properties` | `string` `->` `string`<br>Key/value properties in addition to those set internally by the agent

## ServerProfile

Information about a server to use

Name | Description
---- | -----------
`name` | `string`<br>Name of this server profile
`environment` | `string`<br>Name of the environment (currently just used for tracing)
`url` | `string`<br>Url of the server
`token` | `string`<br>Bearer token to use to initiate the connection
`thumbprint` | `string`<br>Thumbprint of a certificate to trust. Allows using self-signed certs for the server.
`thumbprints` | `string[]`<br>Thumbprints of certificates to trust. Allows using self-signed certs for the server.

## LocalExecutorSettings

Settings for the local executor

Name | Description
---- | -----------
`workspaceDir` | `string`<br>Path to the local workspace to use with the local executor
`runSteps` | `boolean`<br>Whether to actually execute steps, or just do job setup

## PerforceExecutorSettings

Settings for the perforce executor

Name | Description
---- | -----------
`runConform` | `boolean`<br>Whether to run conform jobs

## DirectoryReference

Name | Description
---- | -----------
`parentDirectory` | [`DirectoryReference`](#directoryreference)<br>
`fullName` | `string`<br>

## MountNetworkShare

Describes a network share to mount

Name | Description
---- | -----------
`mountPoint` | `string`<br>Where the share should be mounted on the local machine. Must be a drive letter for Windows.
`remotePath` | `string`<br>Path to the remote resource

## ProcessToTerminate

Specifies a process to terminate

Name | Description
---- | -----------
`name` | `string`<br>Name of the process
`when` | [`TerminateCondition`](#terminatecondition-enum)`[]`<br>When to terminate this process

## TerminateCondition (Enum)

Flags for processes to terminate

Name | Description
---- | -----------
`None` | Not specified; terminate in all circumstances
`BeforeSession` | When a session starts
`BeforeConform` | Before running a conform
`BeforeBatch` | Before executing a batch
`AfterBatch` | Terminate at the end of a batch
`AfterStep` | After a step completes
