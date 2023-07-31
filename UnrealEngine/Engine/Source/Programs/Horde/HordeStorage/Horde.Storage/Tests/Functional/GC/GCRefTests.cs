// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using Dasync.Collections;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Serilog;
using Serilog.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter;

namespace Horde.Storage.FunctionalTests.GC
{
    [TestClass]

    public class GCRefTests : IDisposable
    {
        private HttpClient? _httpClient;
        private Mock<IRefsStore>? _refMock;

        private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace");
        private readonly BucketId DefaultBucket = new BucketId("default");
        private TestServer? _server;

        [TestInitialize]
        public void Setup()
        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                            // we are not reading the base appSettings here as we want exact control over what runs in the tests
                            .AddJsonFile("appsettings.Testing.json", false)
                            .AddEnvironmentVariables()
                            .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            OldRecord[] oldRecords =
            {
                new OldRecord(TestNamespace, DefaultBucket, new KeyId("object2")),
                new OldRecord(TestNamespace, DefaultBucket, new KeyId("object5")),
                new OldRecord(TestNamespace, DefaultBucket, new KeyId("object6")),
            };
            _refMock = new Mock<IRefsStore>();
            _refMock.Setup(store => store.GetOldRecords(TestNamespace, It.IsAny<TimeSpan>())).Returns(oldRecords.ToAsyncEnumerable()).Verifiable();
            _refMock.Setup(store => store.Delete(TestNamespace, DefaultBucket, It.IsAny<KeyId>())).Verifiable();

           _server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<HordeStorageStartup>()
                .ConfigureTestServices(collection =>
                {
                    // use our refs mock instead of the actual refs store so we can control which records are invalid
                    collection.AddSingleton<IRefsStore>(_refMock.Object);

                    collection.Configure<NamespaceSettings>(settings =>
                    {
                        settings.Policies = new Dictionary<string, NamespacePolicy>()
                        {
                            {
                                TestNamespace.ToString(), new NamespacePolicy()
                                {
                                    IsLegacyNamespace = true
                                }
                            }
                        };
                    });
                })
            );
            _httpClient = _server.CreateClient();
        }

        [TestMethod]
        public async Task RunRefCleanup()
        {
            // trigger the cleanup
            using StringContent content = new StringContent(string.Empty);
            HttpResponseMessage cleanupResponse = await _httpClient!.PostAsync(new Uri($"api/v1/admin/refCleanup/{TestNamespace}", UriKind.Relative), content);
            cleanupResponse.EnsureSuccessStatusCode();
            RemovedRefRecordsResponse removedRefRecords = await cleanupResponse.Content.ReadAsAsync<RemovedRefRecordsResponse>();

            Assert.AreEqual(3, removedRefRecords.CountOfRemovedRecords);

            _refMock!.Verify();
            _refMock.VerifyNoOtherCalls();
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _httpClient?.Dispose();
                _server?.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            System.GC.SuppressFinalize(this);
        }
    }

    [TestClass]
    public class MemoryGCReferencesTests : GCReferencesTests
    {
        protected override string GetImplementation()
        {
            return "Memory";
        }
    }

    [TestClass]
    public class ScyllaGCReferencesTests : GCReferencesTests
    {
        protected override string GetImplementation()
        {
            return "Scylla";
        }
    }

    public abstract class GCReferencesTests : IDisposable
    {
        private HttpClient? _httpClient;

        private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace");
        private readonly BucketId DefaultBucket = new BucketId("default");

        private readonly BlobIdentifier object0id = new BlobIdentifier("0000000000000000000000000000000000000000");
        private readonly BlobIdentifier object1id = new BlobIdentifier("1111111111111111111111111111111111111111");
        private readonly BlobIdentifier object2id = new BlobIdentifier("2222222222222222222222222222222222222222");
        private readonly BlobIdentifier object3id = new BlobIdentifier("3333333333333333333333333333333333333333");
        private readonly BlobIdentifier object4id = new BlobIdentifier("4444444444444444444444444444444444444444");
        private readonly BlobIdentifier object5id = new BlobIdentifier("5555555555555555555555555555555555555555");
        private readonly BlobIdentifier object6id = new BlobIdentifier("6666666666666666666666666666666666666666");

        private readonly IoHashKey object0Name = IoHashKey.FromName("object0");
        private readonly IoHashKey object1Name = IoHashKey.FromName("object1");
        private readonly IoHashKey object2Name = IoHashKey.FromName("object2");
        private readonly IoHashKey object3Name = IoHashKey.FromName("object3");
        private readonly IoHashKey object4Name = IoHashKey.FromName("object4");
        private readonly IoHashKey object5Name = IoHashKey.FromName("object5");
        private readonly IoHashKey object6Name = IoHashKey.FromName("object6");
        private TestServer? _server;

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
                    new KeyValuePair<string, string>("Horde_Storage:BlobIndexImplementation", GetImplementation()),
                })
                .Build();
            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            _server = new TestServer(new WebHostBuilder()
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
            _httpClient = _server.CreateClient();

            IObjectService? objectService = _server.Services.GetService<IObjectService>()!;
            Assert.IsNotNull(objectService);
            (BlobIdentifier ob0_hash, CbObject ob0_cb) = GetCBWithAttachment(object0id);
            await objectService.Put(TestNamespace, DefaultBucket, object0Name, ob0_hash, ob0_cb);
           
            (BlobIdentifier ob1_hash, CbObject ob1_cb) = GetCBWithAttachment(object1id);
            await objectService.Put(TestNamespace, DefaultBucket, object1Name, ob1_hash, ob1_cb);

            (BlobIdentifier ob2_hash, CbObject ob2_cb) = GetCBWithAttachment(object2id);
            await objectService.Put(TestNamespace, DefaultBucket, object2Name, ob2_hash, ob2_cb);

            (BlobIdentifier ob3_hash, CbObject ob3_cb) = GetCBWithAttachment(object3id);
            await objectService.Put(TestNamespace, DefaultBucket, object3Name, ob3_hash, ob3_cb);

            (BlobIdentifier ob4_hash, CbObject ob4_cb) = GetCBWithAttachment(object4id);
            await objectService.Put(TestNamespace, DefaultBucket, object4Name, ob4_hash, ob4_cb);

            (BlobIdentifier ob5_hash, CbObject ob5_cb) = GetCBWithAttachment(object5id);
            await objectService.Put(TestNamespace, DefaultBucket, object5Name, ob5_hash, ob5_cb);

            (BlobIdentifier ob6_hash, CbObject ob6_cb) = GetCBWithAttachment(object6id);
            await objectService.Put(TestNamespace, DefaultBucket, object6Name, ob6_hash, ob6_cb);

            IReferencesStore referenceStore = _server.Services.GetService<IReferencesStore>()!;
            DateTime oldTimestamp = DateTime.Now.AddDays(-30);
            DateTime newTimestamp = DateTime.Now;
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object0Name, oldTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object1Name, newTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object2Name, oldTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object3Name, oldTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object4Name, newTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object5Name, newTimestamp);
            await referenceStore.UpdateLastAccessTime(TestNamespace, DefaultBucket, object6Name, oldTimestamp);
        }

        protected abstract string GetImplementation();

        [TestMethod]
        public async Task RunRefCleanup()
        {
            // trigger the cleanup
            using StringContent content = new StringContent(string.Empty);
            HttpResponseMessage cleanupResponse = await _httpClient!.PostAsync(new Uri($"api/v1/admin/refCleanup/{TestNamespace}", UriKind.Relative), content);
            cleanupResponse.EnsureSuccessStatusCode();
            RemovedRefRecordsResponse removedRefRecords = await cleanupResponse.Content.ReadAsAsync<RemovedRefRecordsResponse>();
            Assert.AreEqual(4, removedRefRecords.CountOfRemovedRecords);
        }

        private static (BlobIdentifier, CbObject) GetCBWithAttachment(BlobIdentifier blobIdentifier)
        {
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteBinaryAttachment("Attachment", blobIdentifier.AsIoHash());
            writer.EndObject();

            byte[] b = writer.ToByteArray();
            return (BlobIdentifier.FromBlob(b), new CbObject(b));
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _httpClient?.Dispose();
                _server?.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            System.GC.SuppressFinalize(this);
        }
    }
}
