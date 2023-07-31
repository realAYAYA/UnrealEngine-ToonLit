// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using async_enumerable_dotnet;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Common;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using RestSharp;
using RestSharp.Serializers.NewtonsoftJson;
using Serilog;

namespace Horde.Storage.Implementation
{
    public interface IBlobCleanup
    {
        bool ShouldRun();
        Task<ulong> Cleanup(CancellationToken none);
    }

    public class OrphanBlobCleanup : IBlobCleanup
    {
        private readonly IBlobService _blobService;
        private readonly ILeaderElection _leaderElection;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly IRestClient _client;
        private readonly ILogger _logger = Log.ForContext<OrphanBlobCleanup>();

        // ReSharper disable once UnusedMember.Global
        public OrphanBlobCleanup(IBlobService blobService, ILeaderElection leaderElection, IOptionsMonitor<CallistoTransactionLogSettings> callistoSettings, IServiceCredentials serviceCredentials, INamespacePolicyResolver namespacePolicyResolver)
        {
            _blobService = blobService;
            _leaderElection = leaderElection;
            _namespacePolicyResolver = namespacePolicyResolver;

            _client = new RestClient(callistoSettings.CurrentValue.ConnectionString)
            {
                Authenticator = serviceCredentials.GetAuthenticator()
            }.UseSerializer(() => new JsonNetSerializer());
        }

        internal OrphanBlobCleanup(IBlobService blobService, ILeaderElection leaderElection, IRestClient callistoClient, INamespacePolicyResolver namespacePolicyResolver)
        {
            _blobService = blobService;
            _leaderElection = leaderElection;
            _client = callistoClient;
            _namespacePolicyResolver = namespacePolicyResolver;
        }

        private struct GCRootState
        {
            public long TransactionLogPointer { get; }
            public HashSet<BlobIdentifier> GcRoots { get; }
            public Guid TransactionLogGeneration { get; set; }

            public GCRootState(long transactionLogPointer, Guid transactionLogGeneration, HashSet<BlobIdentifier> gcRoots)
            {
                TransactionLogPointer = transactionLogPointer;
                TransactionLogGeneration = transactionLogGeneration;
                GcRoots = gcRoots;
            }
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
                _logger.Information("Skipped orphan blob cleanup run as this instance is not the leader");
                return 0;
            }

            List<NamespaceId> namespaces = await ListNamespaces().Where(NamespaceShouldBeCleaned).ToListAsync();
            ConcurrentDictionary<NamespaceId, GCRootState> perNamespaceRoots = new();
            await namespaces.ParallelForEachAsync(async ns =>
            {
                GCRootState gcRootState = await DetermineGCRoots(ns, _client, cancellationToken);
                perNamespaceRoots.TryAdd(ns, gcRootState);
            }, cancellationToken);

            ulong countOfBlobsRemoved = 0;
            List<BlobIdentifier> blobsToRemove = new List<BlobIdentifier>();

            // enumerate all namespaces, and check if the old blob is valid in any of them to allow for a blob store to just store them in a single pile if it wants to
            foreach (NamespaceId @namespace in namespaces)
            {
                // only consider blobs that have been around for 60 minutes
                // this due to cases were blobs are uploaded first, and the GC roots specified after the fact for DDC keys
                await foreach ((BlobIdentifier blob, DateTime lastModified) in _blobService.ListObjects(@namespace).WithCancellation(cancellationToken))
                {
                    if (lastModified > DateTime.Now.AddMinutes(-60))
                    {
                        continue;
                    }

                    // TODO: This could be turned into a parallel for as each iteration does not mutate the state
                    // there is no parallel for each for async enumerable yet
                    // we could manually build one, see https://github.com/dotnet/corefx/issues/34233 but lets wait until we know the time this takes is an issue

                    bool blobFoundInRootSet = perNamespaceRoots.Values.Any(gcRootState => gcRootState.GcRoots.Contains(blob));

                    if (blobFoundInRootSet)
                    {
                        continue;
                    }

                    bool blobFound = false;
                    // we enumerate the namespaces again as we need to check each transaction log to determine if its been added to any of them
                    await namespaces.ParallelForEachAsync(async ns =>
                    {
                        GCRootState gcRootState = perNamespaceRoots[ns];
                        bool hasBeenAdded = await HasBlobBeenAddedToGcSet(ns, blob, gcRootState.TransactionLogPointer, gcRootState.TransactionLogGeneration,
                            cancellationToken);
                        if (hasBeenAdded)
                        {
                            blobFound = true;
                        }
                    }, cancellationToken);

                    if (!blobFound)
                    {
                        blobsToRemove.Add(blob);
                    }
                }
            }

            foreach (BlobIdentifier blob in blobsToRemove)
            {
                if (cancellationToken.IsCancellationRequested)
                {
                    break;
                }

                _logger.Information("Attempting to GC Orphan blob {Blob}", blob);

                bool deleted = true;
                foreach (NamespaceId ns in namespaces)
                {
                    using IScope removeBlobScope = Tracer.Instance.StartActive("gc.blob.legacy");
                    removeBlobScope.Span.ResourceName = $"{ns}.{blob}";

                    try
                    {
                        await _blobService.DeleteObject(ns, blob);
                    }
                    catch (Exception e)
                    {
                        _logger.Warning("Failed to delete blob {Blob} from {Namespace} due to {Error}", blob, ns, e.Message);
                        deleted = false;
                    }
                }
                if (deleted)
                {
                    ++countOfBlobsRemoved;
                }
            }

            return countOfBlobsRemoved;
        }

        private bool NamespaceShouldBeCleaned(NamespaceId ns)
        {
            NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

            return policy.IsLegacyNamespace.HasValue && policy.IsLegacyNamespace.Value;
        }

        public virtual async IAsyncEnumerable<NamespaceId> ListNamespaces()
        {
            RestRequest request = new RestRequest("api/v1/t/");

            IRestResponse<ListNamespaceResponse> response = await _client.ExecuteGetAsync<ListNamespaceResponse>(request);
            if (!response.IsSuccessful)
            {
                throw response.ToException("Unable to list namespaces from callisto.");
            }

            foreach (NamespaceId ns in response.Data.Logs)
            {
                yield return ns;
            }
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Used by serialization")]
        public class ListNamespaceResponse
        {
            public NamespaceId[] Logs { get; set; } = null!;
        }

        private async Task<bool> HasBlobBeenAddedToGcSet(NamespaceId ns, BlobIdentifier blob, long transactionLogPointer, Guid transactionLogGeneration, CancellationToken cancellationToken)
        {
            // request all events since the transaction log pointer and make sure this blob has not been added since

            CallistoReader reader = new CallistoReader(_client, ns);
            await foreach (bool found in reader.GetOps(transactionLogPointer, transactionLogGeneration, maxOffsetsAttempted: 1000000, cancellationToken: cancellationToken).Any(transactionEvent =>
            {
                if (transactionEvent is AddTransactionEvent addTransactionEvent)
                {
                    return addTransactionEvent.Blobs?.Any(identifier => identifier.Equals(blob)) ?? false;
                }

                return false;
            })
                .WithCancellation(cancellationToken))
            {
                return found;
            }

            return false;
        }

        private async Task<GCRootState> DetermineGCRoots(NamespaceId ns, IRestClient client, CancellationToken cancellationToken)
        {
            Dictionary<string, BlobIdentifier[]> events = new Dictionary<string, BlobIdentifier[]>();
            // Build the hashset from reading the entire callisto log
            const int prefetchCount = 300; // 300 because the chunk size is usually 100 for callisto gets, thus we attempt to read 3x rows ahead
            // prefetch events so that we do can just processes them directly without having to wait as much

            CallistoReader reader = new CallistoReader(client, ns);
            OpsEnumerationState enumerationState = new OpsEnumerationState();
            long offsetToContinueAt = 0;
            await foreach (TransactionEvent transactionEvent in reader.GetOps(cancellationToken: cancellationToken, maxOffsetsAttempted: 1000000, enumerationState: enumerationState).Prefetch(prefetchCount).WithCancellation(cancellationToken))
            {
                if (transactionEvent == null)
                {
                    continue;
                }

                string key = $"{transactionEvent.Bucket}.{transactionEvent.Name}";
                switch (transactionEvent)
                {
                    case AddTransactionEvent add:
                        if (add.Blobs == null)
                        {
                            throw new Exception("Expected to find blobs in a add transaction event");
                        }

                        if (!events.TryAdd(key, add.Blobs))
                        {
                            _logger.Debug("Event with {Key} already added. This indicates a replace operation which should not happen often.", key);
                        }
                        break;
                    case RemoveTransactionEvent remove:
                        events.Remove(key);
                        break;

                    default:
                        throw new NotImplementedException("Unknown transaction event type: " + transactionEvent);
                }

                offsetToContinueAt = transactionEvent.NextIdentifier!.Value;
            }

            HashSet<BlobIdentifier> gcRoots = new HashSet<BlobIdentifier>(events.Values.SelectMany(e => e));
            return new GCRootState(offsetToContinueAt, enumerationState.ReplicatingGeneration, gcRoots);
        }
    }
}
