// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using Horde.Storage.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;

namespace Horde.Storage.UnitTests
{
    [TestClass]
    public class HordeStorageStartupTest
    {
        private readonly string localTestDir;
        
        public HordeStorageStartupTest()
        {
            localTestDir = Path.Combine(Path.GetTempPath(), "HordeStorageStartupTest", Path.GetRandomFileName());
        }

        [TestMethod]
        public void StorageBackendImplConfiguration()
        {
            // No configuration set
            BlobService blobService = GetBlobServiceForConfig(new Dictionary<string, string>());
            Assert.IsTrue(blobService.BlobStore.Single() is MemoryCacheBlobStore);
            
            // A single blob store configuration should yield the store itself without a hierarchical wrapper store
            blobService = GetBlobServiceForConfig(new Dictionary<string, string> {{"Horde_Storage:StorageImplementations:0", "FileSystem"}});
            Assert.IsTrue(blobService.BlobStore.Single() is FileSystemStore);
            
            // Should not be case-sensitive
            blobService = GetBlobServiceForConfig(new Dictionary<string, string> {{"Horde_Storage:StorageImplementations:0", "FiLeSYsTEm"}});
            Assert.IsTrue(blobService.BlobStore.Single() is FileSystemStore);
            
            // Two or more blob stores returns a hierarchical store
            blobService = GetBlobServiceForConfig(new Dictionary<string, string>
            {
                {"Horde_Storage:StorageImplementations:0", "FileSystem"},
                {"Horde_Storage:StorageImplementations:1", "Memory"},
            });

            List<IBlobStore> blobStores = blobService.BlobStore.ToList();
            Assert.IsTrue(blobStores[0] is FileSystemStore);
            Assert.IsTrue(blobStores[1] is MemoryCacheBlobStore);
        }

        private BlobService GetBlobServiceForConfig(Dictionary<string, string> configDict)
        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                .AddJsonFile("appsettings.Testing.json", true)
                .AddInMemoryCollection(new Dictionary<string, string> {{"Filesystem:RootDir", localTestDir}})
                .AddInMemoryCollection(configDict)
                .Build();
            using TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(new LoggerConfiguration().ReadFrom.Configuration(configuration).CreateLogger())
                .UseStartup<HordeStorageStartup>()
            );

            return (BlobService)server.Services.GetService<IBlobService>()!;
        }
    }
}
