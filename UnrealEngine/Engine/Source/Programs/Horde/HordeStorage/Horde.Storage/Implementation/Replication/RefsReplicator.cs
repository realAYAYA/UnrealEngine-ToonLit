// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation.TransactionLog;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Nito.AsyncEx;
using Serilog;
using JsonSerializer = Newtonsoft.Json.JsonSerializer;

namespace Horde.Storage.Implementation
{
    public class RefsReplicator : IReplicator
    {
        private readonly string _name;
        private readonly ILogger _logger = Log.ForContext<RefsReplicator>();
        private readonly FileInfo _stateFile;
        private readonly AsyncManualResetEvent _replicationFinishedEvent = new AsyncManualResetEvent(true);
        private readonly CancellationTokenSource _replicationTokenSource = new CancellationTokenSource();
        private readonly ReplicatorSettings _replicatorSettings;
        private readonly IBlobService _blobService;
        private readonly IReplicationLog _replicationLog;
        private readonly IServiceCredentials _serviceCredentials;
        private readonly HttpClient _httpClient;
        private readonly NamespaceId _namespace;
        private RefsState _refsState;
        private bool _replicationRunning;
        private bool _disposed = false;

        public RefsReplicator(ReplicatorSettings replicatorSettings, IOptionsMonitor<ReplicationSettings> replicationSettings, IBlobService blobService, IHttpClientFactory httpClientFactory, IReplicationLog replicationLog, IServiceCredentials serviceCredentials)
        {
            _name = replicatorSettings.ReplicatorName;
            _namespace = new NamespaceId(replicatorSettings.NamespaceToReplicate);
            _replicatorSettings = replicatorSettings;
            _blobService = blobService;
            _replicationLog = replicationLog;
            _serviceCredentials = serviceCredentials;

            _httpClient = httpClientFactory.CreateClient();
            _httpClient.BaseAddress = new Uri(replicatorSettings.ConnectionString);

            string stateFileName = $"{_name}.json";
            DirectoryInfo stateRoot = new DirectoryInfo(replicationSettings.CurrentValue.StateRoot);
            _stateFile = new FileInfo(Path.Combine(stateRoot.FullName, stateFileName));

            ReplicatorState? replicatorState = _replicationLog.GetReplicatorState(_namespace, _name).Result;
            if (replicatorState == null)
            {
                if (_stateFile.Exists)
                {
                    _refsState = ReadState(_stateFile)!;
                }
                else
                {
                    _refsState = new RefsState();
                }
            }
            else
            {
                _refsState = new RefsState()
                {
                    LastBucket = replicatorState.LastBucket,
                    LastEvent = replicatorState.LastEvent,
                };
            }

            Info = new ReplicatorInfo(replicatorSettings.ReplicatorName, _namespace, _refsState);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (_disposed)
            {
                return;
            }

            if (disposing)
            {
                SaveState(_stateFile, _refsState);

                _replicationFinishedEvent.Wait();
                _replicationTokenSource.Dispose();

                _httpClient.Dispose();
            }

            _disposed = true;
        }

        private static RefsState ReadState(FileInfo stateFile)
        {
            using StreamReader streamReader = stateFile.OpenText();
            using JsonReader reader = new JsonTextReader(streamReader);
            JsonSerializer serializer = new JsonSerializer();
            RefsState? state =  serializer.Deserialize<RefsState>(reader);
            if (state == null)
            {
                throw new Exception("Failed to read state");
            }

            return state;
        }

        private void SaveState(FileInfo stateFile, RefsState newState)
        {
            using StreamWriter writer = stateFile.CreateText();
            JsonSerializer serializer = new JsonSerializer();
            serializer.Serialize(writer, newState);

            _replicationLog.UpdateReplicatorState(Info.NamespaceToReplicate, _name, new ReplicatorState
            {
                LastBucket = newState.LastBucket,
                LastEvent = newState.LastEvent
            }).Wait();
        }

        public async Task<bool> TriggerNewReplications()
        {
            if (_replicationRunning)
            {
                _logger.Debug("Skipping replication of replicator: {Name} as it was already running.", _name);
                return false;
            }

            // read the state again to allow it to be modified by the admin controller / other instances of horde-storage connected to the same filesystem
            ReplicatorState? replicatorState = await _replicationLog.GetReplicatorState(_namespace, _name);
            if (replicatorState == null)
            {
                _stateFile.Refresh();
                if (_stateFile.Exists)
                {
                    _refsState = ReadState(_stateFile) ?? new RefsState();
                }
                else
                {
                    _refsState = new RefsState();
                }
            }
            else
            {
                _refsState = new RefsState()
                {
                    LastBucket = replicatorState.LastBucket,
                    LastEvent = replicatorState.LastEvent,
                };
            }

            _logger.Debug("Read Replication state for replicator: {Name}. {LastBucket} {LastEvent}.", _name, _refsState.LastBucket, _refsState.LastEvent);

            LogReplicationHeartbeat(0);

            bool hasRun;
            int countOfReplicationsDone = 0;

            try
            {
                //using IScope scope = Tracer.Instance.StartActive("replicator.run");
                //scope.Span.ResourceName =_name;

                _logger.Debug("Replicator: {Name} is starting a run.", _name);

                _replicationTokenSource.TryReset();
                _replicationRunning = true;
                _replicationFinishedEvent.Reset();
                CancellationToken replicationToken = _replicationTokenSource.Token;

                NamespaceId ns = _namespace;

                Info.CountOfRunningReplications = 0;

                string? lastBucket = null;
                Guid? lastEvent = null;

                bool haveRunBefore = _refsState.LastBucket != null && _refsState.LastEvent != null;
                if (!haveRunBefore && !_replicatorSettings.SkipSnapshot)
                {
                    // have not run before, replicate a snapshot
                    _logger.Information("{Name} Have not run replication before, attempting to use snapshot. State: {@State}", _name, _refsState);
                    try
                    {
                        (string eventBucket, Guid eventId, int countOfEventsReplicated) = await ReplicateFromSnapshot(ns, replicationToken);
                        countOfReplicationsDone += countOfEventsReplicated;

                        // finished replicating the snapshot, persist the state
                        _refsState.LastBucket = eventBucket;
                        _refsState.LastEvent = eventId;
                        SaveState(_stateFile, _refsState);
                        hasRun = true;
                    }
                    catch (NoSnapshotAvailableException)
                    {
                        // no snapshot available so we attempt a incremental replication with no bucket set
                        _refsState.LastBucket = null;
                        _refsState.LastEvent = null;
                    }
                }

                lastBucket = _refsState.LastBucket;
                lastEvent = _refsState.LastEvent;

                bool retry;
                bool haveAttemptedSnapshot = false;
                do
                {
                    retry = false;
                    UseSnapshotException? useSnapshotException = null;
                    try
                    {
                        countOfReplicationsDone += await ReplicateIncrementally(ns, lastBucket, lastEvent, replicationToken);
                    }
                    catch (AggregateException ae)
                    {
                        ae.Handle(e =>
                        {
                            if (e is UseSnapshotException snapshotException)
                            {
                                useSnapshotException = snapshotException;
                                return true;
                            }

                            return false;
                        });
                    }
                    catch (UseSnapshotException e)
                    {
                        useSnapshotException = e;
                    }

                    if (useSnapshotException != null)
                    {
                        // if we have already attempted to recover using a snapshot and we still fail we just give up as there is no content to fetch
                        if (haveAttemptedSnapshot)
                        {
                            break;
                        }

                        // if we fail to replicate incrementally we revert to using a snapshot
                        haveAttemptedSnapshot = true;

                        // if we are told to use a snapshot but are configured to not do so we will simply reset the state tracking and thus start from the oldest incremental event
                        // this will result in a partial replication
                        if (_replicatorSettings.SkipSnapshot)
                        {
                            lastBucket = null;
                            lastEvent = null;
                            retry = true;
                            continue;
                        }

                        (string eventBucket, Guid eventId, int countOfEventsReplicated) = await ReplicateFromSnapshot(ns, replicationToken, useSnapshotException.SnapshotBlob, INamespacePolicyResolver.JupiterInternalNamespace);
                        countOfReplicationsDone += countOfEventsReplicated;

                        // resume from these new events instead
                        lastBucket = eventBucket;
                        lastEvent = eventId;

                        _refsState.LastBucket = eventBucket;
                        _refsState.LastEvent = eventId;
                        SaveState(_stateFile, _refsState);
                        retry = true;
                    }
                } while (retry);
                hasRun = countOfReplicationsDone != 0;
            }
            finally
            {
                _replicationRunning = false;
                _replicationFinishedEvent.Set();
            }

            _logger.Debug("Replicator: {Name} finished its replication run. Replications completed: {ReplicationsDone} .", _name, countOfReplicationsDone);

            return hasRun;
        }

        private async Task<(string, Guid, int)> ReplicateFromSnapshot(NamespaceId ns, CancellationToken cancellationToken, BlobIdentifier? snapshotBlob = null, NamespaceId? blobNamespace = null)
        {
            // determine latest snapshot if no specific blob was specified
            if (snapshotBlob == null)
            {
                using HttpRequestMessage snapshotRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/replication-log/snapshots/{ns}", UriKind.Relative));
                HttpResponseMessage snapshotResponse = await _httpClient.SendAsync(snapshotRequest, cancellationToken);
                snapshotResponse.EnsureSuccessStatusCode();

                string s = await snapshotResponse.Content.ReadAsStringAsync(cancellationToken);
                ReplicationLogSnapshots? snapshots = JsonConvert.DeserializeObject<ReplicationLogSnapshots>(s);
                if (snapshots == null)
                {
                    throw new NotImplementedException();
                }

                SnapshotInfo? snapshotInfo = snapshots.Snapshots?.FirstOrDefault();
                if (snapshotInfo == null)
                {
                    throw new NoSnapshotAvailableException("No snapshots found");
                }

                snapshotBlob = snapshotInfo.SnapshotBlob;
                blobNamespace = snapshotInfo.BlobNamespace;
            }

            // process snapshot
            // fetch the snapshot from the remote blob store
            ReplicationLogSnapshot snapshot;
            {
                using HttpRequestMessage request = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/blobs/{blobNamespace}/{snapshotBlob}", UriKind.Relative));
                HttpResponseMessage response = await _httpClient.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
                response.EnsureSuccessStatusCode();
                await using Stream blobStream = await response.Content.ReadAsStreamAsync(cancellationToken);
                snapshot = ReplicationLogFactory.DeserializeSnapshotFromStream(blobStream);
            }

            if (!snapshot.LastEvent.HasValue)
            {
                throw new Exception("No last event found");
            }

            Guid snapshotEvent = snapshot.LastEvent.Value;
            string snapshotBucket = snapshot.LastBucket!;
            int countOfObjectsReplicated = 0;
            int countOfObjectsCurrentlyReplicating = 0;

            // if MaxParallelReplications is set to not limit we use the default behavior of ParallelForEachAsync which is to limit based on CPUs 
            int maxParallelism = _replicatorSettings.MaxParallelReplications != -1 ? _replicatorSettings.MaxParallelReplications : 0;
            await snapshot.GetLiveObjects().ParallelForEachAsync(async snapshotLiveObject =>
            {
                try
                {
                    using IScope scope = Tracer.Instance.StartActive("replicator.replicate_op_snapshot");
                    scope.Span.ResourceName = $"{ns}.{snapshotLiveObject.Blob}";
                    Interlocked.Increment(ref countOfObjectsCurrentlyReplicating);
                    LogReplicationHeartbeat(countOfObjectsCurrentlyReplicating);

                    if (countOfObjectsReplicated % 100 == 0)
                    {
                        _logger.Information("{Name} Snapshot replication still running, replicated {CountOfObjects} of {TotalCountOfObjects}. Replicating {Namespace}", _name, countOfObjectsReplicated, snapshot.LiveObjectsCount, ns);
                    }

                    Info.CountOfRunningReplications = countOfObjectsCurrentlyReplicating;
                    Info.LastRun = DateTime.Now;

                    await ReplicateOp(ns, snapshotLiveObject.Blob, cancellationToken);
                    // TODO: Avoid adding to the replication log as we will get into infinite recursion if we do with the remote site
                    // we could add to the replication log only if the op was missing, but that could cause issue for a 3rd site replicating from us
                    // and the 3rd site is the whole reason we even add this to replication log in the first place
                    //await AddToReplicationLog(ns, snapshotLiveObject.Bucket, snapshotLiveObject.Key, snapshotLiveObject.Blob);

                }
                finally
                {
                    Interlocked.Increment(ref countOfObjectsReplicated);
                    Interlocked.Decrement(ref countOfObjectsCurrentlyReplicating);
                }
            }, maxParallelism, cancellationToken);

            snapshot.Dispose();

            // snapshot processed, we proceed to reading the incremental state
            return (snapshotBucket, snapshotEvent, countOfObjectsReplicated);
        }

        private HttpRequestMessage BuildHttpRequest(HttpMethod httpMethod, Uri uri)
        {
            string? token = _serviceCredentials.GetToken();
            HttpRequestMessage request = new HttpRequestMessage(httpMethod, uri);
            if (!string.IsNullOrEmpty(token))
            {
                request.Headers.Add("Authorization", $"{_serviceCredentials.GetAuthenticationScheme()} {token}");
            }

            return request;
        }

        private async Task<int> ReplicateIncrementally(NamespaceId ns, string? lastBucket, Guid? lastEvent, CancellationToken replicationToken)
        {
            int countOfReplicationsDone = 0;
            _logger.Information("{Name} Looking for new transaction. Previous state: {@State}", _name, _refsState);

            SortedSet<long> replicationTasks = new();

            // if MaxParallelReplications is set to not limit we use the default behavior of ParallelForEachAsync which is to limit based on CPUs 
            int maxParallelism = _replicatorSettings.MaxParallelReplications != -1 ? _replicatorSettings.MaxParallelReplications : 0;

            _logger.Information("{Name} Starting incremental replication maxParallelism: {MaxParallelism} Last event: {LastEvent} Last Bucket {LastBucket}", _name, maxParallelism, lastEvent, lastBucket);

            using CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
            using CancellationTokenSource linkedTokenSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationTokenSource.Token, replicationToken);

            if (replicationToken.IsCancellationRequested)
            {
                return countOfReplicationsDone;
            }

            await GetRefEvents(ns, lastBucket, lastEvent, replicationToken).ParallelForEachAsync(async (ReplicationLogEvent @event) =>
            {
                using IScope scope = Tracer.Instance.StartActive("replicator.replicate_op_incremental");
                scope.Span.ResourceName = $"{ns}.{@event.Bucket}.{@event.Key}";
                scope.Span.SetTag("time-bucket", @event.Timestamp.ToString(CultureInfo.InvariantCulture));

                _logger.Information("{Name} New transaction to replicate found. Ref: {Namespace} {Bucket} {Key} in {TimeBucket} ({TimeDate}) with id {EventId}. Count of running replications: {CurrentReplications}", _name, @event.Namespace, @event.Bucket, @event.Key, @event.TimeBucket, @event.Timestamp, @event.EventId, replicationTasks.Count);
                
                Info.CountOfRunningReplications = replicationTasks.Count;
                LogReplicationHeartbeat(replicationTasks.Count);
                long currentOffset = Interlocked.Increment(ref countOfReplicationsDone);

                try
                {
                    string eventBucket = @event.TimeBucket;
                    Guid eventId = @event.EventId;

                    lock (replicationTasks)
                    {
                        replicationTasks.Add(currentOffset);
                    }

                    // we do not need to replicate delete events
                    if (@event.Op != ReplicationLogEvent.OpType.Deleted)
                    {
                        if (@event.Blob == null)
                        {
                            throw new Exception($"Event: {@event.Bucket} {@event.Key} in namespace {@event.Namespace} was missing a blob, unable to replicate it");
                        }

                        await ReplicateOp(@event.Namespace, @event.Blob, replicationToken);
                    }

                    // TODO: Avoid adding to the replication log as we will get into infinite recursion if we do with the remote site
                    // we could add to the replication log only if the op was missing, but that could cause issue for a 3rd site replicating from us
                    // and the 3rd site is the whole reason we even add this to replication log in the first place
                    // await AddToReplicationLog(@event.Namespace, @event.Bucket, @event.Key, @event.Blob);

                    bool wasOldest;
                    lock (replicationTasks)
                    {
                        wasOldest = currentOffset <= replicationTasks.First();
                    }

                    // only update the state when we have replicated everything up that point
                    if (wasOldest)
                    {
                        // we have replicated everything up to a point and can persist this in the state
                        _refsState.LastBucket = eventBucket;
                        _refsState.LastEvent = eventId;
                        SaveState(_stateFile, _refsState);

                        _logger.Information("{Name} replicated all events up to {Time} . Bucket: {EventBucket} Id: {EventId}", _name, @event.Timestamp, eventBucket, eventId);
                    }

                    Info.LastRun = DateTime.Now;
                    LogReplicationHeartbeat(replicationTasks.Count);
                }
                catch (BlobNotFoundException)
                {
                    _logger.Warning("{Name} Failed to replicate {@Op} in {Namespace} because blob was not present in remote store. Skipping.", _name, @event, Info.NamespaceToReplicate);
                }
                finally
                {
                    lock (replicationTasks)
                    {
                        replicationTasks.Remove(currentOffset);
                    }
                }
            } , maxParallelism, breakLoopOnException: true, cancellationToken: linkedTokenSource.Token);

            return countOfReplicationsDone;
        }

        private async Task ReplicateOp(NamespaceId ns, BlobIdentifier objectToReplicate, CancellationToken cancellationToken)
        {
            using IScope scope = Tracer.Instance.StartActive("replicator.replicate_op");
            scope.Span.ResourceName = $"{ns}.{objectToReplicate}";

            _logger.Information("Attempting to replicate object {Blob} in {Namespace}.", objectToReplicate, ns);

            // We could potentially do this, but that could be dangerous if missing child references
            // check if this blob exists locally before replicating, if it does we assume we have all of its references already
            //if (await _blobService.Exists(ns, blob))
            //    return currentOffset;

            using HttpRequestMessage referencesRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/objects/{ns}/{objectToReplicate}/references", UriKind.Relative));
            HttpResponseMessage referencesResponse = await _httpClient.SendAsync(referencesRequest, cancellationToken);
            string body = await referencesResponse.Content.ReadAsStringAsync(cancellationToken);

            if (referencesResponse.StatusCode == HttpStatusCode.BadRequest)
            {
                _logger.Warning("Failed to resolve references for object {Blob} in {Namespace}. Skipping replication", objectToReplicate, ns);
                return;
            }
            if (referencesResponse.StatusCode == HttpStatusCode.NotFound)
            {
                // objects that do not exist can not be replicated so we skip them
                _logger.Warning("Failed to resolve references for object {Blob} in {Namespace}. Got not found with message \"{Message}\". Skipping replication.", objectToReplicate, ns, referencesResponse.ReasonPhrase);
                return;
            }
            referencesResponse.EnsureSuccessStatusCode();

            ResolvedReferencesResult? refs = JsonConvert.DeserializeObject<ResolvedReferencesResult>(body);
            if (refs == null)
            {
                throw new Exception($"Unable to resolve references for object {objectToReplicate} in namespace {ns}");
            }

            BlobIdentifier[] potentialBlobs = new BlobIdentifier[refs.References.Length + 1];
            Array.Copy(refs.References, potentialBlobs, refs.References.Length);
            potentialBlobs[^1] = objectToReplicate;

            BlobIdentifier[] missingBlobs = await _blobService.FilterOutKnownBlobs(ns, potentialBlobs);
            Task[] blobReplicationTasks = new Task[missingBlobs.Length];
            for (int i = 0; i < missingBlobs.Length; i++)
            {
                BlobIdentifier blobToReplicate = missingBlobs[i];
                blobReplicationTasks[i] = Task.Run(async () =>
                {
                    HttpRequestMessage blobRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/blobs/{ns}/{blobToReplicate}", UriKind.Relative));
                    HttpResponseMessage blobResponse = await _httpClient.SendAsync(blobRequest, HttpCompletionOption.ResponseHeadersRead, cancellationToken);

                    _logger.Information("Attempting to replicate blob {Blob} in {Namespace}.", blobToReplicate, ns);

                    if (blobResponse.StatusCode == HttpStatusCode.NotFound)
                    {
                        _logger.Warning("Failed to replicate {Blob} in {Namespace} due to it not existing.", blobToReplicate, ns);
                        return;
                    }

                    if (blobResponse.StatusCode != HttpStatusCode.OK)
                    {
                        _logger.Error("Bad http response when replicating {Blob} in {Namespace} . Status code: {StatusCode}", blobToReplicate, ns, blobResponse.StatusCode);
                        return;
                    }

                    await using Stream s = await blobResponse.Content.ReadAsStreamAsync(cancellationToken);
                    long? contentLength = blobResponse.Content.Headers.ContentLength;
                            
                    using IBufferedPayload payload = contentLength is null or > int.MaxValue ? await FilesystemBufferedPayload.Create(s) : await MemoryBufferedPayload.Create(s);

                    await _blobService.PutObject(ns, payload, blobToReplicate);
                }, cancellationToken);
            }

            await Task.WhenAll(blobReplicationTasks);
        }

        private async IAsyncEnumerable<ReplicationLogEvent> GetRefEvents(NamespaceId ns, string? lastBucket, Guid? lastEvent, [EnumeratorCancellation] CancellationToken cancellationToken)
        {
            bool hasRunOnce = false;
            ReplicationLogEvents logEvents;
            do
            {
                if (cancellationToken.IsCancellationRequested)
                {
                    yield break;
                }

                if (hasRunOnce && (lastBucket == null || lastEvent == null))
                {
                    throw new Exception($"Failed to find state to resume from after first page of ref events, lastBucket: {lastBucket} lastEvent: {lastEvent}");
                }
                StringBuilder url = new StringBuilder($"/api/v1/replication-log/incremental/{ns}");
                // its okay for last bucket and last event to be null incase we have never run before, but after the first iteration we need them to keep track of where we were
                if (lastBucket != null)
                {
                    url.Append($"?lastBucket={lastBucket}");
                }

                if (lastEvent != null)
                {
                    url.Append($"&lastEvent={lastEvent}");
                }

                hasRunOnce = true;
                using HttpRequestMessage request = BuildHttpRequest(HttpMethod.Get, new Uri(url.ToString(), UriKind.Relative));
                HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
                string body = await response.Content.ReadAsStringAsync(cancellationToken);
                if (response.StatusCode == HttpStatusCode.BadRequest)
                {
                    ProblemDetails? problemDetails = JsonConvert.DeserializeObject<ProblemDetails>(body);
                    if (problemDetails == null)
                    {
                        throw new Exception($"Unknown bad request body when reading incremental replication log. Body: {body}");
                    }

                    if (problemDetails.Type == ProblemTypes.UseSnapshot)
                    {
                        ProblemDetailsWithSnapshots? problemDetailsWithSnapshots = JsonConvert.DeserializeObject<ProblemDetailsWithSnapshots>(body);

                        if (problemDetailsWithSnapshots == null)
                        {
                            throw new Exception($"Unable to cast the problem details to a snapshot version. Body: {body}");
                        }

                        BlobIdentifier snapshotBlob = problemDetailsWithSnapshots.SnapshotId;
                        throw new UseSnapshotException(snapshotBlob);
                    }
                }

                response.EnsureSuccessStatusCode();
                ReplicationLogEvents? e = JsonConvert.DeserializeObject<ReplicationLogEvents>(body);
                if (e == null)
                {
                    throw new Exception($"Unknown error when deserializing replication log events {ns} {lastBucket} {lastEvent}");
                }

                logEvents = e;
                foreach (ReplicationLogEvent logEvent in logEvents.Events)
                {
                    yield return logEvent;

                    lastBucket = logEvent.TimeBucket;
                    lastEvent = logEvent.EventId;
                }
            } while (logEvents.Events.Any());
        }

        private void LogReplicationHeartbeat(int countOfCurrentReplications)
        {
            
            // log message used to generate metric for how many replications are currently running
            _logger.Information("{Name} replication has run . Count of running replications: {CurrentReplications}", _name, countOfCurrentReplications);

            // log message used to verify replicators are actually running
            _logger.Information("{Name} starting replication. Last transaction was {TransactionId} {Generation}", _name, State.ReplicatorOffset.GetValueOrDefault(0L), State.ReplicatingGeneration.GetValueOrDefault(Guid.Empty) );
        }

        public void SetReplicationOffset(long? state)
        {
            throw new NotImplementedException();
        }

        public async Task StopReplicating()
        {
            if (_disposed)
            {
                return;
            }

            _replicationTokenSource.Cancel(true);
            await _replicationFinishedEvent.WaitAsync();
        }

        public ReplicatorState State => _refsState;

        public ReplicatorInfo Info { get; private set; }

        public Task DeleteState()
        {
            _refsState = new RefsState();
            SaveState(_stateFile, _refsState);

            return Task.CompletedTask;
        }

        public void SetRefState(string? lastBucket, Guid? lastEvent)
        {
            _refsState.LastBucket = lastBucket;
            _refsState.LastEvent = lastEvent;
            SaveState(_stateFile, _refsState);
        }
    }

    public class UseSnapshotException : Exception
    {
        public BlobIdentifier SnapshotBlob { get; }

        public UseSnapshotException(BlobIdentifier snapshotBlob)
        {
            SnapshotBlob = snapshotBlob;
        }
    }

    public class NoSnapshotAvailableException : Exception
    {
        public NoSnapshotAvailableException(string message) : base(message)
        {
        }
    }

    public class RefsState : ReplicatorState
    {
        public RefsState()
        {
            ReplicatingGeneration = null;
            ReplicatorOffset = 0;

            LastEvent = null;
            LastBucket = null;
        }
    }

    public class ProblemDetailsWithSnapshots : ProblemDetails
    {
        public BlobIdentifier SnapshotId { get; set; } = null!;
    }
}
