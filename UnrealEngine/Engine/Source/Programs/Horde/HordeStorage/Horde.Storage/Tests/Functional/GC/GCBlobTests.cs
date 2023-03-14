// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using Jupiter.Implementation;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.Blob;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using RestSharp;
using Serilog;
using Serilog.Core;
using Moq;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter;
using Jupiter.Common;

namespace Horde.Storage.FunctionalTests.GC
{
    [TestClass]
    public class GCBlobTests
    {
        private readonly BlobIdentifier object0id = new BlobIdentifier("0000000000000000000000000000000000000000");
        private readonly BlobIdentifier object1id = new BlobIdentifier("1111111111111111111111111111111111111111");
        private readonly BlobIdentifier object2id = new BlobIdentifier("2222222222222222222222222222222222222222");
        private readonly BlobIdentifier object3id = new BlobIdentifier("3333333333333333333333333333333333333333");
        private readonly BlobIdentifier object4id = new BlobIdentifier("4444444444444444444444444444444444444444");
        private readonly BlobIdentifier object5id = new BlobIdentifier("5555555555555555555555555555555555555555");
        private readonly BlobIdentifier object6id = new BlobIdentifier("6666666666666666666666666666666666666666");
        private Mock<IRestClient> _callistoBlobMock = null!;
        private IBlobService? _blobService;

        private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace");

        [TestInitialize]
        public async Task Setup()
        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", false)
                .AddEnvironmentVariables()
                .AddInMemoryCollection(new List<KeyValuePair<string, string>>() { new KeyValuePair<string, string>("Horde_Storage:StorageImplementations:0", "MemoryBlobStore")})
                .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            _callistoBlobMock = GetCallistoMock(new CallistoGetResponse(new TransactionEvent[]
            {
                new AddTransactionEvent("object0", "default", new[] {object0id}, identifier: 0, nextIdentifier: 1),
                new AddTransactionEvent("object2", "default", new[] {object2id}, identifier: 1, nextIdentifier: 2),
                new RemoveTransactionEvent("object2", "default", identifier: 2, nextIdentifier: 3),
                new AddTransactionEvent("object3", "default", new[] {object3id}, identifier: 3, nextIdentifier: 4),
                new AddTransactionEvent("object6", "default", new[] {object6id}, identifier: 4, nextIdentifier: 5)
            }.ToList(), Guid.Empty, 10));

           using  TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<HordeStorageStartup>()
                .ConfigureTestServices(collection =>
                {
                    collection.AddSingleton<IBlobCleanup>(provider =>
                    {
                        GCSettings gcSettings = new GCSettings()
                        {
                            CleanOldBlobs = true,
                            CleanOldBlobsLegacy = true,
                        };
                        IOptionsMonitor<GCSettings> gcSettingsMon = Mock.Of<IOptionsMonitor<GCSettings>>(_ => _.CurrentValue == gcSettings);

                        NamespacePolicyResolver policyResolver = Mock.Of<NamespacePolicyResolver>();

                        IBlobService blobService = provider.GetService<IBlobService>()!;
                        return new OrphanBlobCleanup(blobService!, new LeaderElectionStub(true), _callistoBlobMock.Object, policyResolver);
                    });
                })
            );
            _blobService = server.Services.GetService<IBlobService>()!;

            MemoryBlobStore memoryBlobStore = (MemoryBlobStore) ((BlobService)_blobService).BlobStore.First();
            byte[] emptyContents = Array.Empty<byte>();
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object0id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object1id);// this is not in callisto
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object2id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object3id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object4id); // this is not in callisto
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object5id); // this is not in callisto
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object6id);

            // set all objects to be old, only the orphaned blobs should be deleted
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object0id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object1id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object2id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object3id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object4id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object5id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object6id, DateTime.Now.AddDays(-2));
        }

        private Mock<IRestClient> GetCallistoMock(CallistoGetResponse responseData)
        {
            // the callisto reader will do 2 requests to fetch the events (one to determine the generation and the other to actually fetch the objects)
            // after that we insert a empty response to indicate the end of the log was reached
            Mock<IRestClient> mock = new Mock<IRestClient> { DefaultValue = DefaultValue.Empty };

            
            Mock<IRestResponse<OrphanBlobCleanup.ListNamespaceResponse>> listNsResponse = new Mock<IRestResponse<OrphanBlobCleanup.ListNamespaceResponse>>();
            listNsResponse.Setup(_ => _.StatusCode).Returns(HttpStatusCode.OK);
            listNsResponse.Setup(_ => _.IsSuccessful).Returns(true);
            listNsResponse.Setup(_ => _.Data).Returns(new OrphanBlobCleanup.ListNamespaceResponse() { Logs = new [] {TestNamespace}});
            listNsResponse.Setup(_ => _.ResponseUri).Returns(new Uri("http://localhost"));
            
            mock.Setup(x =>
                x.ExecuteGetAsync<OrphanBlobCleanup.ListNamespaceResponse>(It.IsAny<IRestRequest>(),
                    It.IsAny<CancellationToken>())).ReturnsAsync(listNsResponse.Object).Verifiable();
            
            
            Mock<IRestResponse<CallistoGetResponse>> response = new Mock<IRestResponse<CallistoGetResponse>>();
            response.Setup(_ => _.StatusCode).Returns(HttpStatusCode.OK);
            response.Setup(_ => _.IsSuccessful).Returns(true);
            response.Setup(_ => _.Data).Returns(responseData);
            response.Setup(_ => _.ResponseUri).Returns(new Uri("http://localhost"));
            
            mock.Setup(x =>
                x.ExecuteGetAsync<CallistoGetResponse>(It.IsAny<IRestRequest>(),
                    It.IsAny<CancellationToken>())).ReturnsAsync(response.Object).Verifiable();

            // when we do the second call to callisto with a offset we return a empty object indicating we have reached the end
            Mock<IRestResponse<CallistoGetResponse>> responseSecondPage = new Mock<IRestResponse<CallistoGetResponse>>();
            responseSecondPage.Setup(_ => _.StatusCode).Returns(HttpStatusCode.OK);
            responseSecondPage.Setup(_ => _.IsSuccessful).Returns(true);
            responseSecondPage.Setup(_ => _.Data).Returns(new CallistoGetResponse(
                new List<TransactionEvent>(),
                responseData.Generation,
                responseData.CurrentOffset + 10 // set the offset to some arbitrary higher number to indicate we are at the end of the log
            ));
            responseSecondPage.Setup(_ => _.ResponseUri).Returns(new Uri("http://localhost"));
            mock.Setup(x => x.ExecuteGetAsync<CallistoGetResponse>(
                    It.Is<IRestRequest>(request =>
                        request.Parameters.Any(parameter =>
                            parameter.Name == "offset" && parameter.Value!.ToString() != "0")),
                    It.IsAny<CancellationToken>()))
                .ReturnsAsync(responseSecondPage.Object).Verifiable();

            return mock;
        }

        [TestMethod]
        public async Task RunBlobCleanup()
        {
            GCSettings gcSettings = new GCSettings()
            {
                CleanOldBlobs = true,
                CleanOldBlobsLegacy = true,
            };
            IOptionsMonitor<GCSettings> gcSettingsMon = Mock.Of<IOptionsMonitor<GCSettings>>(_ => _.CurrentValue == gcSettings);

            Mock<INamespacePolicyResolver> policyResolverMock = new Mock<INamespacePolicyResolver>();
            policyResolverMock.Setup(resolver => resolver.GetPoliciesForNs(TestNamespace)).Returns(new NamespacePolicy()
            {
                IsLegacyNamespace = true,
            });

            OrphanBlobCleanup cleanup = new OrphanBlobCleanup(_blobService!, new LeaderElectionStub(true), _callistoBlobMock.Object, policyResolverMock.Object);
            List<NamespaceId> namespaces = await cleanup.ListNamespaces().ToListAsync();
            Assert.AreEqual(1, namespaces.Count);

            using CancellationTokenSource cts = new CancellationTokenSource();
            ulong removedBlobsCount = await cleanup.Cleanup(cts.Token);
            Assert.AreEqual(4ul, removedBlobsCount);
            
            _callistoBlobMock.Verify();
            _callistoBlobMock.VerifyNoOtherCalls();
        }
    }

    [TestClass]
    public class MemoryGCBlobTestsRefs : GCBlobTestsRefs
    {
        protected override string GetImplementation()
        {
            return "Memory";
        }
    }

    [TestClass]
    public class ScyllaGCBlobTestsRefs : GCBlobTestsRefs
    {
        protected override string GetImplementation()
        {
            return "Scylla";
        }
    }

    public abstract class GCBlobTestsRefs
    {
        private TestServer? _server;

        private readonly BlobIdentifier object0id = new BlobIdentifier("0000000000000000000000000000000000000000");
        private readonly BlobIdentifier object1id = new BlobIdentifier("1111111111111111111111111111111111111111");
        private readonly BlobIdentifier object2id = new BlobIdentifier("2222222222222222222222222222222222222222");
        private readonly BlobIdentifier object3id = new BlobIdentifier("3333333333333333333333333333333333333333");
        private readonly BlobIdentifier object4id = new BlobIdentifier("4444444444444444444444444444444444444444");
        private readonly BlobIdentifier object5id = new BlobIdentifier("5555555555555555555555555555555555555555");
        private readonly BlobIdentifier object6id = new BlobIdentifier("6666666666666666666666666666666666666666");
        private IBlobService? _blobService;

        private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace");

        [TestInitialize]
        public async Task Setup()
        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", false)
                .AddEnvironmentVariables()
                .AddInMemoryCollection(new List<KeyValuePair<string, string>>()
                {
                    new KeyValuePair<string, string>("Horde_Storage:StorageImplementations:0", "MemoryBlobStore"),
                    new KeyValuePair<string, string>("GC:CleanOldBlobs", true.ToString()),
                    new KeyValuePair<string, string>("GC:CleanOldBlobsLegacy", false.ToString()),
                    new KeyValuePair<string, string>("Horde_Storage:BlobIndexImplementation", GetImplementation()),
                    
                })
                .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .ConfigureTestServices(collection =>
                {
                    collection.Configure<NamespaceSettings>(settings =>
                    {
                        settings.Policies = new Dictionary<string, NamespacePolicy>()
                        {
                            {
                                TestNamespace.ToString(), new NamespacePolicy()
                                {
                                    IsLegacyNamespace = false
                                }
                            }
                        };
                    });
                })
                .UseStartup<HordeStorageStartup>()
            );

            _server = server;

            _blobService = server.Services.GetService<IBlobService>()!;

            MemoryBlobStore memoryBlobStore = (MemoryBlobStore) ((BlobService)_blobService).BlobStore.First();
            byte[] emptyContents = Array.Empty<byte>();
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object0id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object1id);// this is not in the index
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object2id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object3id);
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object4id); // this is not in the index
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object5id); // this is not in the index
            await memoryBlobStore.PutObject(TestNamespace, emptyContents, object6id);

            // set all objects to be old, only the orphaned blobs should be deleted
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object0id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object1id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object2id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object3id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object4id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object5id, DateTime.Now.AddDays(-2));
            memoryBlobStore.SetLastModifiedTime(TestNamespace, object6id, DateTime.Now.AddDays(-2));

            BucketId testBucket = new BucketId("test");
            IObjectService? objectService = server.Services.GetService<IObjectService>()!;
            Assert.IsNotNull(objectService);
            (BlobIdentifier ob0_hash, CbObject ob0_cb) = GetCBWithAttachment(object0id);
            await objectService.Put(TestNamespace, testBucket, IoHashKey.FromName("object0"), ob0_hash, ob0_cb);
           
            (BlobIdentifier ob2_hash, CbObject ob2_cb) = GetCBWithAttachment(object2id);
            await objectService.Put(TestNamespace, testBucket, IoHashKey.FromName("object2"), ob2_hash, ob2_cb);

            (BlobIdentifier ob3_hash, CbObject ob3_cb) = GetCBWithAttachment(object3id);
            await objectService.Put(TestNamespace, testBucket, IoHashKey.FromName("object3"), ob3_hash, ob3_cb);

            (BlobIdentifier ob6_hash, CbObject ob6_cb) = GetCBWithAttachment(object6id);
            await objectService.Put(TestNamespace, testBucket, IoHashKey.FromName("object6"), ob6_hash, ob6_cb);

            IReferencesStore referenceStore = server.Services.GetService<IReferencesStore>()!;
            await referenceStore.UpdateLastAccessTime(TestNamespace, testBucket, IoHashKey.FromName("object0"), DateTime.Now.AddDays(-2));
            await referenceStore.UpdateLastAccessTime(TestNamespace, testBucket, IoHashKey.FromName("object2"), DateTime.Now.AddDays(-2));
            await referenceStore.UpdateLastAccessTime(TestNamespace, testBucket, IoHashKey.FromName("object3"), DateTime.Now.AddDays(-2));
            await referenceStore.UpdateLastAccessTime(TestNamespace, testBucket, IoHashKey.FromName("object6"), DateTime.Now.AddDays(-2));

            IBlobIndex? blobIndex = server.Services.GetService<IBlobIndex>()!;
            Assert.IsNotNull(blobIndex);
            await blobIndex.AddBlobToIndex(TestNamespace, object0id);
            await blobIndex.AddRefToBlobs(TestNamespace, testBucket, IoHashKey.FromName("object0"), new [] {object0id });
            await blobIndex.AddBlobToIndex(TestNamespace, object2id);
            await blobIndex.AddRefToBlobs(TestNamespace, testBucket, IoHashKey.FromName("object2"), new [] {object2id });
            await blobIndex.AddBlobToIndex(TestNamespace, object3id);
            await blobIndex.AddRefToBlobs(TestNamespace, testBucket, IoHashKey.FromName("object3"), new [] {object3id });
            await blobIndex.AddBlobToIndex(TestNamespace, object6id);
            await blobIndex.AddRefToBlobs(TestNamespace, testBucket, IoHashKey.FromName("object6"), new [] {object6id });
        }

        protected abstract string GetImplementation();

        private static (BlobIdentifier, CbObject) GetCBWithAttachment(BlobIdentifier blobIdentifier)
        {
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteBinaryAttachment("Attachment", blobIdentifier.AsIoHash());
            writer.EndObject();

            byte[] b = writer.ToByteArray();
            return (BlobIdentifier.FromBlob(b), new CbObject(b));
        }

        [TestMethod]
        public async Task RunBlobCleanupRefs()
        {
            OrphanBlobCleanupRefs? cleanup = _server!.Services.GetService<OrphanBlobCleanupRefs>();
            Assert.IsNotNull(cleanup);

            using CancellationTokenSource cts = new();
            ulong countOfRemovedBlobs = await cleanup.Cleanup(cts.Token);
            Assert.AreEqual(3u, countOfRemovedBlobs);

            foreach (BlobIdentifier blob in new BlobIdentifier[] {object1id, object4id, object5id})
            {
                Assert.IsFalse(await _blobService!.Exists(TestNamespace, blob));
            }

            foreach (BlobIdentifier blob in new BlobIdentifier[] {object2id, object3id, object6id})
            {
                Assert.IsTrue(await _blobService!.Exists(TestNamespace, blob));
            }
        }
    }
}
