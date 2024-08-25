[Horde](../../../README.md) > [Configuration](../../Config.md) > Globals.json

# Globals.json

Global configuration

Name | Description
---- | -----------
`version` | `integer`<br>Version number for the server. Values are indicated by the .
`include` | [`ConfigInclude`](#configinclude)`[]`<br>Other paths to include
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>Macros within the global scope
`dashboard` | [`DashboardConfig`](Dashboard.md)<br>Settings for the dashboard
`projects` | [`ProjectConfig`](Projects.md)`[]`<br>List of projects
`pools` | [`PoolConfig`](#poolconfig)`[]`<br>List of pools
`downtime` | [`ScheduledDowntime`](#scheduleddowntime)`[]`<br>List of scheduled downtime
`perforceClusters` | [`PerforceCluster`](#perforcecluster)`[]`<br>List of Perforce clusters
`software` | [`AgentSoftwareConfig`](#agentsoftwareconfig)`[]`<br>List of costs of a particular agent type
`rates` | [`AgentRateConfig`](#agentrateconfig)`[]`<br>List of costs of a particular agent type
`networks` | [`NetworkConfig`](#networkconfig)`[]`<br>List of networks
`compute` | [`ComputeClusterConfig`](#computeclusterconfig)`[]`<br>List of compute profiles
`secrets` | [`SecretConfig`](#secretconfig)`[]`<br>List of secrets
`devices` | [`DeviceConfig`](#deviceconfig)<br>Device configuration
`tools` | [`ToolConfig`](#toolconfig)`[]`<br>List of tools hosted by the server
`maxConformCount` | `integer`<br>Maximum number of conforms to run at once
`agentShutdownIfDisabledGracePeriod` | `string`<br>Time to wait before shutting down an agent that has been disabled Used if no value is set on the actual pool.
`storage` | [`StorageConfig`](#storageconfig)<br>Storage configuration
`artifactTypes` | [`ArtifactTypeConfig`](#artifacttypeconfig)`[]`<br>Configuration for different artifact types
`telemetryStores` | [`TelemetryStoreConfig`](Telemetry.md)`[]`<br>Metrics to aggregate on the Horde server
`parameters` | `object`<br>General parameters for other tools. Can be queried through the api/v1/parameters endpoint.
`acl` | [`AclConfig`](#aclconfig)<br>Access control list

## ConfigInclude

Directive to merge config data from another source

Name | Description
---- | -----------
`path` | `string`<br>Path to the config data to be included. May be relative to the including file's location.

## ConfigMacro

Declares a config macro

Name | Description
---- | -----------
`name` | `string`<br>Name of the macro property
`value` | `string`<br>Value for the macro property

## PoolConfig

Mutable configuration for a pool

Name | Description
---- | -----------
`id` | `string`<br>Unique id for this pool
`base` | `string`<br>Base pool config to copy settings from
`name` | `string`<br>Name of the pool
`condition` | `string`<br>Condition for agents to automatically be included in this pool
`properties` | `string` `->` `string`<br>Arbitrary properties related to this pool
`color` | [`PoolColor`](#poolcolor-enum)<br>Color to use for this pool on the dashboard
`enableAutoscaling` | `boolean`<br>Whether to enable autoscaling for this pool
`minAgents` | `integer`<br>The minimum number of agents to keep in the pool
`numReserveAgents` | `integer`<br>The minimum number of idle agents to hold in reserve
`conformInterval` | `string`<br>Interval between conforms. If zero, the pool will not conform on a schedule.
`scaleOutCooldown` | `string`<br>Cooldown time between scale-out events
`scaleInCooldown` | `string`<br>Cooldown time between scale-in events
`shutdownIfDisabledGracePeriod` | `string`<br>Time to wait before shutting down an agent that has been disabled
`sizeStrategy` | [`PoolSizeStrategy`](#poolsizestrategy-enum)<br>
`sizeStrategies` | [`PoolSizeStrategyInfo`](#poolsizestrategyinfo)`[]`<br>List of pool sizing strategies for this pool. The first strategy with a matching condition will be picked.
`fleetManagers` | [`FleetManagerInfo`](#fleetmanagerinfo)`[]`<br>List of fleet managers for this pool. The first strategy with a matching condition will be picked. If empty or no conditions match, a default fleet manager will be used.
`leaseUtilizationSettings` | [`LeaseUtilizationSettings`](#leaseutilizationsettings)<br>Settings for lease utilization pool sizing strategy (if used)
`jobQueueSettings` | [`JobQueueSettings`](#jobqueuesettings)<br>Settings for job queue pool sizing strategy (if used)
`computeQueueAwsMetricSettings` | [`ComputeQueueAwsMetricSettings`](#computequeueawsmetricsettings)<br>Settings for job queue pool sizing strategy (if used)

## PoolColor (Enum)

Color to use for labels of this pool

Name | Description
---- | -----------
`Default` | 
`Blue` | 
`Orange` | 
`Green` | 
`Gray` | 

## PoolSizeStrategy (Enum)

Available pool sizing strategies

Name | Description
---- | -----------
`LeaseUtilization` | Strategy based on lease utilization
`JobQueue` | Strategy based on size of job build queue
`NoOp` | No-op strategy used as fallback/default behavior
`ComputeQueueAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet
`LeaseUtilizationAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet

## PoolSizeStrategyInfo

Metadata for configuring and picking a pool sizing strategy

Name | Description
---- | -----------
`type` | [`PoolSizeStrategy`](#poolsizestrategy-enum)<br>Strategy implementation to use
`condition` | `string`<br>Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
`config` | `string`<br>Configuration for the strategy, serialized as JSON
`extraAgentCount` | `integer`<br>Integer to add after pool size has been calculated. Can also be negative.

## FleetManagerInfo

Metadata for configuring and picking a fleet manager

Name | Description
---- | -----------
`type` | [`FleetManagerType`](#fleetmanagertype-enum)<br>Fleet manager type implementation to use
`condition` | `string`<br>Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
`config` | `string`<br>Configuration for the strategy, serialized as JSON

## FleetManagerType (Enum)

Available fleet managers

Name | Description
---- | -----------
`Default` | Default fleet manager
`NoOp` | No-op fleet manager.
`Aws` | Fleet manager for handling AWS EC2 instances. Will create and/or terminate instances from scratch.
`AwsReuse` | Fleet manager for handling AWS EC2 instances. Will start already existing but stopped instances to reuse existing EBS disks.
`AwsRecycle` | Fleet manager for handling AWS EC2 instances. Will start already existing but stopped instances to reuse existing EBS disks.
`AwsAsg` | Fleet manager for handling AWS EC2 instances. Uses an EC2 auto-scaling group for controlling the number of running instances.

## LeaseUtilizationSettings

Lease utilization sizing settings for a pool

Name | Description
---- | -----------
`sampleTimeSec` | `integer`<br>Time period for each sample
`numSamples` | `integer`<br>Number of samples to collect for calculating lease utilization
`numSamplesForResult` | `integer`<br>Min number of samples for a valid result
`minAgents` | `integer`<br>The minimum number of agents to keep in the pool
`numReserveAgents` | `integer`<br>The minimum number of idle agents to hold in reserve

## JobQueueSettings

Job queue sizing settings for a pool

Name | Description
---- | -----------
`scaleOutFactor` | `number`<br>Factor translating queue size to additional agents to grow the pool with The result is always rounded up to nearest integer. Example: if there are 20 jobs in queue, a factor 0.25 will result in 5 new agents being added (20 * 0.25)
`scaleInFactor` | `number`<br>Factor by which to shrink the pool size with when queue is empty The result is always rounded up to nearest integer. Example: when the queue size is zero, a default value of 0.9 will shrink the pool by 10% (current agent count * 0.9)
`samplePeriodMin` | `integer`<br>How far back in time to look for job batches (that potentially are in the queue)
`readyTimeThresholdSec` | `integer`<br>Time spent in ready state before considered truly waiting for an agent<br>A job batch can be in ready state before getting picked up and executed. This threshold will help ensure only batches that have been waiting longer than this value will be considered.

## ComputeQueueAwsMetricSettings

Settings for

Name | Description
---- | -----------
`computeClusterId` | `string`<br>Compute cluster ID to observe
`namespace` | `string`<br>AWS CloudWatch namespace to write metrics in

## ScheduledDowntime

Settings for the maintenance window

Name | Description
---- | -----------
`startTime` | `string`<br>Start time
`finishTime` | `string`<br>Finish time
`frequency` | [`ScheduledDowntimeFrequency`](#scheduleddowntimefrequency-enum)<br>Frequency that the window repeats

## ScheduledDowntimeFrequency (Enum)

How frequently the maintence window repeats

Name | Description
---- | -----------
`Once` | Once
`Daily` | Every day
`Weekly` | Every week

## PerforceCluster

Information about a cluster of Perforce servers.

Name | Description
---- | -----------
`name` | `string`<br>Name of the cluster
`serviceAccount` | `string`<br>Username for Horde to log in to this server. Will use the first account specified below if not overridden.
`canImpersonate` | `boolean`<br>Whether the service account can impersonate other users
`supportsPartitionedWorkspaces` | `boolean`<br>Whether to use partitioned workspaces on this server
`servers` | [`PerforceServer`](#perforceserver)`[]`<br>List of servers
`credentials` | [`PerforceCredentials`](#perforcecredentials)`[]`<br>List of server credentials
`autoSdk` | [`AutoSdkWorkspace`](#autosdkworkspace)`[]`<br>List of autosdk streams

## PerforceServer

Information about an individual Perforce server

Name | Description
---- | -----------
`serverAndPort` | `string`<br>The server and port. The server may be a DNS entry with multiple records, in which case it will be actively load balanced.
`healthCheck` | `boolean`<br>Whether to query the healthcheck address under each server
`resolveDns` | `boolean`<br>Whether to resolve the DNS entries and load balance between different hosts
`maxConformCount` | `integer`<br>Maximum number of simultaneous conforms on this server
`condition` | `string`<br>Optional condition for a machine to be eligable to use this server
`properties` | `string[]`<br>List of properties for an agent to be eligable to use this server

## PerforceCredentials

Credentials for a Perforce user

Name | Description
---- | -----------
`userName` | `string`<br>The username
`password` | `string`<br>Password for the user
`ticket` | `string`<br>Login ticket for the user (will be used instead of password if set)

## AutoSdkWorkspace

Path to a platform and stream to use for syncing AutoSDK

Name | Description
---- | -----------
`name` | `string`<br>Name of this workspace
`properties` | `string[]`<br>The agent properties to check (eg. "OSFamily=Windows")
`userName` | `string`<br>Username for logging in to the server
`stream` | `string`<br>Stream to use

## AgentSoftwareConfig

Selects different agent software versions by evaluating a condition

Name | Description
---- | -----------
`toolId` | `string`<br>Tool identifier
`condition` | `string`<br>Condition for using this channel

## AgentRateConfig

Describes the monetary cost of agents matching a particular criteria

Name | Description
---- | -----------
`condition` | `string`<br>Condition string
`rate` | `number`<br>Rate for this agent

## NetworkConfig

Describes a network The ID describes any logical grouping, such as region, availability zone, rack or office location.

Name | Description
---- | -----------
`id` | `string`<br>ID for this network
`cidrBlock` | `string`<br>CIDR block
`description` | `string`<br>Human-readable description
`computeId` | `string`<br>Compute ID for this network (used when allocating compute resources)

## ComputeClusterConfig

Profile for executing compute requests

Name | Description
---- | -----------
`id` | `string`<br>Name of the partition
`namespaceId` | `string`<br>Name of the namespace to use
`requestBucketId` | `string`<br>Name of the input bucket
`responseBucketId` | `string`<br>Name of the output bucket
`condition` | `string`<br>Filter for agents to include
`acl` | [`AclConfig`](#aclconfig)<br>Access control list

## AclConfig

Parameters to update an ACL

Name | Description
---- | -----------
`entries` | [`AclEntryConfig`](#aclentryconfig)`[]`<br>Entries to replace the existing ACL
`profiles` | [`AclProfileConfig`](#aclprofileconfig)`[]`<br>Defines profiles which allow grouping sets of actions into named collections
`inherit` | `boolean`<br>Whether to inherit permissions from the parent ACL
`exceptions` | `string[]`<br>List of exceptions to the inherited setting

## AclEntryConfig

Individual entry in an ACL

Name | Description
---- | -----------
`claim` | [`AclClaimConfig`](#aclclaimconfig)<br>Name of the user or group
`actions` | `string[]`<br>Array of actions to allow
`profiles` | `string[]`<br>List of profiles to grant

## AclClaimConfig

New claim to create

Name | Description
---- | -----------
`type` | `string`<br>The claim type
`value` | `string`<br>The claim value

## AclProfileConfig

Configuration for an ACL profile. This defines a preset group of actions which can be given to a user via an ACL entry.

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this profile
`actions` | `string[]`<br>Actions to include
`excludeActions` | `string[]`<br>Actions to exclude from the inherited actions
`extends` | `string[]`<br>Other profiles to extend from

## SecretConfig

Configuration for a secret value

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this secret
`data` | `string` `->` `string`<br>Key/value pairs associated with this secret
`sources` | [`ExternalSecretConfig`](#externalsecretconfig)`[]`<br>Providers to source key/value pairs from
`acl` | [`AclConfig`](#aclconfig)<br>Defines access to this particular secret

## ExternalSecretConfig

Configuration for an external secret provider

Name | Description
---- | -----------
`provider` | `string`<br>Name of the provider to use
`key` | `string`<br>Optional key indicating the parameter to set in the resulting data array
`path` | `string`<br>Optional value indicating what to fetch from the provider
`arguments` | `string` `->` `string`<br>Additional provider-specific arguments

## DeviceConfig

Configuration for devices

Name | Description
---- | -----------
`platforms` | [`DevicePlatformConfig`](#deviceplatformconfig)`[]`<br>List of device platforms
`pools` | [`DevicePoolConfig`](#devicepoolconfig)`[]`<br>List of device pools

## DevicePlatformConfig

Configuration for a device platform

Name | Description
---- | -----------
`id` | `string`<br>The id for this platform
`name` | `string`<br>Name of the platform
`models` | `string[]`<br>A list of platform models
`legacyNames` | `string[]`<br>Legacy names which older versions of Gauntlet may be using
`legacyPerfSpecHighModel` | `string`<br>Model name for the high perf spec, which may be requested by Gauntlet

## DevicePoolConfig

Configuration for a device pool

Name | Description
---- | -----------
`id` | `string`<br>The id for this platform
`name` | `string`<br>The name of the pool
`poolType` | [`DevicePoolType`](#devicepooltype-enum)<br>The type of the pool
`projectIds` | `string[]`<br>List of project ids associated with pool

## DevicePoolType (Enum)

The type of device pool

Name | Description
---- | -----------
`Automation` | Available to CIS jobs
`Shared` | Shared by users with remote checking and checkouts

## ToolConfig

Options for configuring a tool

Name | Description
---- | -----------
`id` | `string`<br>Unique identifier for the tool
`name` | `string`<br>Name of the tool
`description` | `string`<br>Description for the tool
`category` | `string`<br>Category for the tool. Will cause the tool to be shown in a different tab in the dashboard.
`public` | `boolean`<br>Whether this tool should be exposed for download on a public endpoint without authentication
`showInUgs` | `boolean`<br>Whether to show this tool for download in the UGS tools menu
`showInDashboard` | `boolean`<br>Whether to show this tool for download in the dashboard
`namespaceId` | `string`<br>Default namespace for new deployments of this tool
`acl` | [`AclConfig`](#aclconfig)<br>Permissions for the tool

## StorageConfig

Configuration for storage

Name | Description
---- | -----------
`enableGC` | `boolean`<br>Whether to enable garbage collection
`backends` | [`BackendConfig`](#backendconfig)`[]`<br>List of storage backends
`namespaces` | [`NamespaceConfig`](#namespaceconfig)`[]`<br>List of namespaces for storage

## BackendConfig

Common settings object for different providers

Name | Description
---- | -----------
`id` | `string`<br>The storage backend ID
`base` | `string`<br>Base backend to copy default settings from
`type` | [`StorageBackendType`](#storagebackendtype-enum)<br>The type of storage backend to use
`baseDir` | `string`<br>Base directory for filesystem storage
`awsBucketName` | `string`<br>Name of the bucket to use
`awsBucketPath` | `string`<br>Base path within the bucket
`awsCredentials` | [`AwsCredentialsType`](#awscredentialstype-enum)<br>Type of credentials to use
`awsRole` | `string`<br>ARN of a role to assume
`awsProfile` | `string`<br>The AWS profile to read credentials form
`awsRegion` | `string`<br>Region to connect to
`azureConnectionString` | `string`<br>Connection string for Azure
`azureContainerName` | `string`<br>Name of the container
`relayServer` | `string`<br>
`relayToken` | `string`<br>

## StorageBackendType (Enum)

Types of storage backend to use

Name | Description
---- | -----------
`FileSystem` | Local filesystem
`Aws` | AWS S3
`Azure` | Azure blob store
`Memory` | In-memory only (for testing)

## AwsCredentialsType (Enum)

Credentials to use for AWS

Name | Description
---- | -----------
`Default` | Use default credentials from the AWS SDK
`Profile` | Read credentials from the  profile in the AWS config file
`AssumeRole` | Assume a particular role. Should specify ARN in
`AssumeRoleWebIdentity` | Assume a particular role using the current environment variables.

## NamespaceConfig

Configuration of a particular namespace

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this namespace
`backend` | `string`<br>Backend to use for this namespace
`prefix` | `string`<br>Prefix for items within this namespace
`gcFrequencyHrs` | `number`<br>How frequently to run garbage collection, in hours.
`gcDelayHrs` | `number`<br>How long to keep newly uploaded orphanned objects before allowing them to be deleted, in hours.
`enableAliases` | `boolean`<br>Support querying exports by their aliases
`acl` | [`AclConfig`](#aclconfig)<br>Access list for this namespace

## ArtifactTypeConfig

Configuration for an artifact

Name | Description
---- | -----------
`type` | `string`<br>Name of the artifact type
`name` | `string`<br>Legacy 'Name' property
`keepCount` | `integer`<br>Number of artifacts to retain
`keepDays` | `integer`<br>Number of days to retain artifacts of this type
