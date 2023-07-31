// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Common;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public interface IRefCleanup
    {
        Task<int> Cleanup(NamespaceId ns, CancellationToken cancellationToken);
    }

    public class RefCleanup : IRefCleanup
    {
        private readonly IOptionsMonitor<GCSettings> _settings;
        private readonly IRefsStore _refs;
        private readonly IReferencesStore _referencesStore;
        private readonly IReplicationLog _replicationLog;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly ITransactionLogWriter _transactionLog;
        private readonly ILogger _logger = Log.ForContext<RefCleanup>();

        public RefCleanup(IOptionsMonitor<GCSettings> settings, IRefsStore refs, ITransactionLogWriter transactionLog, IReferencesStore referencesStore, IReplicationLog replicationLog, INamespacePolicyResolver namespacePolicyResolver)
        {
            _settings = settings;
            _refs = refs;
            _transactionLog = transactionLog;
            _referencesStore = referencesStore;
            _replicationLog = replicationLog;
            _namespacePolicyResolver = namespacePolicyResolver;
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
                _logger.Warning("Namespace {Namespace} does not configure any policy, not running ref cleanup on it.", ns);
                return Task.FromResult(0);
            }

            if (ns == INamespacePolicyResolver.JupiterInternalNamespace)
            {
                // do not apply our cleanup policies to the internal namespace
                return Task.FromResult(0);
            }
            else if (!policies.IsLegacyNamespace.HasValue)
            {
                throw new NotImplementedException(
                    $"Namespace {ns} did not set IsLegacyNamespace, unable to clean it as we do not know which method to use.");
            }
            else if (!policies.IsLegacyNamespace.Value)
            {
                return CleanNamespace(ns, cancellationToken);
            }
            else if (policies.IsLegacyNamespace.Value)
            {
                return CleanNamespaceLegacy(ns, cancellationToken);
            }
            else
            {
                throw new NotImplementedException();
            }
        }

        private async Task<int> CleanNamespace(NamespaceId ns, CancellationToken cancellationToken)
        {
            int countOfDeletedRecords = 0;
            DateTime cutoffTime = DateTime.Now.AddSeconds(-1 * _settings.CurrentValue.LastAccessCutoff.TotalSeconds);
            ulong consideredCount = 0;
            await Parallel.ForEachAsync(_referencesStore.GetRecords(ns), new ParallelOptions {MaxDegreeOfParallelism = _settings.CurrentValue.OrphanRefMaxParallelOperations, CancellationToken = cancellationToken}, async (tuple, token) =>
            {
                (BucketId bucket, IoHashKey name, DateTime lastAccessTime) = tuple;

                _logger.Debug("Considering object in {Namespace} {Bucket} {Name} for deletion, was last updated {LastAccessTime}", ns, bucket, name, lastAccessTime);
                Interlocked.Increment(ref consideredCount);

                if (lastAccessTime > cutoffTime)
                {
                    return;
                }

                _logger.Information("Attempting to delete object {Namespace} {Bucket} {Name} as it was last updated {LastAccessTime} which is older then {CutoffTime}", ns, bucket, name, lastAccessTime, cutoffTime);
                using IScope scope = Tracer.Instance.StartActive("gc.ref");
                scope.Span.ResourceName = $"{ns}:{bucket}.{name}";
                scope.Span.SetTag("namespace", ns.ToString());
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
                    _logger.Warning(e, "Exception when attempting to delete record {Bucket} {Name} in {Namespace}", bucket, name, ns);
                }

                if (storeDelete)
                {
                    Interlocked.Increment(ref countOfDeletedRecords);
                }
                else
                {
                    _logger.Warning("Failed to delete record {Bucket} {Name} in {Namespace}", bucket, name, ns);            
                }
            });

            _logger.Information("Finished cleaning {Namespace}. Refs considered: {ConsideredCount} Refs Deleted: {DeletedCount}", ns, consideredCount, countOfDeletedRecords);

            return countOfDeletedRecords;
        }

        private async Task<int> CleanNamespaceLegacy(NamespaceId ns, CancellationToken cancellationToken)
        {
            int countOfDeletedRecords = 0;
            await foreach (OldRecord record in _refs.GetOldRecords(ns, _settings.CurrentValue.LastAccessCutoff).WithCancellation(cancellationToken))
            {
                using IScope scope = Tracer.Instance.StartActive("gc.ref.legacy");
                scope.Span.ResourceName = $"{ns}:{record.Bucket}.{record.RefName}";

                // delete the old record from the ref refs
                Task storeDelete = _refs.Delete(record.Namespace, record.Bucket, record.RefName);
                // insert a delete event into the transaction log
                Task transactionLogDelete = _transactionLog.Delete(record.Namespace, record.Bucket, record.RefName);

                try
                {
                    await Task.WhenAll(storeDelete, transactionLogDelete);
                }
                catch (Exception e)
                {
                    _logger.Warning(e, "Exception when attempting to delete record {Record} in {Namespace}", record, ns);
                }

                Interlocked.Increment(ref countOfDeletedRecords);
            }

            return countOfDeletedRecords;
        }
    }
}
