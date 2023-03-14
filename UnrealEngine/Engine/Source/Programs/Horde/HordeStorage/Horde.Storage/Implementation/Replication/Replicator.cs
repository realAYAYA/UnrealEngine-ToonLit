// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Nito.AsyncEx;
using Nito.AsyncEx.Synchronous;
using RestSharp;
using RestSharp.Serializers.NewtonsoftJson;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class ReplicatorV1 : Replicator<TransactionEvent>
    {
        private readonly ITransactionLogWriter _transactionLogWriter;

        public ReplicatorV1(ReplicatorSettings replicatorSettings, IOptionsMonitor<ReplicationSettings> replicationSettings, IOptionsMonitor<JupiterSettings> jupiterSettings, IBlobService blobService, ITransactionLogWriter transactionLogWriter, IServiceCredentials serviceCredentials, IHttpClientFactory httpClientFactory) : base(replicatorSettings, replicationSettings, jupiterSettings, blobService, CreateRemoteClient(replicatorSettings, serviceCredentials), serviceCredentials, httpClientFactory)
        {
            _transactionLogWriter = transactionLogWriter;
        }

        internal ReplicatorV1(ReplicatorSettings replicatorSettings, IOptionsMonitor<ReplicationSettings> replicationSettings, IOptionsMonitor<JupiterSettings> jupiterSettings, IBlobService blobService, ITransactionLogWriter transactionLogWriter, IRestClient remoteClient, IServiceCredentials serviceCredentials, IHttpClientFactory httpClientFactory) : base(replicatorSettings, replicationSettings, jupiterSettings, blobService, remoteClient, serviceCredentials, httpClientFactory)
        {
            _transactionLogWriter = transactionLogWriter;
        }

        protected override IAsyncEnumerable<TransactionEvent> GetCallistoOp(long stateReplicatorOffset, Guid? stateReplicatingGeneration, string currentSite,
            OpsEnumerationState enumerationState, CancellationToken replicationToken)
        {
            CallistoReader remoteCallistoReader = new CallistoReader(Client, Namespace);

            return remoteCallistoReader.GetOps(stateReplicatorOffset, stateReplicatingGeneration, currentSite, enumerationState: enumerationState, cancellationToken: replicationToken, maxOffsetsAttempted: ReplicatorSettings.MaxOffsetsAttempted);
        }

        protected override async Task ReplicateOp(IRestClient remoteClient, TransactionEvent op, CancellationToken replicationToken)
        {
            using IScope scope = Tracer.Instance.StartActive("replicator.replicate_blobs");
            NamespaceId ns = Namespace;

            // now we replicate the blobs
            switch (op)
            {
                case AddTransactionEvent addEvent:
                    BlobIdentifier[] blobs = addEvent.Blobs;
                    await ReplicateBlobs(ns, blobs, replicationToken);
                    break;
                case RemoveTransactionEvent removeEvent:
                    // TODO: Do we even want to do anything? we can not delete the blob in the store because it may be used by something else
                    // so we wait for the GC to remove it.
                    break;
                default:
                    throw new NotImplementedException("Unknown op type {op}");
            }
        }

        protected override async Task<long?> ReplicateOpInline(IRestClient remoteClient, TransactionEvent op, CancellationToken replicationToken)
        {
            using IScope scope = Tracer.Instance.StartActive("replicator.replicate_inline");
            //scope.Span.ResourceName = op.transactionId;

            NamespaceId ns = Namespace;

            // Put a copy of the event in the local callisto store
            // Make sure this is put before the io put request as we want to pin the reference before uploading the content to avoid the gc removing it right away
            long newTransactionId = await _transactionLogWriter.Add(ns, op);

            Logger.Information("{Name} Replicated Op {@Op} to local store as transaction id {@TransactionId}", Name, op, newTransactionId);

            // TODO: Add Refstore replication
            //_refStore.Add();

            return newTransactionId;
        }
    }

    public abstract class Replicator<T> : IReplicator
        where T: IReplicationEvent
    {
        private readonly IBlobService _blobService;
        private readonly string _currentSite;

        private readonly FileInfo _stateFile;
        private bool _replicationRunning;
        private readonly AsyncManualResetEvent _replicationFinishedEvent = new AsyncManualResetEvent(true);
        private readonly CancellationTokenSource _replicationTokenSource = new CancellationTokenSource();
        private readonly IServiceCredentials _serviceCredentials;
        private readonly HttpClient _httpClient;

        protected string Name { get; set; }
        public NamespaceId Namespace { get; set; }
        protected ReplicatorSettings ReplicatorSettings { get; init; }

        public ReplicatorState State { get; private set; }
        protected ILogger Logger { get; } = Log.ForContext<Replicator<T>>();
        public ReplicatorInfo Info { get; }
        protected IRestClient Client { get; }

        protected static IRestClient CreateRemoteClient(ReplicatorSettings replicatorSettings, IServiceCredentials serviceCredentials)
        {
            IRestClient remoteClient = new RestClient(replicatorSettings.ConnectionString).UseSerializer(() => new JsonNetSerializer());
            remoteClient.Authenticator = serviceCredentials.GetAuthenticator();
            return remoteClient;
        }

        protected Replicator(ReplicatorSettings replicatorSettings, IOptionsMonitor<ReplicationSettings> replicationSettings, IOptionsMonitor<JupiterSettings> jupiterSettings, IBlobService blobService, IRestClient remoteClient, IServiceCredentials serviceCredentials, IHttpClientFactory httpClientFactory)
        {
            Name = replicatorSettings.ReplicatorName;
            Namespace = new NamespaceId(replicatorSettings.NamespaceToReplicate);
            _blobService = blobService;
            _currentSite = jupiterSettings.CurrentValue.CurrentSite;
            ReplicatorSettings = replicatorSettings;

            string stateFileName = $"{Name}.json";
            DirectoryInfo stateRoot = new DirectoryInfo(replicationSettings.CurrentValue.StateRoot);
            _stateFile = new FileInfo(Path.Combine(stateRoot.FullName, stateFileName));

            if (_stateFile.Exists)
            {
                State = ReadState(_stateFile) ?? new ReplicatorState {ReplicatorOffset = null, ReplicatingGeneration = null};
            }
            else
            {
                State = new ReplicatorState {ReplicatorOffset = null, ReplicatingGeneration = null};
            }

            Info = new ReplicatorInfo(ReplicatorSettings.ReplicatorName, Namespace, State);
            Client = remoteClient;

            _serviceCredentials = serviceCredentials;

            _httpClient = httpClientFactory.CreateClient();
            _httpClient.BaseAddress = new Uri(replicatorSettings.ConnectionString);
        }

        ~Replicator()
        {
            Dispose(false);
        }

        /// <summary>
        /// Attempt to run a new replication, if a replication is already in flight for this replicator this will early exist.
        /// </summary>
        /// <returns>True if the replication actually attempted to run</returns>
        public async Task<bool> TriggerNewReplications()
        {
            if (_replicationRunning)
            {
                return false;
            }

            bool hasRun = false;
            OpsEnumerationState enumerationState = new();
            DateTime startedReplicationAt = DateTime.Now;
            long countOfReplicatedEvents = 0L;
            try
            {
                //using IScope scope = Tracer.Instance.StartActive("replicator.run");
                //scope.Span.ResourceName =_name;

                _replicationRunning = true;
                _replicationFinishedEvent.Reset();
                CancellationToken replicationToken = _replicationTokenSource.Token;

                // read the state again to allow it to be modified by the admin controller
                _stateFile.Refresh();
                if (_stateFile.Exists)
                {
                    State = ReadState(_stateFile) ?? new ReplicatorState {ReplicatorOffset = null, ReplicatingGeneration = null};
                }
                else
                {
                    State = new ReplicatorState {ReplicatorOffset = null, ReplicatingGeneration = null};
                }

                LogReplicationHeartbeat(Name, State, 0);
                Logger.Information("{Name} Looking for new transaction. Previous state: {@State}", Name, State);

                SortedList<long, Task> replicationTasks = new();
                await foreach (T op in GetCallistoOp(State.ReplicatorOffset ?? 0, State.ReplicatingGeneration, _currentSite, enumerationState, replicationToken))
                {
                    hasRun = true;
                    Logger.Information("{Name} New transaction to replicate found. New op: {@Op} . Count of running replications: {CurrentReplications}", Name, op, replicationTasks.Count);

                    Info.CountOfRunningReplications = replicationTasks.Count;

                    if (!op.Identifier.HasValue || !op.NextIdentifier.HasValue)
                    {
                        Logger.Error("{Name} Missing identifier in {@Op}", Name, op);
                        throw new Exception("Missing identifier in op. Version mismatch?");
                    }
                    
                    long currentOffset = op.Identifier.Value;
                    long nextIdentifier = op.NextIdentifier.Value;
                    Guid replicatingGeneration = enumerationState.ReplicatingGeneration;
                    try
                    {
                        // first replicate the things which are order sensitive like the transaction log
                        long? _ = await ReplicateOpInline(Client, op, replicationToken);

                        // next we replicate the large things that overlap with other replications like blobs
                        Task currentReplicationTask = ReplicateOp(Client, op, replicationToken);

                        Task OnDone(Task task)
                        {
                            Interlocked.Increment(ref countOfReplicatedEvents);
                            try
                            {
                                bool wasOldest;
                                lock (replicationTasks)
                                {
                                    wasOldest = currentOffset <= replicationTasks.First().Key;
                                }

                                if (task.IsFaulted)
                                {
                                    // rethrow the exception if there was any
                                    task.WaitAndUnwrapException(replicationToken);
                                }

                                // only update the state when we have replicated everything up that point
                                if (wasOldest)
                                {
                                    // we have finished replicating 
                                    State.ReplicatorOffset = nextIdentifier;
                                    State.ReplicatingGeneration = replicatingGeneration;
                                    SaveState(_stateFile, State);
                                }

                                Info.LastRun = DateTime.Now;
                                Info.CountOfRunningReplications = replicationTasks.Count;

                                LogReplicationHeartbeat(Name, State, replicationTasks.Count);

                                return task;
                            }
                            finally
                            {
                                lock (replicationTasks)
                                {
                                    replicationTasks.Remove(currentOffset);
                                }
                            }
                        }

                        lock (replicationTasks)
                        {
                            Task<Task> commitStateTask = currentReplicationTask.ContinueWith(OnDone, replicationToken, TaskContinuationOptions.None, TaskScheduler.Current);

                            replicationTasks.Add(currentOffset, commitStateTask);
                        }
                    }
                    catch (BlobNotFoundException)
                    {
                        Logger.Warning("{Name} Failed to replicate {@Op} in {Namespace} because blob was not present in remote store. Skipping.", Name, op, Info.NamespaceToReplicate);
                    }

                    // if we have reached the max amount of parallel replications we wait for one of them to finish before starting a new one
                    // if max replications is set to -1 we do not limit the concurrency
                    if ( ReplicatorSettings.MaxParallelReplications != -1 && replicationTasks.Count >= ReplicatorSettings.MaxParallelReplications)
                    {
                        Task[] currentReplicationTasks;
                        lock (replicationTasks)
                        {
                            currentReplicationTasks = replicationTasks.Values.ToArray();
                        }
                        await Task.WhenAny(currentReplicationTasks);
                    }

                    Info.CountOfRunningReplications = replicationTasks.Count;
                    LogReplicationHeartbeat(Name, State, replicationTasks.Count);

                    if (replicationToken.IsCancellationRequested)
                    {
                        break;
                    }
                }

                // make a copy to avoid the collection being modified while we wait for it
                List<Task> tasksToWaitFor = replicationTasks.Values.ToList();
                await Task.WhenAll(tasksToWaitFor);
            }
            finally
            {
                _replicationRunning = false;
                _replicationFinishedEvent.Set();
            }

            DateTime endedReplicationAt = DateTime.Now;
            Logger.Information("{Name} {Namespace} Replication Finished at {EndTime} and last run was at {LastTime}, replicated {CountOfEvents} events. Thus was {TimeDifference} minutes behind.", Name, Info.NamespaceToReplicate, endedReplicationAt, startedReplicationAt, countOfReplicatedEvents, (endedReplicationAt - startedReplicationAt).TotalMinutes);

            if (!hasRun)
            {
                // if no events were found we still persist the offsets we have run thru so we don't have to read them again.
                State.ReplicatorOffset = enumerationState.LastOffset;
                State.ReplicatingGeneration = enumerationState.ReplicatingGeneration;
                SaveState(_stateFile, State);
            }

            return hasRun;
        }

        protected abstract IAsyncEnumerable<T> GetCallistoOp(long stateReplicatorOffset, Guid? stateReplicatingGeneration, string currentSite, OpsEnumerationState enumerationState, CancellationToken replicationToken);
        
        private void LogReplicationHeartbeat(string name, ReplicatorState? state, int countOfCurrentReplications)
        {
            if (state == null)
            {
                return;
            }

            // log message used to generate metric for how many replications are currently running
            Logger.Information("{Name} replication has run . Count of running replications: {CurrentReplications}", Name, countOfCurrentReplications);

            // log message used to verify replicators are actually running
            Logger.Information("{Name} starting replication. Last transaction was {TransactionId} {Generation}", name, state.ReplicatorOffset.GetValueOrDefault(0L), state.ReplicatingGeneration.GetValueOrDefault(Guid.Empty) );
        }

        protected abstract Task<long?> ReplicateOpInline(IRestClient remoteClient, T op, CancellationToken replicationToken);

        protected abstract Task ReplicateOp(IRestClient remoteClient, T op, CancellationToken replicationToken);

        protected async Task ReplicateBlobs(NamespaceId ns, BlobIdentifier[] blobs, CancellationToken replicationToken)
        {
            Task[] blobReplicateTasks = new Task[blobs.Length];
            for (int index = 0; index < blobs.Length; index++)
            {
                BlobIdentifier blob = blobs[index];
                blobReplicateTasks[index] = Task.Run(async () =>
                {
                    // check if this blob exists locally before replicating
                    if (await _blobService.Exists(ns, blob))
                    {
                        return;
                    }

                    // attempt to replicate for a few tries with some delay in between
                    // this because new transactions are written to callisto first (to establish the GC handle) before content is uploaded to io.
                    // as such its possible (though not very likely) when we replicate the very newest transaction that we find a transaction but no content
                    // and as the content upload can be large it can take a little time for it to exist in io.
                    BlobIdentifier? calculatedBlob = null;
                    const int MaxAttempts = 3;
                    for (int attempts = 0; attempts < MaxAttempts; attempts++)
                    {
                        HttpRequestMessage blobRequest = BuildHttpRequest(HttpMethod.Get, $"api/v1/s/{ns}/{blob}");
                        HttpResponseMessage response = await _httpClient.SendAsync(blobRequest, HttpCompletionOption.ResponseHeadersRead, replicationToken);

                        if (response.StatusCode == HttpStatusCode.NotFound)
                        {
                            Logger.Information("{Blob} was not found in remote blob store. Retry attempt {Attempts}", blob, attempts);
                            await Task.Delay(TimeSpan.FromSeconds(5), replicationToken);
                            continue;
                        }

                        if (response.StatusCode == HttpStatusCode.GatewayTimeout)
                        {
                            Logger.Information("GatewayTimeout while replicating {Blob}, retrying. Retry attempt {Attempts}", blob, attempts);
                            continue;
                        }

                        if (response.StatusCode == HttpStatusCode.BadGateway)
                        {
                            Logger.Information("BadGateway while replicating {Blob}, retrying. Retry attempt {Attempts}", blob, attempts);
                            continue;
                        }

                        if (response.StatusCode == HttpStatusCode.BadRequest || response.StatusCode == HttpStatusCode.NotFound)
                        {
                            Logger.Warning("Remote blob {Blob} missing, unable to replicate.", blob);
                            return;
                        }

                        if (response.IsSuccessStatusCode)
                        {
                            long? contentLength = response.Content.Headers.ContentLength;
                            await using Stream responseStream = await response.Content.ReadAsStreamAsync(replicationToken);
                            
                            using IBufferedPayload payload = contentLength is null or > int.MaxValue ? await FilesystemBufferedPayload.Create(responseStream) : await MemoryBufferedPayload.Create(responseStream);

                            {
                                await using Stream s = payload.GetStream();
                                calculatedBlob = await BlobIdentifier.FromStream(s);
                            }
                            if (!blob.Equals(calculatedBlob))
                            {
                                Logger.Warning("Mismatching blob when replicating {Blob}. Determined Hash was {Hash} size was {Size} HttpStatusCode {StatusCode}",
                                    blob, calculatedBlob, payload.Length, response.StatusCode);
                                continue; // attempt to replicate again
                            }

                            await _blobService.PutObject(ns, payload, calculatedBlob);
                            break;
                        }

                        throw new Exception($"Replicator \"{Name}\" failed to replicate blob {blob} due unsuccessful status from remote blob store: {response.StatusCode} . Error message: \"{response.ReasonPhrase}\"");
                    }
                }, replicationToken);
            }

            await Task.WhenAll(blobReplicateTasks);
        }

        private static ReplicatorState? ReadState(FileInfo stateFile)
        {
            using StreamReader streamReader = stateFile.OpenText();
            using JsonReader reader = new JsonTextReader(streamReader);
            JsonSerializer serializer = new JsonSerializer();
            return serializer.Deserialize<ReplicatorState>(reader);
        }

        private static void SaveState(FileInfo stateFile, ReplicatorState newState)
        {
            using StreamWriter writer = stateFile.CreateText();
            JsonSerializer serializer = new JsonSerializer();
            serializer.Serialize(writer, newState);
        }

        private HttpRequestMessage BuildHttpRequest(HttpMethod httpMethod, string uri)
        {
            string? token = _serviceCredentials.GetToken();
            HttpRequestMessage request = new HttpRequestMessage(httpMethod, uri);
            if (!string.IsNullOrEmpty(token))
            {
                request.Headers.Add("Authorization", $"{_serviceCredentials.GetAuthenticationScheme()} {token}");
            }

            return request;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                SaveState(_stateFile, State);

                _replicationTokenSource.Dispose();
                _httpClient.Dispose();
            }
        }

        public async Task StopReplicating()
        {
            _replicationTokenSource.Cancel(true);
            await _replicationFinishedEvent.WaitAsync();
        }

        public void SetReplicationOffset(long? offset)
        {
            State.ReplicatorOffset = offset;
            SaveState(_stateFile, State);
        }

        public Task DeleteState()
        {
            State = new ReplicatorState()
            {
                ReplicatingGeneration = null,
                ReplicatorOffset = 0
            };
            SaveState(_stateFile, State);

            return Task.CompletedTask;
        }
    }
}
