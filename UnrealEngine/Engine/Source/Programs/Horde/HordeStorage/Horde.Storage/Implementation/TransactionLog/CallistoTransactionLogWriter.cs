// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Serilog;
using Microsoft.Extensions.Options;
using RestSharp;
using RestSharp.Serializers.NewtonsoftJson;
using EpicGames.Horde.Storage;

namespace Horde.Storage.Implementation
{
    internal class CallistoTransactionLogWriter : ITransactionLogWriter
    {
        private readonly ILogger _logger = Log.ForContext<CallistoTransactionLogWriter>();
        private readonly IRestClient _client;

        public CallistoTransactionLogWriter(IOptionsMonitor<CallistoTransactionLogSettings> settings, IServiceCredentials serviceCredentials)
        {
            _client = new RestClient(settings.CurrentValue.ConnectionString).UseSerializer(() => new JsonNetSerializer());
            _client.Authenticator = serviceCredentials.GetAuthenticator();
        }

        public async Task<long> Add(NamespaceId ns, TransactionEvent @event)
        {
            const int retryAttempts = 15;
            for (int i = 0; i < retryAttempts; i++)
            {
                RestRequest request = new RestRequest("api/v1/t/{ns}");
                request.AddUrlSegment("ns", ns);
                request.AddJsonBody(@event);

                _logger.Verbose("Sending record {@Record} to callisto", @event);

                IRestResponse<CallistoResponse> response = await _client.ExecutePostAsync<CallistoResponse>(request);
                if (!response.IsSuccessful)
                {
                    // if callisto is down we wait a while and try again
                    if (response.StatusCode == 0 && response.ErrorMessage.StartsWith("Connection refused", StringComparison.OrdinalIgnoreCase))
                    {
                        await Task.Delay(1000);
                        continue; // retry
                    }

                    // unknown error, throw exception
                    throw new Exception($"Failed to put add record in Callisto. Status: {response.StatusCode}. Message: {response.ErrorMessage}. Body: {response.Content}");
                }
                
                return response.Data.Offset;
            }

            throw new Exception($"Failed to reach callisto after {retryAttempts} attempts");
        }

        public async Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            RestRequest request = new("api/v1/t/{ns}");
            CallistoRequest record = new("Delete", key, bucket);
            request.AddUrlSegment("ns", ns);
            request.AddJsonBody(record);
            
            _logger.Verbose("Sending delete record {@Record} to callisto", record);

            CallistoResponse response = await _client.PostAsync<CallistoResponse>(request);

            if (response == null)
            {
                throw new Exception("Failed to put delete record in Callisto.");
            }

            return response.Offset;
        }
    }

    internal class CallistoRequest
    {
        public CallistoRequest(string type, KeyId name, BucketId bucket)
        {
            Type = type;
            Name = name;
            Bucket = bucket;
        }

        public string Type { get; }
        public KeyId Name { get; }
        public BucketId Bucket { get; }
    }

    internal class CallistoResponse
    {
        public long Offset { get; set; }
    }
}
