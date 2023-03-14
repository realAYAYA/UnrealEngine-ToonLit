// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class ConsistencyState
    {
    }

    // ReSharper disable once ClassNeverInstantiated.Global
    public class BlobStoreConsistencyCheckService : PollingService<ConsistencyState>
    {
        private readonly IOptionsMonitor<ConsistencyCheckSettings> _settings;
        private readonly IOptionsMonitor<HordeStorageSettings> _hordeStorageSettings;
        private readonly IServiceProvider _provider;
        private readonly ILeaderElection _leaderElection;
        private readonly IReferencesStore _referencesStore;
        private readonly IBlobIndex _blobIndex;
        private readonly ILogger _logger = Log.ForContext<BlobStoreConsistencyCheckService>();

        protected override bool ShouldStartPolling()
        {
            return _settings.CurrentValue.EnableBlobStoreChecks;
        }

        public BlobStoreConsistencyCheckService(IOptionsMonitor<ConsistencyCheckSettings> settings, IOptionsMonitor<HordeStorageSettings> hordeStorageSettings, IServiceProvider provider, ILeaderElection leaderElection, IReferencesStore referencesStore, IBlobIndex blobIndex) : base(serviceName: nameof(BlobStoreConsistencyCheckService), TimeSpan.FromSeconds(settings.CurrentValue.ConsistencyCheckPollFrequencySeconds), new ConsistencyState())
        {
            _settings = settings;
            _hordeStorageSettings = hordeStorageSettings;
            _provider = provider;
            _leaderElection = leaderElection;
            _referencesStore = referencesStore;
            _blobIndex = blobIndex;
        }

        public override async Task<bool> OnPoll(ConsistencyState state, CancellationToken cancellationToken)
        {
            if (!_settings.CurrentValue.EnableBlobStoreChecks)
            {
                _logger.Information("Skipped running blob store consistency check as it is disabled");
                return false;
            }

            await RunConsistencyCheck();

            return true;
        }

        private async Task RunConsistencyCheck()
        {
            foreach (IBlobStore blobStore in BlobService.GetBlobStores(_provider, _hordeStorageSettings).Where(RunConsistencyCheckOnBlobStore))
            {
                string blobStoreName = blobStore.GetType().Name;

                bool isRootStore = blobStore is AmazonS3Store or AzureBlobStore;
                bool requiresLeader = blobStore is not FileSystemStore;

                if (!_settings.CurrentValue.RunBlobStoreConsistencyCheckOnRootStore && isRootStore)
                {
                    continue;
                }

                if (requiresLeader && !_leaderElection.IsThisInstanceLeader())
                {
                    _logger.Information("Skipped running blob store consistency check Blob Store {BlobStore} because this instance was not the leader", blobStoreName);
                    continue;
                }

                List<NamespaceId> namespaces = await _referencesStore.GetNamespaces().ToListAsync();

                // technically this does not need to be run per namespace but per storage pool
                await foreach (NamespaceId ns in namespaces)
                {
                    ulong countOfBlobsChecked = 0;
                    ulong countOfIncorrectBlobsFound = 0;

                    await foreach ((BlobIdentifier blob, DateTime lastModified) in blobStore.ListObjects(ns))
                    {
                        using IScope scope = Tracer.Instance.StartActive("consistency_check.blob_store");
                        scope.Span.ResourceName = $"{ns}.{blob}";
                        scope.Span.SetTag("BlobStore", blobStoreName);

                        if (countOfBlobsChecked % 100 == 0)
                        {
                            _logger.Information("Consistency check running on Blob Store {BlobStore}, count of blobs processed so far: {CountOfBlobs}", blobStoreName, countOfBlobsChecked);
                        }

                        Interlocked.Increment(ref countOfBlobsChecked);
                        
                        BlobContents contents = await blobStore.GetObject(ns, blob, LastAccessTrackingFlags.SkipTracking);
                        await using Stream s = contents.Stream;

                        bool inconsistencyFound = false;
                        BlobIdentifier newHash = await BlobIdentifier.FromStream(s);
                        if (!blob.Equals(newHash))
                        {
                            _logger.Error("Mismatching hash for {Blob} in {Namespace} stored in {BlobStore}, new hash has {NewHash}. Deleting incorrect blob.", blob, ns, blobStoreName,newHash);

                            Interlocked.Increment(ref countOfIncorrectBlobsFound);
                            await blobStore.DeleteObject(ns, blob);

                            if (isRootStore)
                            {
                                // update blob index tracking to indicate that we no longer have this blob in this region
                                await _blobIndex.RemoveBlobFromRegion(ns, blob);
                            }
                        }

                        scope.Span.SetTag("deleted", inconsistencyFound.ToString());
                    }

                    _logger.Information("Blob Store {BlobStore}: Consistency check finished for {Namespace}, found {CountOfIncorrectBlobs} incorrect blobs. Processed {CountOfBlobs} blobs.", blobStoreName, ns, countOfIncorrectBlobsFound, countOfBlobsChecked);
                }
            }
        }

        private bool RunConsistencyCheckOnBlobStore(IBlobStore blobStore)
        {
            switch (blobStore)
            {
                case FileSystemStore:
                case AzureBlobStore:
                case AmazonS3Store:
                    return true;
                case MemoryBlobStore:
                case MemoryCacheBlobStore:
                case RelayBlobStore:
                    return false;
                default:
                    throw new NotImplementedException();
            }
        }

        protected override Task OnStopping(ConsistencyState state)
        {
            return Task.CompletedTask;
        }
    }
}
