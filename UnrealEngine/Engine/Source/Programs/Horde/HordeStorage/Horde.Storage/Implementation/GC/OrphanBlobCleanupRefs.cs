// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using async_enumerable_dotnet;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Common;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class OrphanBlobCleanupRefs : IBlobCleanup
    {
        private readonly IOptionsMonitor<GCSettings> _gcSettings;
        private readonly IBlobService _blobService;
        private readonly IObjectService _objectService;
        private readonly IBlobIndex _blobIndex;
        private readonly ILeaderElection _leaderElection;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly ILogger _logger = Log.ForContext<OrphanBlobCleanupRefs>();

        // ReSharper disable once UnusedMember.Global
        public OrphanBlobCleanupRefs(IOptionsMonitor<GCSettings> gcSettings, IBlobService blobService, IObjectService objectService, IBlobIndex blobIndex, ILeaderElection leaderElection, INamespacePolicyResolver namespacePolicyResolver)
        {
            _gcSettings = gcSettings;
            _blobService = blobService;
            _objectService = objectService;
            _blobIndex = blobIndex;
            _leaderElection = leaderElection;
            _namespacePolicyResolver = namespacePolicyResolver;
        }

        public bool ShouldRun()
        {
            if (!_leaderElection.IsThisInstanceLeader())
            {
                return false;
            }

            return true;
        }

        public async Task<ulong> Cleanup(CancellationToken cancellationToken)
        {
            if (!_leaderElection.IsThisInstanceLeader())
            {
                _logger.Information("Skipped orphan blob (refs) cleanup run as this instance is not the leader");
                return 0;
            }

            List<NamespaceId> namespaces = await ListNamespaces().Where(NamespaceShouldBeCleaned).ToListAsync();
            List<NamespaceId> namespacesThatHaveBeenChecked = new List<NamespaceId>();
            // enumerate all namespaces, and check if the old blob is valid in any of them to allow for a blob store to just store them in a single pile if it wants to
            ulong countOfBlobsRemoved = 0;
            _logger.Information("Started orphan blob");
            foreach (NamespaceId @namespace in namespaces)
            {
                // if we have already checked this namespace there is no need to repeat it
                if (namespacesThatHaveBeenChecked.Contains(@namespace))
                {
                    continue;
                }
                if (cancellationToken.IsCancellationRequested)
                {
                    break;
                }

                DateTime startTime = DateTime.Now;
                NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(@namespace);
                List<NamespaceId> namespacesThatSharePool = namespaces.Where(ns => _namespacePolicyResolver.GetPoliciesForNs(ns).StoragePool == policy.StoragePool).ToList();

                _logger.Information("Running Orphan GC For StoragePool: {StoragePool}", policy.StoragePool);
                namespacesThatHaveBeenChecked.AddRange(namespacesThatSharePool);
                // only consider blobs that have been around for 60 minutes
                // this due to cases were blobs are uploaded first
                DateTime cutoff = DateTime.Now.AddMinutes(-60);
                await _blobService.ListObjects(@namespace).ParallelForEachAsync(async (tuple) =>
                {
                    (BlobIdentifier blob, DateTime lastModified) = tuple;

                    if (lastModified > cutoff)
                    {
                        return;
                    }

                    if (cancellationToken.IsCancellationRequested)
                    {
                        return;
                    }

                    bool removed = await GCBlob(policy.StoragePool, namespacesThatSharePool, blob, lastModified, cancellationToken);

                    if (removed)
                    {
                        Interlocked.Increment(ref countOfBlobsRemoved);
                    }
                }, cancellationToken: cancellationToken, maxDegreeOfParallelism: _gcSettings.CurrentValue.OrphanGCMaxParallelOperations);

                TimeSpan storagePoolGcDuration = DateTime.Now - startTime;
                _logger.Information("Finished running Orphan GC For StoragePool: {StoragePool}. Took {Duration}", policy.StoragePool, storagePoolGcDuration);
            }

            _logger.Information("Finished running Orphan GC");
            return countOfBlobsRemoved;
        }

        private async Task<bool> GCBlob(string storagePool, List<NamespaceId> namespacesThatSharePool, BlobIdentifier blob, DateTime lastModifiedTime, CancellationToken cancellationToken)
        {
            using IScope removeBlobScope = Tracer.Instance.StartActive("gc.blob");
            removeBlobScope.Span.ResourceName = $"{storagePool}.{blob}";

            bool found = false;

            // check all namespaces that share the same storage pool for presence of the blob
            foreach (NamespaceId blobNamespace in namespacesThatSharePool)
            {
                if (cancellationToken.IsCancellationRequested)
                {
                    break;
                }

                if (found)
                {
                    break;
                }

                BlobInfo? blobIndex = await _blobIndex.GetBlobInfo(blobNamespace, blob);

                if (blobIndex == null)
                {
                    break;
                }

                foreach ((BucketId, IoHashKey) tuple in blobIndex.References)
                {
                    try
                    {
                        (BucketId bucket, IoHashKey key) = tuple;
                        (ObjectRecord, BlobContents?) _ = await _objectService.Get(blobNamespace, bucket, key, new string[] { "name" }, doLastAccessTracking: false);
                        found = true;
                        break;
                    }
                    catch (ObjectNotFoundException)
                    {
                        // this is not a valid reference so we should delete
                    }
                    catch (MissingBlobsException)
                    {
                        // we do not care if there are missing blobs, as long as the record is valid we keep this blob around
                        found = true;
                    }
                }
            }

            if (cancellationToken.IsCancellationRequested)
            {
                return false;
            }

            removeBlobScope.Span.SetTag("removed", (!found).ToString());

            // something is still referencing this blob, we should not delete it
            if (found)
            {
                return false;
            }

            // if the blob was not found to have a reference in any of the namespace that share a storage pool then the blob is not used anymore and should be deleted from all the namespaces
            await Parallel.ForEachAsync(namespacesThatSharePool, cancellationToken, async (ns, _) =>
            {
                await RemoveBlob(ns, blob, lastModifiedTime);
            });
            return true;

        }

        private async Task RemoveBlob(NamespaceId ns, BlobIdentifier blob, DateTime lastModifiedTime)
        {
            _logger.Information("Attempting to GC Orphan blob {Blob} from {Namespace} which was last modified at {LastModifiedTime}", blob, ns, lastModifiedTime);
            try
            {
                await _blobService.DeleteObject(ns, blob);
            }
            catch (BlobNotFoundException)
            {
                // ignore blob not found exceptions, if it didn't exist it has been removed so we are happy either way
            }
            catch (Exception e)
            {
                _logger.Warning("Failed to delete blob {Blob} from {Namespace} due to {Error}", blob, ns, e.Message);
            }
        }

        private bool NamespaceShouldBeCleaned(NamespaceId ns)
        {
            try
            {
                NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

                return policy.IsLegacyNamespace.HasValue && !policy.IsLegacyNamespace.Value;
            }
            catch (UnknownNamespaceException)
            {
                _logger.Warning("Namespace {Namespace} does not configure any policy, not running cleanup on it.", ns);
                return false;
            }
        }

        private IAsyncEnumerable<NamespaceId> ListNamespaces()
        {
            return _objectService.GetNamespaces();
        }
    }
}
