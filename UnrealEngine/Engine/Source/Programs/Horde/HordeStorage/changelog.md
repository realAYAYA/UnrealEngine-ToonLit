# Unreleased

# 0.34.8
* Added info to batch request span tags to see the state of each operation
* Fixes for status endpoint when deployed from Perforce

# 0.34.7
* Enabled worker deployments in us-west and eu-central

# 0.34.6
* Improved log event for ref replicator to easier see how far it has replicated

# 0.34.5
* Changed worker deployment container name to be unique (horde-storage-worker) instead of horde-storage to better understand what is running where.
* Fixed up the datadog environment variables to make sure DD understands what is the worker and what is the requests service.

# 0.34.4
* Added missing pod annotations to worker deployments, causing the logs / metrics generated to not show up in the right place in Datadog

# 0.34.3
* Enabled worker deployment and consistency checks in APSE region for testing

# 0.34.2
* Added ConsistencyCheck service which will go thru S3 store and verify that all blobs have the hashes they are expected to have.
* Fixed incorrect hashing of streams, was only used for large (2GB+) blob uploads at this point so should not have caused any issues.
* Added admin endpoint to set state of a refs replicator
* Fixed issues with error messages not being serialized correctly for /refs/ endpoints when using the raw formatter.

# 0.34.1
* Migrated all regions to use AWS Secret Manager
* Bumped replica counts in us-east-1 to distribute load over more instances.
* Fixed issue were the refs replicator would not refresh its state before replication

# 0.34.0
* Deleted gRPC endpoints as they are not in use.
* Deleted /tree endpoints as they are not in use.
* Added support to read secrets from AWS Secret Manager

# 0.33.12
* Changed how the replication-log enumeration works in Scylla to avoid scanning scylla for bucket ids if we can. Should generally improve response times when reading the incremental log and reduce scylla cpu usage.

# 0.33.11
* Added more scopes to scylla replication log read

# 0.33.10
* Added Iron namespace
* Re-renabled refs replication but limiting the work it will do

# 0.33.9
* Disabled refs replication as it seems to be causing performance issues in us-east

# 0.33.8
* Added `test.benchmark` namespace which will be used to run performance tests against

# 0.33.7
* Fixed up issue in the Any access policy were authenticated users could no longer access these endpoints when using OIDC auth.

# 0.33.6
* Disabled orphan GC to prevent it from deleting recently uploaded blobs for DDC2. We still need to implement support for GC of DDC2 refs.

# 0.33.5
* Increased number of retries when we can not reach callisto, makes it less likely to run into issues when the service is being restarted.
* Re-enabled refs replication

# 0.33.4
* Added some more error handling to Callistos metadata deserialization

# 0.33.3
* Fixed order of our ingress rules, fixes that Callisto is no longer accessible because the rule is now last
* Enabled ref replication in EU

# 0.33.2
* Fixed up datadog version annotation

# 0.33.1
* Deployed NLB in us-east-1 and us-west-2 for internal networking

# 0.33.0
* Renamed Europa & Jupiter to Horde.Storage and removed most references to these names.
* Disabled max body size for PUT uploads of blobs
* Fixed bug were /settings was not correctly outputting values of dicts and arrays
* Bug fixes to refs replication
* Added /auth endpoint to let you verify if you have access to a namespace without having to do a operation or know a ref to fetch.
* Improved datadog logging for throughput into s3.
* Added support for sending compact binary objects instead of a json object to most endpoints
* Added /exist endpoint to blobs, similar to /exists but accepts a structured object of what to check for.

# 0.32.13
* Deleted himalia (frontend) as it is not in use.
* Removed if exists check when inserting namespace into scylla, avoiding starting a transaction. Should fix intermittent error seen in prod.

# 0.32.12
* Improved some logging for the ref replication
* Bumped the default replicators in the helm chart
* Renabled health checks in us-east-1

# 0.32.11
* Temporarily disabled refs replicators connecting to us-east to see if this impacts the issues seen in that region.

# 0.32.10
* Catch errors when a replicator fails to start instead of crashing the entire service

# 0.32.9
* Disabled updating the replication log for incremental events (previous version was only for snapshots)

# 0.32.8
* Disabled updating the replication log during replication writing, prevents issue with replication adding copies of the same event from other sites.

# 0.32.7
* Fixed a issue in the ref replicator were it was using the wrong endpoint to resolve reference, thus getting a response it was not expecting
* Fixed a issue in the /refs/ endpoint were it was not correctly verifying access to the namespace

# 0.32.6
* Error out if the incremental replication lacks a ptr to resume from, to prevent infinite replication.

# 0.32.5
* Stop the incremental replication on a unhandled exception

# 0.32.4
* Fixed a issue in the ref replicator not checking status code before attempting to parse response body

# 0.32.3
* Fixed so that the correct exception is throwing during ref replication to actually handle no snapshots being available.

# 0.32.2
* Resolve references during PUT / Finalize of refs async, signficantly speeding up uploads of objects with lots of attachments.
* Restricted the ingress to the http urls we will actually respond to, resulting in people hitting ALB instead of Kestrel for requests we do not handle, which is safer.
* Made the ref replication handle the case were it has not run before, but has no snapshots available to run from.

# 0.32.1
* Use a datacenter aware load balancing policy, setting it to use the local datacenter

# 0.32.0

* Fixed issue when running Jupiter on Windows machines in Dev were it would scan file operations for all files on the drive Jupiter was running on (misconfigured config file reloading)
* Improved performance of blake3 hashing for smallobjects (less then 1MB) by not running those using multithreaded hashing
* Added data validation on scylla settings
* Changed scylla default consistency to LocalOne
* Bumped default replicators to 64 from 16
* Changed how the local scylla keyspace is setup to make it local even on a multi dc deployment
* The settings endpoint will now always pretty print the json output.
* Setup the replication snapshot service to run and create snapshots once a day

# 0.31.24
* Replicators now attempt to automatically recover when they get a transaction log mismatch error

# 0.31.23
* Made sure all replicators have a chance to run even if there is one faulty replicator throwing errors all the time.

# 0.31.22
* Allow `MaxParallelReplications` for the replicator to be set to -1 meaning to run as many as possible. Experimental toggle to see how many parallell invokes the replication would need / do under such conditions.
* Added memory implementation of content id store for easier local testing

# 0.31.21
* Increased the idle timeout on our load balancers from 60 seconds to 180 seconds

# 0.31.20
* Further updates to the replication to make it better handle error responses (bad gateway) during replication

# 0.31.19
* Added retries when getting a bad gateway when querying callisto (likely caused by the batch size being large so the amount of work to do in callisto takes to long).

# 0.31.18
* Added logging to track count of replications being made, even when no replications are being made (resulting in a 0 in our metrics instead of lacking data)
* Exposed when the last replication ran and how many are currently running in the description of a replicator

# 0.31.17
* More error handling in the replication

# 0.31.16
* Fixed issue in the helm chart not setting up the replication log writter setting correctly
* Added null check to replicator state logging to avoid it stopping replication

# 0.31.15
* Another attempt at fixing null state in replicator by removing null override

# 0.31.14
* Resolved null access in replicators that had not run to completion before

# 0.31.13
* Fixed issue were one slow replicator would cause the other replicators to not trigger as often.
* Using the service credentials in the refs replicator resolving 401 issues when attempting to replicate.

# 0.31.12
* More improvements to the logging for the replication.

# 0.31.11
* Improved logging for replication, hoping to be able to use it to better understand how far behind the replication for a region is

# 0.31.10
* Helm package was not forwarding the replicator version which is now fixed.

# 0.31.9
* Fixed issue with controlling parallism of the replication, helm chart was not forwarding the setting as expected.

# 0.31.8
* Increased parallel replications in the ap-southeast-2 region to hopefully have the replication catch up, but also test how the service is impacted by more replications.

# 0.31.7
* Temporarilly reduced size of the ephemeral storage for Europas large file caching as the current nodes can not provide enough to handle a migraton. 

# 0.31.6
* Added missing replicators for `ue.ddc` namespace

# 0.31.5
* Re-enabled replication in us-east-1 now that migration has completed

# 0.31.4
* Added `ephemeral-storage` request to Europa deployment to have space to store large payloads

# 0.31.3
* The replicator will now read its state when attemtpting a new replication, thus respecting the new state that can be set using the admin endpoints.
* Added setting to control the replication strategy when creating the scylla keyspace.

# 0.31.2
* Updated ingress for Europa to include the health check routes used by the cooker now that we do not have himalia at the root.

# 0.31.1
* Disabled admin frontend (Himalia) in production as we do not have a network to reach out internal load balancers.

# 0.31.0

* Added a replication log for data written using `api/v1/refs` and a replicator to replicate blobs added in this way.
* Added support for oodle compression in compressed buffers
* Added a admin endpoint to dump the current set of settings used, `api/v1/admin/settings`
* Moved the admin frontend for prod deployment to only be accessable for internal load balancers.

# 0.30.4
* Added more debugging logging tracking down issues we are seeing in prod.

# 0.30.3
* Fixed a out of memory issue formatting a error string that could grow very large in Callisto

# 0.30.2
* Replication will now verify the hash of the content it is replicating before writing it to the blob store. Fixes issue with the replication polluting the blob store.

# 0.30.1
* Added debug logging to catch issue with 0 byte payload responses in the batch api
* Fixed settings specified in kubernetes for which tree store implementation to use.

# 0.30.0
* Refactored the object api (api for interaction with compact binaries) and is now called the reference api (as it maps from a generic reference to a object)
* Added overload for blob api (previously accessed at `/api/v1/s` is now also found at `api/v1/blobs`).
* Introduced a new object api (`api/v1/objects`) for interaction with compact binary objects (similar to the blob api but with some added features for reference resolving)
* Added compressed buffer api (`api/v1/compressed-blobs`) which allows you to upload a compressed buffer (`application/x-ue-comp`) but have it referenced by its uncompressed payload hash.
* Added type for namespaces, buckets and keys (instead of just using strings). No changes to the api but allows for proper verification of them.

# 0.29.12
* Added retry in callisto writer if we are unable to reach callisto (preventing issues when a new Jupiter version is released and callisto is down for a few seconds)
* Another attempt at fixing the bug mentioned in 0.29.11, this time a test has been added to verify the fix.

# 0.29.11
* Fixed bug in the replication when the remote instance had no new events we would not start from the last event rather we ran from the last event we had seens offset. This resulted in conditions were the replication was running from the start all the time for regions that were fully replicating another region (never had any local data)

# 0.29.10
* Increased resource allocations in kubernetes to match the increased size of kubernetes nodes we are now using.
* Fixed the node selector for the us-east region
* Updated to recreate stategy for callisto in us-east as we already do in other regions

# 0.29.9
* Changed kubernetes pod to node configuration, reverting back to more default approch of even distribution of replicas per node.

# 0.29.8
* Enabled access logging on the ALBs

# 0.29.7
* Enabled replication of fortnite.ddc
* Enabled dotnet metrics sidecar in prod

# 0.29.6
* Added query parameter to callisto log fetching to control how many attempts we do at finding a new valid offset, is useful to find the next valid offset to start replicating from.
* Added api endpoint `/api/v1/g/<ns>/<replicator>/<offset>` to allow you to force which offset the replication should run from. In combination with the callisto endpoint above this should be enough to let you recover a replicator from a bad callisto state.

# 0.29.5
* Callisto will now skip corrupt events attempting to read the next one instead.
* Fixed issue in callisto that occured when creating a new namespace after a failed get attempt of that same namespace.

# 0.29.4
* Replicators are now run in parallel avoiding issues with replicators not running if any other replicator is busy.

# 0.29.3
* Fixed up helm handling when scylla config is not present
* Fixed node affinity configuration for us-west and eu-central.
* Exposed object db implmementation and scylla config to Helm.
* Added replication of ue.mirage.

# 0.29.2
* Fixed issue with response status being set after the response has started in ddc controller

# 0.29.1
* Callisto can now recover from a mismatched starting offset of a event, useful to recover from situtations were it failed to flush to disk.
* Set Recreate for Callisto deployments to avoid issues mounting the EBS volume.

# 0.29.0
* Added the object api that lets you submit compact binaries as a description of which payloads you want to use.
* Added support for Scylla as a database for the object api, which we hope can be used to replace Dynamo while also handling onprem deployments.

# Older releases
No detailed changelog provided for older releases.