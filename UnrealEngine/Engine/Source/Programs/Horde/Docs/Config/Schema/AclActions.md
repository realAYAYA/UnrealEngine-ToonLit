[Horde](../../../README.md) > [Configuration](../../Config.md) > ACL Actions

# ACL Actions

## Accounts

| Name | Description |
| ---- | ----------- |
| `CreateAccount` | Ability to create new accounts |
| `UpdateAccount` | Update an account settings |
| `DeleteAccount` | Delete an account from the server |
| `ViewAccount` | Ability to view account information |

## Agents

| Name | Description |
| ---- | ----------- |
| `CreateAgent` | Ability to create an agent. This may be done explicitly, or granted to agents to allow them to self-register. |
| `UpdateAgent` | Update an agent's name, pools, etc... |
| `DeleteAgent` | Soft-delete an agent |
| `ViewAgent` | View an agent |
| `ListAgents` | List the available agents |

## Artifacts

| Name | Description |
| ---- | ----------- |
| `ReadArtifact` | Permission to read from an artifact |
| `WriteArtifact` | Permission to write to an artifact |
| `UploadArtifact` | Ability to create an artifact. Typically just for debugging; agents have this access for a particular session. |
| `DownloadArtifact` | Ability to download an artifact |

## Bisect

| Name | Description |
| ---- | ----------- |
| `CreateBisectTask` | Ability to start new bisect tasks |
| `UpdateBisectTask` | Ability to update a bisect task |
| `ViewBisectTask` | Ability to view a bisect task |

## Compute

| Name | Description |
| ---- | ----------- |
| `AddComputeTasks` | User can add tasks to the compute cluster |
| `GetComputeTasks` | User can get and list tasks from the compute cluster |

## Ddc

| Name | Description |
| ---- | ----------- |
| `DdcReadObject` | General read access to refs / blobs and so on |
| `DdcWriteObject` | General write access to upload refs / blobs etc |
| `DdcDeleteObject` | Access to delete blobs / refs etc |
| `DdcDeleteBucket` | Access to delete a particular bucket |
| `DdcDeleteNamespace` | Access to delete a whole namespace |
| `DdcReadTransactionLog` | Access to read the transaction log |
| `DdcWriteTransactionLog` | Access to write the transaction log |
| `DdcAdminAction` | Access to perform administrative task |

## Devices

| Name | Description |
| ---- | ----------- |
| `DeviceRead` | Ability to read devices |
| `DeviceWrite` | Ability to write devices |

## Jobs

| Name | Description |
| ---- | ----------- |
| `CreateJob` | Ability to start new jobs |
| `UpdateJob` | Rename a job, modify its priority, etc... |
| `DeleteJob` | Delete a job properties |
| `ExecuteJob` | Allows updating a job metadata (name, changelist number, step properties, new groups, job states, etc...). Typically granted to agents. Not user facing. |
| `RetryJobStep` | Ability to retry a failed job step |
| `ViewJob` | Ability to view a job |

## Leases

| Name | Description |
| ---- | ----------- |
| `ViewLeases` | View all the leases that an agent has worked on |
| `ViewLeaseTasks` | View the task data for a lease |

## Logs

| Name | Description |
| ---- | ----------- |
| `CreateLog` | Ability to create a log. Implicitly granted to agents. |
| `UpdateLog` | Ability to update log metadata |
| `ViewLog` | Ability to view a log contents |
| `WriteLogData` | Ability to write log data |
| `CreateEvent` | Ability to create events |
| `ViewEvent` | Ability to view events |

## Notices

| Name | Description |
| ---- | ----------- |
| `CreateNotice` | Ability to create new notices |
| `UpdateNotice` | Ability to modify notices on the server |
| `DeleteNotice` | Ability to delete notices |

## Notifications

| Name | Description |
| ---- | ----------- |
| `CreateSubscription` | Ability to subscribe to notifications |

## Pools

| Name | Description |
| ---- | ----------- |
| `CreatePool` | Create a global pool of agents |
| `UpdatePool` | Modify an agent pool |
| `DeletePool` | Delete an agent pool |
| `ViewPool` | Ability to view a pool |
| `ListPools` | View all the available agent pools |

## Projects

| Name | Description |
| ---- | ----------- |
| `CreateProject` | Allows the creation of new projects |
| `DeleteProject` | Allows deletion of projects. |
| `UpdateProject` | Modify attributes of a project (name, categories, etc...) |
| `ViewProject` | View information about a project |

## Replicators

| Name | Description |
| ---- | ----------- |
| `UpdateReplicator` | Allows deletion of projects. |
| `ViewReplicator` | Allows the creation of new projects |

## Secrets

| Name | Description |
| ---- | ----------- |
| `ViewSecret` | View a credential |

## Server

| Name | Description |
| ---- | ----------- |
| `AdminRead` | Ability to read any data from the server. Always inherited. |
| `AdminWrite` | Ability to write any data to the server. |
| `Debug` | Access to the debug endpoints |
| `Impersonate` | Ability to impersonate another user |
| `ViewCosts` | View estimated costs for particular operations |
| `IssueBearerToken` | Issue bearer token for the current user |

## ServiceAccounts

| Name | Description |
| ---- | ----------- |
| `CreateAccount` | Ability to create new accounts |
| `UpdateAccount` | Update an account settings |
| `DeleteAccount` | Delete an account from the server |
| `ViewAccount` | Ability to view account information |

## Sessions

| Name | Description |
| ---- | ----------- |
| `CreateSession` | Granted to agents to call CreateSession, which returns a bearer token identifying themselves valid to call UpdateSesssion via gRPC. |
| `ViewSession` | Allows viewing information about an agent session |

## Software

| Name | Description |
| ---- | ----------- |
| `UploadSoftware` | Ability to upload new versions of the agent software |
| `DownloadSoftware` | Ability to download the agent software |
| `DeleteSoftware` | Ability to delete agent software |

## Storage

| Name | Description |
| ---- | ----------- |
| `ReadBlobs` | Ability to read blobs from the storage service |
| `WriteBlobs` | Ability to write blobs to the storage service |
| `ReadRefs` | Ability to read refs from the storage service |
| `WriteRefs` | Ability to write refs to the storage service |
| `DeleteRefs` | Ability to delete refs |

## Streams

| Name | Description |
| ---- | ----------- |
| `CreateStream` | Allows the creation of new streams within a project |
| `UpdateStream` | Allows updating a stream (agent types, templates, schedules) |
| `DeleteStream` | Allows deleting a stream |
| `ViewStream` | Ability to view a stream |
| `ViewChanges` | View changes submitted to a stream. NOTE: this returns responses from the server's Perforce account, which may be a priviledged user. |
| `ViewTemplate` | View template associated with a stream |

## Telemetry

| Name | Description |
| ---- | ----------- |
| `QueryMetrics` | Ability to search for various metrics |

## Tools

| Name | Description |
| ---- | ----------- |
| `DownloadTool` | Ability to download a tool |
| `UploadTool` | Ability to upload new tool versions |
