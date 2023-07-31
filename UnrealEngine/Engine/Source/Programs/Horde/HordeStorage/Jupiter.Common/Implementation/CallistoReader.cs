// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using RestSharp;

namespace Jupiter.Implementation
{
    public class CallistoGetResponse
    {
        [JsonConstructor]
        public CallistoGetResponse(List<TransactionEvent> events, Guid generation, long currentOffset)
        {
            Events = events;
            Generation = generation;
            CurrentOffset = currentOffset;
        }

        public CallistoGetResponse()
        {
            Events = null!;
            Generation = Guid.Empty;
            CurrentOffset = 0;
        }

        public Guid Generation { get; set; }
        public long CurrentOffset { get; set; }
        public List<TransactionEvent> Events { get; init; }
    }

	public class CallistoReader
    {
        private readonly NamespaceId _ns;
        private readonly IRestClient _client;

        public CallistoReader(IRestClient callistoClient, NamespaceId ns)
        {
            _ns = ns;
            _client = callistoClient;
        }

        public async IAsyncEnumerable<TransactionEvent> GetOps(long startingOffset = 0, Guid? expectedReplicationGeneration = null, string? siteFilter = null, int? maxOffsetsAttempted = null, OpsEnumerationState? enumerationState = null, [EnumeratorCancellation] CancellationToken cancellationToken = default(CancellationToken))
        {
            if (expectedReplicationGeneration == null)
            {
                Transactions initialInfo = await FetchTransactions(0, countOfEventsToFetch: 1, siteFilter: null, replicatingGeneration: null, maxOffsetsAttempted: maxOffsetsAttempted, cancellationToken);
                expectedReplicationGeneration = initialInfo.ReplicatingGeneration;
            }

            if (cancellationToken.IsCancellationRequested)
            {
                yield break;
            }

            bool hadNewEvents;
            long currentOffset = startingOffset;
            do
            {
                Transactions currentBatch = await FetchTransactions(currentOffset, countOfEventsToFetch: 100, siteFilter, expectedReplicationGeneration, maxOffsetsAttempted: maxOffsetsAttempted, cancellationToken);

                hadNewEvents = currentBatch.Events?.Any() ?? false;
                currentOffset = currentBatch.LastOffset;

                if (enumerationState != null)
                {
                    enumerationState.ReplicatingGeneration = currentBatch.ReplicatingGeneration;
                    enumerationState.LastOffset = currentOffset;
                }

                if (currentBatch.Events == null)
                {
                    yield break;
                }

                foreach (TransactionEvent transactionEvent in currentBatch.Events)
                {
                    yield return transactionEvent;
                }
            } while (hadNewEvents);
        }

        private async Task<Transactions> FetchTransactions(long startingOffset, int countOfEventsToFetch, string? siteFilter, Guid? replicatingGeneration, int? maxOffsetsAttempted, CancellationToken replicationToken)
        {
            const int MaxAttempts = 3;
            CallistoGetResponse? callistoGetResponse = null;
            IRestResponse<CallistoGetResponse>? response = null;
            for (int i = 0; i < MaxAttempts; i++)
            {
                RestRequest request = new RestRequest("api/v1/t/{ns}/{offset}");
                request.AddUrlSegment("ns", _ns);
                request.AddUrlSegment("offset", startingOffset);

                request.AddQueryParameter("count", countOfEventsToFetch.ToString());

                if (siteFilter != null)
                {
                    request.AddQueryParameter("notSeenAtSite", siteFilter);
                }

                if (replicatingGeneration != null)
                {
                    request.AddQueryParameter("expectedGeneration", replicatingGeneration.ToString()!);
                }

                if (maxOffsetsAttempted.HasValue)
                {
                    request.AddQueryParameter("maxOffsetsAttempted", maxOffsetsAttempted.Value.ToString());
                }

                response = await _client.ExecuteGetAsync<CallistoGetResponse>(request, replicationToken);
                if (response.StatusCode == HttpStatusCode.BadRequest)
                {
                    JObject badRequestObj = JObject.Parse(response.Content);
                    if (badRequestObj.ContainsKey("type") && badRequestObj["type"]!.Value<string>() == "http://jupiter.epicgames.com/callisto/newGeneration")
                    {
                        string errorMsg = badRequestObj["title"]!.Value<string>()!;
                        throw new ReplicatingGenerationChangedException($"Generation mismatch when replicating remote callisto. Received error message: {errorMsg}");
                    }

                    if (badRequestObj.ContainsKey("type") && badRequestObj["type"]!.Value<string>() ==
                        "http://jupiter.epicgames.com/callisto/unknownNamespace")
                    {
                        throw new UnknownNamespaceException($"Namespace {_ns} was not known by Callisto.");
                    }

                    if (badRequestObj.ContainsKey("type") && badRequestObj["type"]!.Value<string>() ==
                        "http://jupiter.epicgames.com/callisto/transactionLogOffsetMismatch")
                    {
                        // increase offsets by a order of magnitude so we have a better chance of recovering
                        maxOffsetsAttempted = maxOffsetsAttempted.GetValueOrDefault(100) * 10;

                        // if we reached max attempts throw a exception other retry with more offsets attempted
                        if (i == MaxAttempts - 1)
                        {
                            string errorMsg = badRequestObj["title"]!.Value<string>()!;
                            throw new UnknownNamespaceException($"Transaction log mismatch in {_ns}. Error message {errorMsg}");
                        }
                        continue;
                    }

                    throw new Exception("Unknown bad request response from remote callisto: " + response.Content);
                }

                if (response.StatusCode == HttpStatusCode.BadGateway)
                {
                    // the request timed out, likely because we requested to many objects to reduce the size of the batch
                    countOfEventsToFetch = countOfEventsToFetch / 2;
                    continue;
                }

                if (!response.IsSuccessful)
                {
                    throw new Exception($"Unsuccessful response from remote callisto. Status Code: {response.StatusCode}. Url: {response.ResponseUri} . Message: {response.ErrorMessage}");
                }

                // we have data so stop retrying
                callistoGetResponse = response.Data;
                break;
            }

            if (callistoGetResponse == null)
            {
                throw new Exception($"Unsuccessful response from remote callisto, gave up after {MaxAttempts} attempts. Status Code: {response!.StatusCode}. Url: {response.ResponseUri} . Message: {response.ErrorMessage}");
            }

            List<TransactionEvent> events = callistoGetResponse.Events;
            if (events == null)
            {
                throw new Exception($"Invalid response from remote callisto: {_client} expected objects with events array");
            }

            if (events.Count == 0)
            {
                // nothing new
                return new Transactions
                {
                    ReplicatingGeneration = callistoGetResponse.Generation,
                    LastOffset = callistoGetResponse.CurrentOffset,
                    Events = null
                };
            }

            return new Transactions()
            {
                ReplicatingGeneration = callistoGetResponse.Generation,
                LastOffset = callistoGetResponse.CurrentOffset,
                Events = events
            };
        }

        private class Transactions
        {
            public Guid ReplicatingGeneration { get; set; }
            public long LastOffset { get; set; }
            public List<TransactionEvent>? Events { get; set; }
        }
    }
    public class OpsEnumerationState
    {
        public Guid ReplicatingGeneration { get; set; }
        public long LastOffset { get; set; }
    }

    public class UnknownNamespaceException : Exception
    {
        public UnknownNamespaceException(string message) : base(message)
        {
        }
    }

    public class ReplicatingGenerationChangedException : Exception
    {
        public ReplicatingGenerationChangedException(string message) : base(message)
        {
        }
    }
}
