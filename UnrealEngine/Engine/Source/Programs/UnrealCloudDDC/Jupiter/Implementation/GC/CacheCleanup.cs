// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public interface IRefCleanup
    {
        Task<int> Cleanup(CancellationToken cancellationToken);
    }

    public class RefLastAccessCleanup : IRefCleanup
    {
        private readonly IOptionsMonitor<GCSettings> _settings;
        private readonly IReferencesStore _referencesStore;
        private readonly IReplicationLog _replicationLog;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly Tracer _tracer;
        private readonly ILogger _logger;

        public RefLastAccessCleanup(IOptionsMonitor<GCSettings> settings, IReferencesStore referencesStore,
            IReplicationLog replicationLog, INamespacePolicyResolver namespacePolicyResolver, Tracer tracer, ILogger<RefLastAccessCleanup> logger)
        {
            _settings = settings;
            _referencesStore = referencesStore;
            _replicationLog = replicationLog;
            _namespacePolicyResolver = namespacePolicyResolver;
            _tracer = tracer;
            _logger = logger;
        }

        private bool ShouldGCNamespace(NamespaceId ns)
        {
            if (ns == INamespacePolicyResolver.JupiterInternalNamespace)
            {
                // do not apply our cleanup policies to the internal namespace
                return false;
            }

            try
            {
                NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

                if (policy.GcMethod == NamespacePolicy.StoragePoolGCMethod.LastAccess)
                {
                    // only run for namespaces set to use last access tracking
                    return true;
                }

                if (policy.GcMethod == NamespacePolicy.StoragePoolGCMethod.Always)
                {
                    // this is a old namespace that should be cleaned up
                    return true;
                }
            }
            catch (NamespaceNotFoundException)
            {
                _logger.LogWarning("Unknown namespace {Namespace} when attempting to GC References. To opt in to deleting the old namespace add a policy for it with the GcMethod set to always", ns);
                return false;
            }
            
            return false;
        }

        public async Task<int> Cleanup(CancellationToken cancellationToken)
        {
            int countOfDeletedRecords = 0;
            DateTime cutoffTime = DateTime.Now.AddSeconds(-1 * _settings.CurrentValue.LastAccessCutoff.TotalSeconds);
            ulong consideredCount = 0;
            DateTime cleanupStart = DateTime.Now;

            await Parallel.ForEachAsync(_referencesStore.GetRecords(),
                new ParallelOptions
                {
                    MaxDegreeOfParallelism = _settings.CurrentValue.OrphanRefMaxParallelOperations,
                    CancellationToken = cancellationToken
                }, async (tuple, token) =>
                {
                    (NamespaceId ns, BucketId bucket, IoHashKey name, DateTime lastAccessTime) = tuple;

                    if (!ShouldGCNamespace(ns))
                    {
                        return;
                    }

                    _logger.LogDebug(
                        "Considering object in {Namespace} {Bucket} {Name} for deletion, was last updated {LastAccessTime}",
                        ns, bucket, name, lastAccessTime);
                    Interlocked.Increment(ref consideredCount);

                    if (lastAccessTime > cutoffTime)
                    {
                        return;
                    }

                    _logger.LogInformation(
                        "Attempting to delete object {Namespace} {Bucket} {Name} as it was last updated {LastAccessTime} which is older then {CutoffTime}",
                        ns, bucket, name, lastAccessTime, cutoffTime);
                    using TelemetrySpan scope = _tracer.StartActiveSpan("gc.ref")
                        .SetAttribute("operation.name", "gc.ref")
                        .SetAttribute("resource.name", $"{ns}:{bucket}.{name}")
                        .SetAttribute("namespace", ns.ToString());
                    // delete the old record from the ref refs

                    bool storeDelete = false;
                    try
                    {
                        storeDelete = await _referencesStore.Delete(ns, bucket, name);
                        if (storeDelete && _settings.CurrentValue.WriteDeleteToReplicationLog)
                        {
                            // insert a delete event into the transaction log
                            await _replicationLog.InsertDeleteEvent(ns, bucket, name, null);
                        }
                    }
                    catch (Exception e)
                    {
                        _logger.LogWarning(e, "Exception when attempting to delete record {Bucket} {Name} in {Namespace}",
                            bucket, name, ns);
                    }

                    if (storeDelete)
                    {
                        Interlocked.Increment(ref countOfDeletedRecords);
                    }
                    else
                    {
                        _logger.LogWarning("Failed to delete record {Bucket} {Name} in {Namespace}", bucket, name, ns);
                    }
                });

            TimeSpan cleanupDuration = DateTime.Now - cleanupStart;
            _logger.LogInformation(
                "Finished cleaning refs. Refs considered: {ConsideredCount} Refs Deleted: {DeletedCount}. Cleanup took: {CleanupDuration}", consideredCount, countOfDeletedRecords, cleanupDuration);

            return countOfDeletedRecords;
        }
    }
}
