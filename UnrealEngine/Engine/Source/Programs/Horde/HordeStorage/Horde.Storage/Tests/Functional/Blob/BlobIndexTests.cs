// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using async_enumerable_dotnet;
using Cassandra;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Driver;
using Serilog;
using Logger = Serilog.Core.Logger;

namespace Horde.Storage.FunctionalTests.Storage
{
    public abstract class BlobIndexTests
    {
        private TestServer? _server;
        private HttpClient? _httpClient;
        private readonly NamespaceId _testNamespaceName = new NamespaceId("testbucket");

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

            // Seed storage
            await Seed(_server.Services);
        }

        protected abstract IEnumerable<KeyValuePair<string, string>> GetSettings();

        protected abstract Task Seed(IServiceProvider serverServices);
        protected abstract Task Teardown(IServiceProvider serverServices);

        [TestCleanup]
        public async Task MyTeardown()
        {
            await Teardown(_server!.Services);
        }

        
        [TestMethod]
        public async Task PutBlobToIndex()
        {
            byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents");
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            BlobIdentifier contentHash = BlobIdentifier.FromBlob(payload);
            HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
            response.EnsureSuccessStatusCode();

            IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
            Assert.IsNotNull(index);
            BlobInfo? blobInfo = await index.GetBlobInfo(_testNamespaceName, contentHash);

            Assert.IsNotNull(blobInfo);
            Assert.IsTrue(blobInfo.Regions.Contains("test"));
        }

        [TestMethod]
        public async Task UploadRef()
        {
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteString("stringField","thisIsAField");
            writer.EndObject();

            byte[] objectData = writer.ToByteArray();
            BlobIdentifier objectHash = BlobIdentifier.FromBlob(objectData);

            using HttpContent requestContent = new ByteArrayContent(objectData);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
            requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());
            IoHashKey putKey = IoHashKey.FromName("newReferenceUploadObject");
            HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/refs/{_testNamespaceName}/bucket/{putKey}.uecb", UriKind.Relative), requestContent);
            result.EnsureSuccessStatusCode();

            IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
            Assert.IsNotNull(index);
            BlobInfo? blobInfo = await index.GetBlobInfo(_testNamespaceName, objectHash);

            Assert.IsNotNull(blobInfo);
            Assert.IsTrue(blobInfo.Regions.Contains("test"));
            Assert.AreEqual(1, blobInfo.References.Count);

            (BucketId bucket, IoHashKey key) = blobInfo.References[0];
            Assert.AreEqual("bucket", bucket.ToString());
            Assert.AreEqual(putKey, key);
        }

        [TestMethod]
        public async Task DeleteBlob()
        {
            // upload a blob
            byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents");
            BlobIdentifier contentHash = BlobIdentifier.FromBlob(payload);

            {
                using ByteArrayContent requestContent = new ByteArrayContent(payload);
                requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
                response.EnsureSuccessStatusCode();
            }
            // verify its present in the blob index
            IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
            Assert.IsNotNull(index);
            BlobInfo? blobInfo = await index.GetBlobInfo(_testNamespaceName, contentHash);
            Assert.IsNotNull(blobInfo);
            Assert.IsTrue(blobInfo.Regions.Any());

            // delete the blob
            {
                HttpResponseMessage response = await _httpClient!.DeleteAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative));
                response.EnsureSuccessStatusCode();
            }

            BlobInfo? deletedBlobInfo = await index.GetBlobInfo(_testNamespaceName, contentHash);

            bool hasRegions = deletedBlobInfo != null && deletedBlobInfo.Regions.Any();
            // but the blob info will not contain the current region
            Assert.IsTrue(!hasRegions);
        }

        [TestMethod]
        public async Task EnumerateAllBlobs()
        {
            IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
            Assert.IsNotNull(index);
            {
                // verify the blob info list is empty at the start
                BlobInfo[]? blobInfos =  await index.GetAllBlobs().ToArrayAsync();
                Assert.AreEqual(0, blobInfos.Length);
            }

            // upload a blob
            BlobIdentifier contentHash;
            {
                byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents");
                contentHash = BlobIdentifier.FromBlob(payload);

                {
                    using ByteArrayContent requestContent = new ByteArrayContent(payload);
                    requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                    HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/blobs/{_testNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
                    response.EnsureSuccessStatusCode();
                }
            }

            // upload a compressed blob
            BlobIdentifier compressedPayloadIdentifier;
            {
                byte[] texturePayload = await File.ReadAllBytesAsync($"ContentId/Payloads/UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd");
                compressedPayloadIdentifier = BlobIdentifier.FromBlob(texturePayload);
                BlobIdentifier uncompressedPayloadIdentifier = new BlobIdentifier("DEA81B6C3B565BB5089695377C98CE0F1C13B0C3");

                using ByteArrayContent content = new ByteArrayContent(texturePayload);
                content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
                HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/compressed-blobs/{_testNamespaceName}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
                result.EnsureSuccessStatusCode();
            }

            {
                BlobInfo[]? blobInfos =  await index.GetAllBlobs().ToArrayAsync();
                Assert.AreEqual(2, blobInfos.Length);

                Assert.IsNotNull(blobInfos.FirstOrDefault(info => info.BlobIdentifier.Equals(compressedPayloadIdentifier)));
                Assert.IsNotNull(blobInfos.FirstOrDefault(info => info.BlobIdentifier.Equals(contentHash)));
            }
        }
    }

    [TestClass()]
    public class MemoryBlobIndexTests : BlobIndexTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] { new KeyValuePair<string, string>("Horde_Storage:BlobIndexImplementation", HordeStorageSettings.BlobIndexImplementations.Memory.ToString()) };
        }

        protected override Task Seed(IServiceProvider serverServices)
        {
            return Task.CompletedTask;
        }

        protected override Task Teardown(IServiceProvider serverServices)
        {
            return Task.CompletedTask;
        }
    }

    [TestClass]
    public class ScyllaBlobIndexTests : BlobIndexTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] { new KeyValuePair<string, string>("Horde_Storage:BlobIndexImplementation", HordeStorageSettings.BlobIndexImplementations.Scylla.ToString()) };
        }

        protected override Task Seed(IServiceProvider serverServices)
        {
            return Task.CompletedTask;
        }

        protected override async Task Teardown(IServiceProvider provider)
        {
            IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;

            ISession replicatedKeyspace = scyllaSessionManager.GetSessionForReplicatedKeyspace();
            await replicatedKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS blob_index"));
        }
    }

    [TestClass]
    public class MongoBlobIndexTests : BlobIndexTests
    {
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] { new KeyValuePair<string, string>("Horde_Storage:BlobIndexImplementation", HordeStorageSettings.BlobIndexImplementations.Mongo.ToString()) };
        }

        protected override async Task Seed(IServiceProvider provider)
        {
            IOptionsMonitor<MongoSettings> mongoSettings = provider.GetService<IOptionsMonitor<MongoSettings>>()!;

            IBlobIndex blobIndex = provider.GetService<IBlobIndex>()!;
            Assert.IsTrue(blobIndex is MongoBlobIndex);

            MongoBlobIndex mongoBlobIndex = (MongoBlobIndex)blobIndex;
            
            MongoClient client = GetMongoClient(mongoSettings);
            string dbName = mongoBlobIndex.GetDatabaseName();
            if (client.GetDatabase(dbName) != null)
            {
                await client.DropDatabaseAsync(dbName);
            }
        }
        private static MongoClient GetMongoClient(IOptionsMonitor<MongoSettings> mongoSettings)
        {
            return new MongoClient(mongoSettings.CurrentValue.ConnectionString);
        }

        protected override async Task Teardown(IServiceProvider provider)
        {
            await Task.CompletedTask;
        }
    }
}
