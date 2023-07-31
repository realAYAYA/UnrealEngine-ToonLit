// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.DataModel;
using Amazon.DynamoDBv2.DocumentModel;
using Amazon.DynamoDBv2.Model;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Horde.Storage.Implementation
{
    public class DynamoNamespaceStore : DynamoStore
    {
        private const string NamespaceTableName = "Europa_Namespace";

        public DynamoNamespaceStore(IOptionsMonitor<DynamoDbSettings> settings, IAmazonDynamoDB client) : base(settings, client)
        {
        }

        public async Task AddToNamespaceTable(NamespaceId ns, NamespaceUsage usage)
        {
            // insert the namespace if it does not exist
            PutItemRequest putItem = new PutItemRequest(NamespaceTableName,
                new Dictionary<string, AttributeValue>
                {
                    { "Namespace", new AttributeValue(ns.ToString()) },
                    { "Usage", new AttributeValue {N = ((int)usage).ToString()}},

                })
            {
                ConditionExpression = "attribute_not_exists(Namespace)"
            };

            try
            {
                await Client.PutItemAsync(putItem);
            }
            catch (ConditionalCheckFailedException)
            {
                // if it already exists we do not need to do anything
            }
        }

        public async IAsyncEnumerable<NamespaceId> GetNamespaces(NamespaceUsage usage)
        {
            await Initialize();

            AsyncSearch<NamespaceMappingRecord> search = Context.ScanAsync<NamespaceMappingRecord>(new ScanCondition[]
            {
                new ScanCondition("Usage", ScanOperator.Equal, usage),
            });

            do
            {
                List<NamespaceMappingRecord> newSet = await search.GetNextSetAsync();
                foreach (NamespaceMappingRecord document in newSet)
                {
                    if (document.Namespace == null)
                    {
                        continue;
                    }

                    yield return new NamespaceId(document.Namespace);
                }
            } while (!search.IsDone);
        }

        protected override async Task CreateTables()
        {
            await CreateTable(NamespaceTableName, new[]
            {
                new KeySchemaElement("Namespace", KeyType.HASH),
            }, new[]
            {
                new AttributeDefinition("Namespace", ScalarAttributeType.S)
            }, new ProvisionedThroughput
            {
                // hard coding capacity as it should be really low for this table
                ReadCapacityUnits = 20,
                WriteCapacityUnits = 10,
            });

        }

        public async Task RemoveNamespace(NamespaceId ns)
        {
            try
            {
                await Context.DeleteAsync(new Primitive(ns.ToString(), true));
            }
            catch (ResourceNotFoundException)
            {
                // if the table does note exist it is considered a success
            }
        }

        public enum NamespaceUsage
        {
            // TODO: This should be called Ref but we keep it for legacy reasons
            Cache,
            Tree
        }

        [DynamoDBTable(NamespaceTableName)]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Used by serialization")]
        public class NamespaceMappingRecord
        {
            [DynamoDBHashKey] public string Namespace { get; set; } = null!;

            public NamespaceUsage Usage { get; set; }
        }
    }
}
