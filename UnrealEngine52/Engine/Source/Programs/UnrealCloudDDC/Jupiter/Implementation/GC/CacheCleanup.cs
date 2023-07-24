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
        Task<int> Cleanup(NamespaceId ns, CancellationToken cancellationToken);
    }

    public class RefCleanup : IRefCleanup
    {
        private readonly IOptionsMonitor<GCSettings> _settings;
        private readonly IReferencesStore _referencesStore;
        private readonly IReplicationLog _replicationLog;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly Tracer _tracer;
        private readonly ILogger _logger;

        public RefCleanup(IOptionsMonitor<GCSettings> settings, IReferencesStore referencesStore,
            IReplicationLog replicationLog, INamespacePolicyResolver namespacePolicyResolver, Tracer tracer, ILogger<RefCleanup> logger)
        {
            _settings = settings;
            _referencesStore = referencesStore;
            _replicationLog = replicationLog;
            _namespacePolicyResolver = namespacePolicyResolver;
            _tracer = tracer;
            _logger = logger;
        }

        public Task<int> Cleanup(NamespaceId ns, CancellationToken cancellationToken)
        {
            NamespacePolicy policies;
            try
            {
                policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
            }
            catch (UnknownNamespaceException)
            {
                _logger.LogWarning("Namespace {Namespace} does not configure any policy, not running ref cleanup on it.",
                    ns);
                return Task.FromResult(0);
            }

            if (ns == INamespacePolicyResolver.JupiterInternalNamespace)
            {
                // do not apply our cleanup policies to the internal namespace
                return Task.FromResult(0);
            }

            return CleanNamespace(ns, cancellationToken);
        }

        private async Task<int> CleanNamespace(NamespaceId ns, CancellationToken cancellationToken)
        {
            int countOfDeletedRecords = 0;
            DateTime cutoffTime = DateTime.Now.AddSeconds(-1 * _settings.CurrentValue.LastAccessCutoff.TotalSeconds);
            ulong consideredCount = 0;
            DateTime cleanupStart = DateTime.Now;
            await Parallel.ForEachAsync(_referencesStore.GetRecords(ns),
                new ParallelOptions
                {
                    MaxDegreeOfParallelism = _settings.CurrentValue.OrphanRefMaxParallelOperations,
                    CancellationToken = cancellationToken
                }, async (tuple, token) =>
                {
                    (BucketId bucket, IoHashKey name, DateTime lastAccessTime) = tuple;

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
                        if (storeDelete)
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
                "Finished cleaning {Namespace}. Refs considered: {ConsideredCount} Refs Deleted: {DeletedCount}. Cleanup took: {CleanupDuration}", ns,
                consideredCount, countOfDeletedRecords, cleanupDuration);

            return countOfDeletedRecords;
        }
    }
}
