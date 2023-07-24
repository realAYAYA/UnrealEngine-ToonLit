// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Jupiter.FunctionalTests.GC
{
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
                    new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:0", "Memory"),
                    new KeyValuePair<string, string>("GC:CleanOldBlobs", true.ToString()),
                    new KeyValuePair<string, string>("UnrealCloudDDC:BlobIndexImplementation", GetImplementation()),
                    
                })
                .Build();

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<JupiterStartup>()
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
