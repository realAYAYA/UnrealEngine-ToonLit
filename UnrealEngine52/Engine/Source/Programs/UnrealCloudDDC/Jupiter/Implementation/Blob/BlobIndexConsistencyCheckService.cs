// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using Microsoft.Extensions.Logging;

namespace Jupiter.Implementation
{
    public class BlobIndexConsistencyState
    {
    }

    // ReSharper disable once ClassNeverInstantiated.Global
    public class BlobIndexConsistencyCheckService : PollingService<BlobIndexConsistencyState>
    {
        private readonly IOptionsMonitor<ConsistencyCheckSettings> _settings;
        private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
        private readonly ILeaderElection _leaderElection;
        private readonly IBlobIndex _blobIndex;
        private readonly IBlobService _blobService;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly Tracer _tracer;
        private readonly ILogger _logger;

        protected override bool ShouldStartPolling()
        {
            return _settings.CurrentValue.EnableBlobIndexChecks;
        }

        public BlobIndexConsistencyCheckService(IOptionsMonitor<ConsistencyCheckSettings> settings, IOptionsMonitor<JupiterSettings> jupiterSettings, ILeaderElection leaderElection, IBlobIndex blobIndex, IBlobService blobService, INamespacePolicyResolver namespacePolicyResolver, Tracer tracer, ILogger<BlobIndexConsistencyCheckService> logger) : base(serviceName: nameof(BlobStoreConsistencyCheckService), TimeSpan.FromSeconds(settings.CurrentValue.ConsistencyCheckPollFrequencySeconds), new BlobIndexConsistencyState(), logger)
        {
            _settings = settings;
            _jupiterSettings = jupiterSettings;
            _leaderElection = leaderElection;
            _blobIndex = blobIndex;
            _blobService = blobService;
            _namespacePolicyResolver = namespacePolicyResolver;
            _tracer = tracer;
            _logger = logger;
        }

        public override async Task<bool> OnPoll(BlobIndexConsistencyState state, CancellationToken cancellationToken)
        {
            if (!_settings.CurrentValue.EnableBlobIndexChecks)
            {
                _logger.LogInformation("Skipped running blob index consistency check as it is disabled");
                return false;
            }

            if (!_leaderElection.IsThisInstanceLeader())
            {
                _logger.LogInformation("Skipped running blob index consistency check because this instance was not the leader");
                return false;
            }

            await RunConsistencyCheck();

            return true;
        }

        private async Task RunConsistencyCheck()
        {
            ulong countOfBlobsChecked = 0;
            ulong countOfIncorrectBlobsFound = 0;
            string currentRegion = _jupiterSettings.CurrentValue.CurrentSite;
            await Parallel.ForEachAsync(_blobIndex.GetAllBlobs(), new ParallelOptions
                {
                    MaxDegreeOfParallelism = _settings.CurrentValue.BlobIndexMaxParallelOperations,
                },
                async (blobInfo, token) =>
                {
                    Interlocked.Increment(ref countOfBlobsChecked);

                    if (countOfBlobsChecked % 100 == 0)
                    {
                        _logger.LogInformation("Consistency check running on blob index, count of blobs processed so far: {CountOfBlobs}", countOfBlobsChecked);
                    }

                    using TelemetrySpan scope = _tracer.StartActiveSpan("consistency_check.blob_index").SetAttribute("resource.name", $"{blobInfo.Namespace}.{blobInfo.BlobIdentifier}").SetAttribute("operation.name", "consistency_check.blob_index");

                    bool issueFound = false;
                    bool deleted = false;

                    try
                    {
                        if (!blobInfo.Regions.Any())
                        {
                            Interlocked.Increment(ref countOfIncorrectBlobsFound);
                            _logger.LogWarning("Blob {Blob} in namespace {Namespace} is not tracked to exist in any region", blobInfo.BlobIdentifier, blobInfo.Namespace);
                            issueFound = true;

                            if (await _blobService.ExistsInRootStore(blobInfo.Namespace, blobInfo.BlobIdentifier))
                            {
                                _logger.LogWarning("Blob {Blob} in namespace {Namespace} was found to exist in our blob store so re-adding it to the index", blobInfo.BlobIdentifier, blobInfo.Namespace);

                                // we did have it in our blob store so we adjust the index
                                await _blobIndex.AddBlobToIndex(blobInfo.Namespace, blobInfo.BlobIdentifier);
                            }
                            else
                            {
                                if (_settings.CurrentValue.AllowDeletesInBlobIndex)
                                {
                                    // this blob doesn't exist anywhere so we just cleanup the blob index
                                    _logger.LogWarning("Blob {Blob} in namespace {Namespace} was removed from the blob index as it didnt exist anywhere", blobInfo.BlobIdentifier, blobInfo.Namespace);

                                    await _blobIndex.RemoveBlobFromIndex(blobInfo.Namespace, blobInfo.BlobIdentifier);
                                    deleted = true;
                                }
                            }
                        }
                        else if (blobInfo.Regions.Contains(currentRegion))
                        {
                            if (!await _blobService.ExistsInRootStore(blobInfo.Namespace, blobInfo.BlobIdentifier))
                            {
                                Interlocked.Increment(ref countOfIncorrectBlobsFound);
                                issueFound = true;
                                
                                if (blobInfo.Regions.Count > 1)
                                {
                                    _logger.LogWarning("Blob {Blob} in namespace {Namespace} did not exist in root store but is tracked as doing so in the blob index. Attempting to replicate it.", blobInfo.BlobIdentifier, blobInfo.Namespace);

                                    try
                                    {
                                        BlobContents _ = await _blobService.ReplicateObject(blobInfo.Namespace, blobInfo.BlobIdentifier, force: true);
                                    }
                                    catch (BlobReplicationException e)
                                    {
                                        // we update the blob index to accurately reflect that we do not have the blob, this is not good though as it means a upload that we thought happened now lacks content
                                        if (_settings.CurrentValue.AllowDeletesInBlobIndex)
                                        {
                                            _logger.LogWarning("Updating blob index to remove Blob {Blob} in namespace {Namespace} as we failed to repair it.", blobInfo.BlobIdentifier, blobInfo.Namespace);
                                            await _blobIndex.RemoveBlobFromRegion(blobInfo.Namespace, blobInfo.BlobIdentifier);
                                            deleted = true;
                                        }
                                        else
                                        {
                                            _logger.LogError(e, "Failed to replicate Blob {Blob} in namespace {Namespace}. Unable to repair the blob index", blobInfo.BlobIdentifier, blobInfo.Namespace);
                                        }
                                    }
                                    catch (BlobNotFoundException)
                                    {
                                        // the blob does not exist we should remove it from the blob index
                                        await _blobIndex.RemoveBlobFromIndex(blobInfo.Namespace, blobInfo.BlobIdentifier);
                                        deleted = true;
                                    }
                                }
                                else
                                {
                                    // if the blob only exists in the current region there is no point in attempting to replicate it
                                    _logger.LogWarning("Blob {Blob} in namespace {Namespace} did not exist in root store but is tracked as doing so in the blob index. Does not exist anywhere else so unable to replicate it.", blobInfo.BlobIdentifier, blobInfo.Namespace);

                                    if (_settings.CurrentValue.AllowDeletesInBlobIndex)
                                    {
                                        // this blob can not be repaired so we just delete it from the blob index
                                        _logger.LogWarning("Blob {Blob} in namespace {Namespace} can not be repaired so removing existence from current region.", blobInfo.BlobIdentifier, blobInfo.Namespace);

                                        await _blobIndex.RemoveBlobFromRegion(blobInfo.Namespace, blobInfo.BlobIdentifier);
                                        deleted = true;
                                    }
                                }
                            }
                        }
                    }
                    catch (UnknownNamespaceException)
                    {
                        if (_settings.CurrentValue.AllowDeletesInBlobIndex)
                        {
                            _logger.LogWarning("Blob {Blob} in namespace {Namespace} is of a unknown namespace, removing.", blobInfo.BlobIdentifier, blobInfo.Namespace);

                            // for entries that are of a unknown namespace we simply remove them
                            await _blobIndex.RemoveBlobFromIndex(blobInfo.Namespace, blobInfo.BlobIdentifier);
                            deleted = true;
                        }
                    }
                    catch (Exception e)
                    {
                        _logger.LogError(e, "Exception when doing blob index consistency check for {Blob} in namespace {Namespace}", blobInfo.BlobIdentifier, blobInfo.Namespace);
                        scope.RecordException(e);
                        scope.SetStatus(Status.Error);
                    }

                    scope.SetAttribute("issueFound", issueFound.ToString());
                    scope.SetAttribute("deleted", deleted.ToString());
                }
            );

            _logger.LogInformation("Blob Index Consistency check finished, found {CountOfIncorrectBlobs} incorrect blobs. Processed {CountOfBlobs} blobs.", countOfIncorrectBlobsFound, countOfBlobsChecked);
        }

        protected override Task OnStopping(BlobIndexConsistencyState state)
        {
            return Task.CompletedTask;
        }
    }
}
