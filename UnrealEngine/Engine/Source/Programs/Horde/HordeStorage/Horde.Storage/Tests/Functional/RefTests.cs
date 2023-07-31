// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Dasync.Collections;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Serilog;
using Serilog.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Horde.Storage.FunctionalTests.Ref
{

    [TestClass]
    public class MongoRefTests : RefTests
    {
        private static MongoSettings? _mongoSettings;
        private MongoRefsStore? _refsStore;

        private static MongoClient GetMongoClient()
        {
            return new MongoClient(_mongoSettings!.ConnectionString);
        }

        protected override async Task SeedDb(IServiceProvider provider)
        {
            _mongoSettings = provider.GetService<IOptionsMonitor<MongoSettings>>()!.CurrentValue;

            MongoClient client = GetMongoClient();
            foreach (NamespaceId ns in new[] {TestNamespace, LastAccessTestNamespace})
            {
                string dbName = $"Europa_{ns}";
                if (client.GetDatabase(dbName) != null)
                {
                    await client.DropDatabaseAsync(dbName);
                }
            }

            //verify we are using the expected refs store
            IRefsStore? refStore = provider.GetService<IRefsStore>();
            Assert.IsTrue(RefStoreIs(refStore, typeof(MongoRefsStore)));

            _refsStore = new MongoRefsStore(provider.GetService<IOptionsMonitor<MongoSettings>>()!);
            IBlobService blobStore = (IBlobService)provider.GetService(typeof(IBlobService))!;

            await SeedRefTestData(_refsStore, blobStore);

        }

        protected override Task TeardownDb(IServiceProvider provider)
        {
            return Task.CompletedTask;
        }

        protected override async Task<RefRecord> GetTestRecord(BucketId bucket, KeyId name, IRefsStore.ExtraFieldsFlag fields)
        {
            RefRecord? record = await _refsStore!.Get(TestNamespace, bucket, name, fields);
            if (record == null)
            {
                throw new Exception("Unable to find record");
            }

            return record;
        }
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] {new KeyValuePair<string, string>("Horde_Storage:RefDbImplementation", HordeStorageSettings.RefDbImplementations.Mongo.ToString())};
        }
    }

    [TestClass]
    public class MemoryRefTests : RefTests
    {
        private MemoryRefsStore? _refsStore;

        protected override async Task SeedDb(IServiceProvider provider)
        {
            //verify we are using the expected refs store
            IRefsStore? refStore = provider.GetService<IRefsStore>();
            Assert.IsTrue(RefStoreIs(refStore, typeof(MemoryRefsStore)));
            if (refStore is CachedRefStore cachedRefStore)
            {
                _refsStore = (MemoryRefsStore)cachedRefStore.BackingStore;
            }
            else
            {
                _refsStore = (MemoryRefsStore)refStore!;
            }

            IBlobService blobStore = (IBlobService)provider.GetService(typeof(IBlobService))!;

            await SeedRefTestData(_refsStore, blobStore);

        }

        protected override Task TeardownDb(IServiceProvider provider)
        {
            return Task.CompletedTask;
        }

        protected override async Task<RefRecord> GetTestRecord(BucketId bucket, KeyId name, IRefsStore.ExtraFieldsFlag fields)
        {
            RefRecord? record = await _refsStore!.Get(TestNamespace, bucket, name, fields);
            if (record == null)
            {
                throw new Exception("Unable to find record");
            }

            return record;
        }
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] {new KeyValuePair<string, string>("Horde_Storage:RefDbImplementation", HordeStorageSettings.RefDbImplementations.Memory.ToString())};
        }
    }

    [TestClass]
    public class DynamoRefTests : RefTests, IDisposable
    {
        private DynamoDbRefsStore? _refStore;
        private readonly string[] _tablesToDelete = new[] {"Europa_Cache", "Europa_Namespace"};

        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] {new KeyValuePair<string, string>("Horde_Storage:RefDbImplementation", HordeStorageSettings.RefDbImplementations.DynamoDb.ToString())};
        }

        protected override async Task SeedDb(IServiceProvider provider)
        {
            IOptionsMonitor<DynamoDbSettings> dynamoSettings = provider.GetService<IOptionsMonitor<DynamoDbSettings>>()!;

            //verify we are using the expected refs store
            IRefsStore? cacheStore = provider.GetService<IRefsStore>();
            Assert.IsTrue(RefStoreIs(cacheStore, typeof(DynamoDbRefsStore)));

            _refStore = new DynamoDbRefsStore(dynamoSettings, provider.GetService<IAmazonDynamoDB>()!, provider.GetService<DynamoNamespaceStore>()!);
            IBlobService blobService = provider.GetService<IBlobService>()!;

            await SeedRefTestData(_refStore, blobService);
        }

        protected override async Task TeardownDb(IServiceProvider provider)
        {
            // delete tables after use so that we always run from a clean slate
            // you can remove this if you want to look at the resulting state after a run
            IAmazonDynamoDB client = provider.GetService<IAmazonDynamoDB>()!;
            ListTablesResponse tables = await client.ListTablesAsync();
            foreach (string t in _tablesToDelete)
            {
                if (tables.TableNames.Contains(t))
                {
                    await client.DeleteTableAsync(t);
                }
            }
        }

        protected override async Task<RefRecord> GetTestRecord(BucketId bucket, KeyId name, IRefsStore.ExtraFieldsFlag fields)
        {
            RefRecord? record = await _refStore!.Get(TestNamespace, bucket, name, fields);
            if (record == null)
            {
                throw new Exception("Unable to find record");
            }

            return record;
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _refStore?.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            System.GC.SuppressFinalize(this);
        }
    }
    
    public abstract class RefTests
    {
        private static TestServer? _server;
        private static HttpClient? _httpClient;
        private static ILastAccessCache<RefRecord>? _lastAccessCache;

        private const string TestObjectContents = "Test Object Data";
        private static readonly byte[] TestObjectData = Encoding.ASCII.GetBytes(TestObjectContents);
        private static readonly ContentHash TestObjectHash = ContentHash.FromBlob(TestObjectData);
        private static readonly BlobIdentifier TestObjectBlobHash = BlobIdentifier.FromContentHash(TestObjectHash);

        protected NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace");
        protected NamespaceId LastAccessTestNamespace { get; } = new NamespaceId("test-namespace-last-access");

        [TestInitialize]
        public async Task Setup()

        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", true)
                .AddInMemoryCollection(GetSettings())
                .AddEnvironmentVariables()
                .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<HordeStorageStartup>()
            );
            _httpClient = server.CreateClient();
            _server = server;

            _lastAccessCache = _server.Services.GetService<ILastAccessCache<RefRecord>>();

            await SeedDb(server.Services);
        }

        protected async Task SeedRefTestData(IRefsStore refsStore, IBlobService blobStore)
        {
            // add the objects as if they were last accessed a day ago
            DateTime lastAccessTime = DateTime.Now.AddDays(-1);

            BucketId bucket = new BucketId("bucket");

            foreach (NamespaceId ns in new [] {TestNamespace, LastAccessTestNamespace})
            {
                await refsStore.Add(new RefRecord(ns, bucket, new KeyId("testObject"), blobs: new[] { TestObjectBlobHash },
                    lastAccessTime: lastAccessTime, contentHash: TestObjectHash, metadata: null));
                await blobStore.PutObject(ns, TestObjectData, TestObjectBlobHash);

                // a object with metadata
                await refsStore.Add(new RefRecord(ns, bucket, new KeyId("testObjectWithMetadata"), blobs: new[] { TestObjectBlobHash },
                    lastAccessTime: lastAccessTime, contentHash: TestObjectHash, metadata: new Dictionary<string, object>() {{"Foo", "Bar"}}));
                await blobStore.PutObject(ns, TestObjectData, TestObjectBlobHash);

                // the deletableObject should be slightly newer then testObject to control sorting for last access
                await refsStore.Add(new RefRecord(ns, bucket, new KeyId("deletableObject"),
                    blobs: new[] { TestObjectBlobHash }, contentHash: TestObjectHash,lastAccessTime: lastAccessTime.AddMinutes(2), metadata: null));
                await blobStore.PutObject(ns, TestObjectData, TestObjectBlobHash);

                byte[] contents = Encoding.ASCII.GetBytes("This content will only be hashed");
                BlobIdentifier newContentIdentifier = BlobIdentifier.FromBlob(contents);
                await refsStore.Add(new RefRecord(ns, bucket, new KeyId("notUploadedToIo"), blobs: new[] { newContentIdentifier },
                    lastAccessTime: lastAccessTime, contentHash: newContentIdentifier, metadata: null));
                // this object should not be uploaded to IO, and is considered recently uploaded for last access filtering 
            }
        }

        [TestCleanup]
        public async Task Teardown()
        {
            if (_server != null)
            {
                await TeardownDb(_server.Services);
            }
        }

        protected static bool RefStoreIs(IRefsStore? refStore, Type refStoreType)
        {
            if (refStore == null)
            {
                return false;
            }

            if (refStore.GetType() == refStoreType)
            {
                return true;
            }

            if (refStore is CachedRefStore store)
            {
                if (store.BackingStore.GetType() == refStoreType)
                {
                    return true;
                }
            }

            return false;
        }

        protected abstract IEnumerable<KeyValuePair<string, string>> GetSettings();

        protected abstract Task SeedDb(IServiceProvider provider);
        protected abstract Task TeardownDb(IServiceProvider provider);

        [TestMethod]
        public async Task ListNamespaces()
        {
            HttpResponseMessage response = await _httpClient!.GetAsync(new Uri("api/v1/c/ddc", UriKind.Relative));
            response.EnsureSuccessStatusCode();
            GetNamespacesResponse content = await response.Content.ReadAsAsync<GetNamespacesResponse>();

            // the namespaces here may not just be the namespaces we have created depending on what has been done in the dbs before
            // as such we can only verify that TestNamespace exists

            Assert.IsTrue(content.Namespaces.Contains(TestNamespace));
        }

        [TestMethod]
        public async Task GetRefObject()
        {
            // fetch refs records once first so that any previous runs of tests are ignored
            List<(RefRecord, DateTime)> _ = await _lastAccessCache!.GetLastAccessedRecords();

            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObject", UriKind.Relative));

            result.EnsureSuccessStatusCode();
            RefResponse content = await result.Content.ReadAsAsync<RefResponse>();
            Assert.AreEqual(expected: $"{TestNamespace}.bucket.testObject", content.Name);
            Assert.IsNull(content.Metadata);
            Assert.IsNotNull(content.Blob);
            CollectionAssert.AreEqual(TestObjectData, content.Blob);

            // give the last access cache some time to get updated as this is not guaranteed to have finished when the Get endpoints returns
            // Disabled these checks for last access cache as there is no guarantees when it is updated compared to our get, so these tests will just intermittently fail
            //List<(RefRecord, DateTime)> accessedRecords = await _lastAccessCache!.GetLastAccessedRecords();
            //Assert.AreEqual(1, accessedRecords.Count);
            //(RefRecord record, DateTime _) = accessedRecords.First();
            //Assert.AreEqual("testObject", record.RefName);
        }

        [TestMethod]
        public async Task GetFullRefObjectAsCompactBinary()
        {
            using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObjectWithMetadata.uecb", UriKind.Relative));
            HttpResponseMessage response = await _httpClient!.SendAsync(request);

            Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, response.Content.Headers.ContentType!.MediaType!);
            response.EnsureSuccessStatusCode();

            byte[] buffer = await response.Content.ReadAsByteArrayAsync();
            CbObject o = new CbObject(buffer);
            List<CbField> fields = o.ToList();
            Assert.AreEqual(5, fields.Count);
            Assert.AreEqual($"{TestNamespace}.bucket.testObjectWithMetadata", o["name"]!.AsString());

            Assert.IsNotNull(o["contentHash"]);

            Assert.IsNotNull(o["lastAccessTime"]);

            Assert.IsNotNull(o["blob"]);
            CollectionAssert.AreEqual(TestObjectData, o["blob"]!.AsBinary().ToArray());
        }

        [TestMethod]
        public async Task GetRefWithMetadata()
        {
            Task<HttpResponseMessage> response = _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObjectWithMetadata", UriKind.Relative));
            HttpResponseMessage result = await response;

            result.EnsureSuccessStatusCode();
            RefResponse content = await result.Content.ReadAsAsync<RefResponse>();
            Assert.AreEqual(expected: $"{TestNamespace}.bucket.testObjectWithMetadata", content.Name);
            Assert.IsNotNull(content.Metadata);
            Assert.IsTrue(content.Metadata!.ContainsKey("Foo"));
            Assert.AreEqual("Bar", content!.Metadata["Foo"]);

            Assert.IsNotNull(content.Blob);
            CollectionAssert.AreEqual(TestObjectData, content.Blob);
        }

        [TestMethod]
        public async Task GetRefRawObject()
        {
            Task<HttpResponseMessage> response = _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObject.raw", UriKind.Relative));
            HttpResponseMessage result = await response;

            result.EnsureSuccessStatusCode();
            byte[] content = await result.Content.ReadAsByteArrayAsync();
            CollectionAssert.AreEqual(TestObjectData, content);
        }

        [TestMethod]
        public async Task GetRefObjectFieldFilteringAwayBlob()
        {
            // fetch refs records once first so that any previous runs of tests are ignored
            List<(RefRecord, DateTime)> _ = await _lastAccessCache!.GetLastAccessedRecords();

            Task<HttpResponseMessage> response = _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObject?fields=name&fields=blobIdentifiers", UriKind.Relative));
            HttpResponseMessage result = await response;

            result.EnsureSuccessStatusCode();

            string content = await result.Content.ReadAsStringAsync();
            JToken? jsonResult = JsonConvert.DeserializeObject(content) as JToken;

            Assert.IsNotNull(jsonResult);

            Assert.AreEqual($"{TestNamespace}.bucket.testObject", jsonResult!["name"]);
            Assert.AreEqual(TestObjectBlobHash, new BlobIdentifier(jsonResult!["blobIdentifiers"]!.Value<JArray>()![0]!.Value<string>()!));
            Assert.IsTrue(jsonResult["blob"] == null, "Expecting blob field to not be present");
            Assert.IsTrue(jsonResult["contentHash"] == null, "Expecting contentHash field to not be present");
        }

        
        [TestMethod]
        public async Task GetRefObjectEmptyFields()
        {
            // fetch refs records once first so that any previous runs of tests are ignored
            List<(RefRecord, DateTime)> _ = await _lastAccessCache!.GetLastAccessedRecords();

            Task<HttpResponseMessage> response = _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/testObject?fields=", UriKind.Relative));
            HttpResponseMessage result = await response;

            result.EnsureSuccessStatusCode();

            RefResponse content = await result.Content.ReadAsAsync<RefResponse>();
            Assert.AreEqual(expected: $"{TestNamespace}.bucket.testObject", content.Name);
            Assert.IsNull(content.Metadata);
            Assert.IsNotNull(content.Blob);
            CollectionAssert.AreEqual(TestObjectData, content.Blob);
        }

        [TestMethod]
        public async Task GetNonExistentIoObject()
        {
            HttpResponseMessage response = await _httpClient!.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/notUploadedToIo", UriKind.Relative));

            Assert.AreEqual(HttpStatusCode.BadRequest, response.StatusCode);

            string content = await response.Content.ReadAsStringAsync();
            JToken? jsonResult = JsonConvert.DeserializeObject(content) as JToken;
            Assert.AreEqual("Object EBA4BE7385F365E6AC2354096A6CA2C5AF412A0F in test-namespace not found", jsonResult!["title"]!.ToString());
        }

        [TestMethod]
        public async Task PutRefObject()
        {
            byte[] payload = Encoding.ASCII.GetBytes("I have some new content");
            ContentHash payloadHash = ContentHash.FromBlob(payload);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, payloadHash.ToString());

            HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/newObject", UriKind.Relative), requestContent);

            result.EnsureSuccessStatusCode();
        }

        [TestMethod]
        public async Task PutGetStructuredRefObject()
        {
            string jsonObject = $"{{\"metadata\": null, \"blobReferences\": [\"{TestObjectHash}\"], \"contentHash\" : \"{TestObjectHash}\"}}";
            using StringContent requestContent = new StringContent(jsonObject);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/json");
            requestContent.Headers.ContentLength = jsonObject.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, TestObjectHash.ToString());

            HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/newStructuredObject", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/newStructuredObject.raw", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            await using MemoryStream ms = new MemoryStream();
            await getResponse.Content.CopyToAsync(ms);

            byte[] roundTrippedBuffer = ms.ToArray();
            string roundTrippedPayload = Encoding.ASCII.GetString(roundTrippedBuffer);

            Assert.AreEqual(TestObjectContents, roundTrippedPayload);
            CollectionAssert.AreEqual(TestObjectData, roundTrippedBuffer);
        }

        [TestMethod]
        public async Task PutGetLargeObject()
        {
            // we chunk at 1 kb to lets generate a 3kb string
            string content = string.Concat(Enumerable.Repeat("Duplicate string", 200));
            byte[] payload = Encoding.ASCII.GetBytes(content);
            ContentHash payloadHash = ContentHash.FromBlob(payload);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, payloadHash.ToString());

            HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/largeObject", UriKind.Relative), requestContent);

            result.EnsureSuccessStatusCode();

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/largeObject.raw", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            await using MemoryStream ms = new MemoryStream();
            await getResponse.Content.CopyToAsync(ms);

            byte[] roundTrippedBuffer = ms.ToArray();
            string roundTrippedPayload = Encoding.ASCII.GetString(roundTrippedBuffer);

            Assert.AreEqual(content.Length, roundTrippedPayload.Length, "Expected the same object, length was different");

            Assert.AreEqual(content, roundTrippedPayload);

            CollectionAssert.AreEqual(payload, roundTrippedBuffer);
        }

        [TestMethod]
        public async Task DeleteRefObject()
        {
            HttpResponseMessage deleteResponse = await _httpClient!.DeleteAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/deletableObject", UriKind.Relative));
            deleteResponse.EnsureSuccessStatusCode();
        }

        [TestMethod]
        public async Task DeleteNonExistentObject()
        {
            HttpResponseMessage deleteResponse = await _httpClient!.DeleteAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/thisDoesNotExistForDelete", UriKind.Relative));
            Assert.AreEqual(System.Net.HttpStatusCode.BadRequest, deleteResponse.StatusCode);
        }

        [TestMethod]
        public async Task FullFlow()
        {
            byte[] payload = Encoding.ASCII.GetBytes("Foo bar");
            ContentHash payloadHash = ContentHash.FromBlob(payload);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, payloadHash.ToString());

            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/fullFlowObject", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            await putResponse.Content.ReadAsStringAsync();

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/fullFlowObject", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            RefResponse content = await getResponse.Content.ReadAsAsync<RefResponse>();
            Assert.AreEqual(expected: $"{TestNamespace}.bucket.fullFlowObject", content.Name);
            Assert.IsNull(content.Metadata);
            Assert.IsNotNull(content.Blob);
            CollectionAssert.AreEqual(payload, content.Blob);

            HttpResponseMessage deleteResponse = await _httpClient.DeleteAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/fullFlowObject", UriKind.Relative));
            deleteResponse.EnsureSuccessStatusCode();
            Assert.AreEqual(System.Net.HttpStatusCode.NoContent, deleteResponse.StatusCode);
        }

        [TestMethod]
        public async Task DeleteNamespace()
        {
            NamespaceId deleteNamespace = new NamespaceId("delete-namespace");

            byte[] payload = Encoding.ASCII.GetBytes("Foo bar");
            ContentHash payloadHash = ContentHash.FromBlob(payload);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, payloadHash.ToString());

            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{deleteNamespace}/bucket/deleteNamespaceObject", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            await putResponse.Content.ReadAsStringAsync();

            HttpResponseMessage deleteResponse = await _httpClient.DeleteAsync(new Uri($"api/v1/c/ddc/{deleteNamespace}", UriKind.Relative));
            deleteResponse.EnsureSuccessStatusCode();
            Assert.AreEqual(System.Net.HttpStatusCode.NoContent, deleteResponse.StatusCode);

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/bucket/deleteNamespaceObject", UriKind.Relative));
            Assert.AreEqual(System.Net.HttpStatusCode.BadRequest, getResponse.StatusCode);
        }

        protected abstract Task<RefRecord> GetTestRecord(BucketId bucket, KeyId name, IRefsStore.ExtraFieldsFlag fields);

        [TestMethod]
        public async Task LastAccessRollup()
        {
            BucketId bucket = new BucketId("bucket");
            KeyId key = new KeyId("lastAccessObject");
            byte[] payload = Encoding.ASCII.GetBytes("This payload does not matter");
            ContentHash payloadHash = ContentHash.FromBlob(payload);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, payloadHash.ToString());

            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/{bucket}/{key}", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            await putResponse.Content.ReadAsStringAsync();

            RefRecord initialLastAccessObject = await GetTestRecord(bucket, key, IRefsStore.ExtraFieldsFlag.LastAccess);
            Assert.AreEqual(key, initialLastAccessObject.RefName);
            Assert.IsTrue(initialLastAccessObject.LastAccessTime.HasValue);
            DateTime creationTime = initialLastAccessObject.LastAccessTime!.Value;

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/c/ddc/{TestNamespace}/{bucket}/{key}", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            RefResponse content = await getResponse.Content.ReadAsAsync<RefResponse>();
            Assert.AreEqual(expected: $"{TestNamespace}.bucket.lastAccessObject", content.Name);

            using StringContent jsonContent = new StringContent("", Encoding.UTF8, "application/json");
            HttpResponseMessage lastAccessRollupResponse = await _httpClient.PostAsync(new Uri($"api/v1/admin/startLastAccessRollup", UriKind.Relative), jsonContent);
            lastAccessRollupResponse.EnsureSuccessStatusCode();

            RefRecord doc = await GetTestRecord(bucket, key, IRefsStore.ExtraFieldsFlag.LastAccess);
            Assert.AreEqual(key, doc.RefName);
            Assert.IsTrue(doc.LastAccessTime > creationTime);

            // make sure the refs is empty
            List<(RefRecord, DateTime)> records = await _lastAccessCache!.GetLastAccessedRecords();
            Assert.AreEqual(0, records.Count);
        }

        [TestMethod]
        public async Task LastAccess()
        {
            IRefsStore refsStore = _server!.Services.GetService<IRefsStore>()!;

            OldRecord[] emptyRecords = await refsStore.GetOldRecords(LastAccessTestNamespace, TimeSpan.FromDays(7)).ToArrayAsync();
            // no content older then 7 days as we just insert it
            Assert.AreEqual(0, emptyRecords.Length);

            // check content inserted in the last hour, which should return the 3 objects set as inserted a day ago, but not the one that just got inserted
            OldRecord[] oldRecords = await refsStore.GetOldRecords(LastAccessTestNamespace, TimeSpan.FromHours(1)).ToArrayAsync();

            List<KeyId> validRefs = new List<KeyId>()
            {
                new KeyId("deletableObject"),
                new KeyId("testObject"),
                new KeyId("testObjectWithMetadata"),
                new KeyId("notUploadedToIo")
            };
            foreach(OldRecord record in oldRecords)
            {
                Assert.IsTrue(validRefs.Contains(record.RefName), $"ValidRefs did not contain ref {record.RefName}");
            }

            // we should have our 4 pre-seeded objects
            Assert.AreEqual(4, oldRecords.Length);
        }

        [TestMethod]
        public async Task Batch()
        {
            if (this is DynamoRefTests)
            {
                Assert.Inconclusive();
            }

            // we chunk at 1 kb to lets generate a 3kb string
            string content = string.Concat(Enumerable.Repeat("Duplicate string", 200));
            byte[] payload = Encoding.ASCII.GetBytes(content);
            ContentHash payloadHash = ContentHash.FromBlob(payload);

            var ops = new
            {
                Operations = new object[]
                {
                    new
                    {
                        Op = "GET",
                        Namespace = TestNamespace,
                        Bucket = "bucket",
                        Id = "testObject",
                    },
                    new
                    {
                        Namespace = TestNamespace,
                        Op = "GET",
                        Bucket = "bucket",
                        Id = "thisDoesNotExist",
                    },
                    new
                    {
                        Namespace = TestNamespace,
                        Op = "PUT",
                        Bucket = "bucket",
                        Id = "largeBatchObject",
                        Content = payload,
                        ContentHash = payloadHash
                    }
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/c/ddc-rpc", ops);
            response.EnsureSuccessStatusCode();

            string stringResult = await response.Content.ReadAsStringAsync();
            object? result = JsonConvert.DeserializeObject(stringResult);
            JArray? array = result as JArray;
            Assert.IsNotNull(array);
            Assert.AreEqual(3, array!.Count);

            Assert.AreEqual($"{TestNamespace}.bucket.testObject", array![0]["name"]);
            Assert.AreEqual(TestObjectHash, new ContentHash(array![0]["contentHash"]!.Value<string>()!));
            Assert.AreEqual(TestObjectBlobHash, new BlobIdentifier(array![0]["blobIdentifiers"]![0]!.Value<string>()!));
            Assert.AreEqual("Test Object Data", Encoding.ASCII.GetString(Convert.FromBase64String(array![0]["blob"]!.Value<string>()!)));

            Assert.AreEqual("Exception thrown", array![1]["title"]);
            Assert.AreEqual(0L, array![2]["transactionId"]!.Value<long>());
        }

        [TestMethod]
        public async Task BatchGet()
        {
            var ops = new
            {
                Namespace = TestNamespace,
                Operations = new object[]
                {
                    new
                    {
                        Bucket = "bucket",
                        Key = "testObject",
                    },
                    new
                    {
                        Bucket = "bucket",
                        Key = "thisDoesNotExist",
                    },
                    new
                    {
                        Bucket = "bucket",
                        Key = "headCheckObjectNotExist",
                        Verb = "HEAD"
                    },
                    new
                    {
                        Bucket = "bucket",
                        Key = "testObjectWithMetadata",
                        Verb = "HEAD"
                    },
                    new
                    {
                        Bucket = "bucket",
                        Key = "notUploadedToIo",
                    }
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/c/ddc-rpc/batchGet", ops);
            response.EnsureSuccessStatusCode();

            Stream stream = await response.Content.ReadAsStreamAsync();
            await using MemoryStream ms = new MemoryStream();
            // read the entire stream into a buffer before we start processing it
            await stream.CopyToAsync(ms);
            byte[] buffer = ms.ToArray();
            ReadOnlyMemory<byte> span = new ReadOnlyMemory<byte>(buffer);
            // first 4 bytes is a magic header
            Assert.AreEqual("JPTR", Encoding.ASCII.GetString(span.Slice(0, 4).Span));
            span = span.Slice(4);
            const uint ExpectedCountOfObjects = 5u;
            // next we expect a 32 bit unsigned integer of the number of results
            Assert.AreEqual(ExpectedCountOfObjects, BitConverter.ToUInt32(span.Slice(0, 4).Span));
            span = span.Slice(4);

            for (int i = 0; i < ExpectedCountOfObjects; i++)
            {
                // first 4 bytes is another magic header
                Assert.AreEqual("JPEE", Encoding.ASCII.GetString(span.Slice(0, 4).Span));
                span = span.Slice(4);

                // count length of string
                int count = 0;
                ReadOnlyMemory<byte> stringMemory = span;
                while (stringMemory.Slice(0, 1).ToArray()[0] != '\0')
                {
                    count++;
                    stringMemory = stringMemory.Slice(1);
                }

                // next we should find the full key to the object
                string key = Encoding.ASCII.GetString(span.Slice(0, count).Span);
                span = span.Slice(count+1); // +1 because we want to also remove the null terminator

                byte state = span.Slice(0, 1).ToArray()[0];
                span = span.Slice(1);

                if (key == $"{TestNamespace}.bucket.testObject")
                {
                    Assert.AreEqual(0, state);

                    // next is a hash (20 bytes)
                    ContentHash hash = new ContentHash(span.Slice(0, 20).ToArray());
                    Assert.AreEqual(TestObjectHash, hash);
                    span = span.Slice(20);

                    // next we expect a 64 bit uint of the number of results
                    ulong lengthOfBlob = BitConverter.ToUInt64(span.Slice(0, 8).Span);
                    Assert.AreEqual((uint)TestObjectData.Length, lengthOfBlob);
                    span = span.Slice(8);

                    // lastly we read the object
                    byte[] blob = span.Slice(0, (int)lengthOfBlob).ToArray();
                    CollectionAssert.AreEqual(TestObjectData, blob);
                    span = span.Slice((int)lengthOfBlob);

                } 
                else if (key == $"{TestNamespace}.bucket.thisDoesNotExist")
                {
                    // this object was not found, thus its state will be NotFound but the test is empty
                    Assert.AreEqual(2, state);
                }
                else if (key == $"{TestNamespace}.bucket.notUploadedToIo")
                {
                    // this object does exist but a payload is missing from the blob store
                    Assert.AreEqual(2, state);
                }
                else if (key == $"{TestNamespace}.bucket.headCheckObjectNotExist")
                {
                    // this object was not found, thus its state will be NotFound but the test is empty
                    Assert.AreEqual(2, state);
                }
                else if (key == $"{TestNamespace}.bucket.testObjectWithMetadata")
                {
                    // this object should exist but as its a head check will only return the state
                    Assert.AreEqual(3, state);
                }
                else
                {
                    throw new NotImplementedException();
                }
            }

            // we have reached the end of the result thus the stream should be empty
            Assert.IsTrue(span.IsEmpty);
        }
    }
}
