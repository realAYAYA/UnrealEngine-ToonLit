// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Driver;
using Serilog;

namespace Horde.Storage.Implementation
{
    /// <summary>
    /// A specialized form of the Mongo refs store that adds some Cosmos specific management
    /// </summary>
    // ReSharper disable once ClassNeverInstantiated.Global
    public class CosmosRefsStore : MongoRefsStore
    {
        private readonly ILogger _logger = Log.ForContext<CosmosRefsStore>();
        private readonly IOptionsMonitor<CosmosSettings> _cosmosSettings;

        public CosmosRefsStore(IOptionsMonitor<MongoSettings> mongoSettings, IOptionsMonitor<CosmosSettings> cosmosSettings) : base(mongoSettings)
        {
            _cosmosSettings = cosmosSettings;
        }

        public override async Task Add(RefRecord record)
        {
            string dbName = GetDatabaseName(record.Namespace);

            bool collectionExists = await CollectionExistsAsync(dbName, ModelName);
            if (!collectionExists)
            {
                BsonDocument createDbCommand = new BsonDocument
                {
                    {
                        "customAction","CreateCollection"
                    },
                    {
                        "collection", ModelName
                    },
                    {
                        "offerThroughput",_cosmosSettings.CurrentValue.DefaultRU
                    }
                };
                IMongoDatabase? db = Client.GetDatabase(dbName);
                BsonDocument? result;
                try
                {
                    result = await db.RunCommandAsync<BsonDocument>(createDbCommand);
                }
                catch (MongoCommandException e)
                {
                    if (e.Code == 117) // http 409 - mongo 117
                    {
                        // the database got created by someone else, that is okay lets just use the db and ignore this error
                        result = null;
                    }
                    else
                    {
                        _logger.Warning("Mongo exception: {Code} {Exception}", e.Code, e.ToString());
                        // rethrow if we do not recognize the exception
                        throw;
                    }
                }

                if (result != null)
                {
                    //https://docs.microsoft.com/en-us/azure/cosmos-db/mongodb-custom-commands#default-output
                    int ok = result["ok"].AsInt32;
                    if (ok == 0) // failure
                    {
                        throw new Exception($"Failed to create MongoDB {dbName} in Cosmos. Response: {result}");
                    }
                }
            }

            await base.Add(record);
        }

        private async Task<bool> CollectionExistsAsync(string dbName, string collectionName)
        {
            IMongoDatabase? db = Client.GetDatabase(dbName);

            BsonDocument filter = new BsonDocument("name", collectionName);
            //filter by collection name
            IAsyncCursor<BsonDocument>? collections = await db.ListCollectionsAsync(new ListCollectionsOptions { Filter = filter });
            //check for existence
            return await collections.AnyAsync();
        }
    }
}
