// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.DataModel;
using Amazon.DynamoDBv2.DocumentModel;
using Amazon.DynamoDBv2.Model;
using Datadog.Trace;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;

namespace Horde.Storage.Implementation
{
    public abstract class DynamoStore: IDisposable
    {
        private readonly IOptionsMonitor<DynamoDbSettings> _settings;
        private readonly SemaphoreSlim _initializeSemaphore = new SemaphoreSlim(1, 1);
        private bool _initialized = false;

        protected DynamoStore(IOptionsMonitor<DynamoDbSettings> settings, IAmazonDynamoDB client)
        {
            _settings = settings;
            Client = client;
            Context = CreateDynamoContext(client);
        }

        protected IDynamoDBContext Context { get; }

        protected IAmazonDynamoDB Client { get; }

        protected abstract Task CreateTables();

        public async Task Initialize()
        {
            if (_initialized)
            {
                return;
            }

            await _initializeSemaphore.WaitAsync();

            if (_initialized)
            {
                return;
            }

            try
            {
                await CreateTables();
                _initialized = true;
            }
            finally
            {
                _initializeSemaphore.Release();
            }
        }

        private static IDynamoDBContext CreateDynamoContext(IAmazonDynamoDB client)
        {
            return new DynamoDBContext(client, new DynamoDBContextConfig { Conversion = DynamoDBEntryConversion.V2 });
        }

        protected async Task CreateTable(string tableName, KeySchemaElement[] keySchemaElements, AttributeDefinition[] attributeDefinitions, ProvisionedThroughput provisionedThroughput, IEnumerable<GlobalSecondaryIndex>? secondaryIndices = null)
        {
            ListTablesResponse? tables = await Client.ListTablesAsync();
            if (tables.TableNames.Contains(tableName))
            {
                return;
            }

            bool shouldCreateTable = _settings.CurrentValue.CreateTablesOnDemand;

            if (shouldCreateTable)
            {
                using IScope scope = Tracer.Instance.StartActive("dynamo.create_tables");
                ISpan span = scope.Span;
                span.ResourceName = $"CREATE {tableName}";

                CreateTableRequest createTableRequest = new CreateTableRequest(tableName, keySchemaElements.ToList(), attributeDefinitions.ToList(), _settings.CurrentValue.UseOndemandCapacityProvisioning ? null : provisionedThroughput)
                {
                    BillingMode = _settings.CurrentValue.UseOndemandCapacityProvisioning ? BillingMode.PAY_PER_REQUEST : BillingMode.PROVISIONED,
                };

                if (secondaryIndices != null)
                {
                    // recreate the GlobalSecondaryIndex instance so we can control the provisioned thruput
                    createTableRequest.GlobalSecondaryIndexes.AddRange(from index in secondaryIndices
                        select new GlobalSecondaryIndex
                        {
                            IndexName = index.IndexName,
                            KeySchema = index.KeySchema,
                            Projection = index.Projection,
                            ProvisionedThroughput = _settings.CurrentValue.UseOndemandCapacityProvisioning ? null : provisionedThroughput
                        });
                }

                await Client.CreateTableAsync(createTableRequest);

                // wait for the table to have transitioned into active, e.g. that its finished being created
                while ((await Client.DescribeTableAsync(tableName)).Table.TableStatus != "ACTIVE")
                {
                    await Task.Delay(50);
                }
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _initializeSemaphore.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
    }

    public class DynamoJsonConverter : IPropertyConverter
    {
        public object? FromEntry(DynamoDBEntry entry)
        {
            Primitive? primitive = entry as Primitive;
            if (primitive == null || primitive.Value is not string value || string.IsNullOrEmpty(value))
            {
                throw new ArgumentOutOfRangeException(nameof(entry));
            }

            Dictionary<string, object>? ret = JsonConvert.DeserializeObject<Dictionary<string, object>>(value);
            return ret;
        }

        public DynamoDBEntry ToEntry(object value)
        {
            string jsonString = JsonConvert.SerializeObject(value);
            DynamoDBEntry ret = new Primitive(jsonString);
            return ret;
        }
    }
}
