// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using Microsoft.Extensions.Logging;

namespace Jupiter.Implementation
{
    public class OrphanBlobCleanupRefs : IBlobCleanup
    {
        private readonly IOptionsMonitor<GCSettings> _gcSettings;
        private readonly IBlobService _blobService;
        private readonly IObjectService _objectService;
        private readonly IBlobIndex _blobIndex;
        private readonly ILeaderElection _leaderElection;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly Tracer _tracer;
        private readonly ILogger _logger;

        // ReSharper disable once UnusedMember.Global
        public OrphanBlobCleanupRefs(IOptionsMonitor<GCSettings> gcSettings, IBlobService blobService, IObjectService objectService, IBlobIndex blobIndex, ILeaderElection leaderElection, INamespacePolicyResolver namespacePolicyResolver, Tracer tracer, ILogger<OrphanBlobCleanupRefs> logger)
        {
            _gcSettings = gcSettings;
            _blobService = blobService;
            _objectService = objectService;
            _blobIndex = blobIndex;
            _leaderElection = leaderElection;
            _namespacePolicyResolver = namespacePolicyResolver;
            _tracer = tracer;
            _logger = logger;
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
                _logger.LogInformation("Skipped orphan blob (refs) cleanup run as this instance is not the leader");
                return 0;
            }

            List<NamespaceId> namespaces = await ListNamespaces().ToListAsync(cancellationToken: cancellationToken);
            List<NamespaceId> namespacesThatHaveBeenChecked = new List<NamespaceId>();
            // enumerate all namespaces, and check if the old blob is valid in any of them to allow for a blob store to just store them in a single pile if it wants to
            ulong countOfBlobsRemoved = 0;
            _logger.LogInformation("Started orphan blob");
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

                _logger.LogInformation("Running Orphan GC For StoragePool: {StoragePool}", policy.StoragePool);
                namespacesThatHaveBeenChecked.AddRange(namespacesThatSharePool);
                // only consider blobs that have been around for 60 minutes
                // this due to cases were blobs are uploaded first
                DateTime cutoff = DateTime.Now.AddMinutes(-60);
                await Parallel.ForEachAsync(_blobService.ListObjects(@namespace),
                    new ParallelOptions { MaxDegreeOfParallelism = _gcSettings.CurrentValue.OrphanGCMaxParallelOperations, CancellationToken = cancellationToken },
                    async (tuple, ctx) =>
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
                    });

                TimeSpan storagePoolGcDuration = DateTime.Now - startTime;
                _logger.LogInformation("Finished running Orphan GC For StoragePool: {StoragePool}. Took {Duration}", policy.StoragePool, storagePoolGcDuration);
            }

            _logger.LogInformation("Finished running Orphan GC");
            return countOfBlobsRemoved;
        }

        private async Task<bool> GCBlob(string storagePool, List<NamespaceId> namespacesThatSharePool, BlobIdentifier blob, DateTime lastModifiedTime, CancellationToken cancellationToken)
        {
            string storagePoolName = string.IsNullOrEmpty(storagePool) ? "default" : storagePool; 
            using TelemetrySpan removeBlobScope = _tracer.StartActiveSpan("gc.blob")
                .SetAttribute("operation.name", "gc.blob")
                .SetAttribute("resource.name", $"{storagePoolName}.{blob}");

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

                BlobInfo? blobIndex = await _blobIndex.GetBlobInfo(blobNamespace, blob, BlobIndexFlags.IncludeReferences);

                if (blobIndex == null)
                {
                    continue;
                }

                List<(BucketId, IoHashKey)> oldReferences = new List<(BucketId, IoHashKey)>();

                foreach ((BucketId, IoHashKey) tuple in blobIndex.References!)
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
                        oldReferences.Add(tuple);
                    }
                }

                if (found)
                {
                    // if the object is still alive but had old references we remove the old references to keep the size of the references array more reasonable
                    await _blobIndex.RemoveReferences(blobNamespace, blob, oldReferences);
                }
            }

            if (cancellationToken.IsCancellationRequested)
            {
                return false;
            }

            removeBlobScope.SetAttribute("removed", (!found).ToString());

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
            _logger.LogInformation("Attempting to GC Orphan blob {Blob} from {Namespace} which was last modified at {LastModifiedTime}", blob, ns, lastModifiedTime);
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
                _logger.LogWarning("Failed to delete blob {Blob} from {Namespace} due to {Error}", blob, ns, e.Message);
            }
        }

        private IAsyncEnumerable<NamespaceId> ListNamespaces()
        {
            return _objectService.GetNamespaces();
        }
    }
}
