// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using Azure.Storage.Blobs;
using Dasync.Collections;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Serilog;
using Serilog.Core;
using ContentHash = Jupiter.Implementation.ContentHash;
using IBlobStore = Horde.Storage.Implementation.IBlobStore;

namespace Horde.Storage.FunctionalTests.Storage
{
    [TestClass]
    public class S3StorageTests : StorageTests
    {
        private IAmazonS3? _s3;
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("Horde_Storage:StorageImplementations:0", HordeStorageSettings.StorageBackendImplementations.S3.ToString()),
                new KeyValuePair<string, string>("S3:BucketName", $"tests-{TestNamespaceName}")
            };
        }

        protected override async Task Seed(IServiceProvider provider)
        {
            string S3BucketName = $"tests-{TestNamespaceName}";

            _s3 = provider.GetService<IAmazonS3>();
            Assert.IsNotNull(_s3);
            if (await _s3!.DoesS3BucketExistAsync(S3BucketName))
            {
                // if we have failed to run the cleanup for some reason we run it now
                await Teardown(provider);
            }

            await _s3.PutBucketAsync(S3BucketName);
            await _s3.PutObjectAsync(new PutObjectRequest { BucketName = S3BucketName, Key = SmallFileHash.AsS3Key(), ContentBody = SmallFileContents });
            await _s3.PutObjectAsync(new PutObjectRequest { BucketName = S3BucketName, Key = AnotherFileHash.AsS3Key(), ContentBody = AnotherFileContents });
            await _s3.PutObjectAsync(new PutObjectRequest { BucketName = S3BucketName, Key = DeleteFileHash.AsS3Key(), ContentBody = DeletableFileContents });
            await _s3.PutObjectAsync(new PutObjectRequest {BucketName = S3BucketName, Key = OldBlobFileHash.AsS3Key(), ContentBody = OldFileContents});
        }

        protected override async Task Teardown(IServiceProvider provider)
        {
            string S3BucketName = $"tests-{TestNamespaceName}";
            ListObjectsResponse response = await _s3!.ListObjectsAsync(S3BucketName);
            List<KeyVersion> objectKeys = response.S3Objects.Select(o => new KeyVersion { Key = o.Key }).ToList();
            await _s3.DeleteObjectsAsync(new DeleteObjectsRequest { BucketName = S3BucketName, Objects = objectKeys });

            await _s3.DeleteBucketAsync(S3BucketName);
        }
    }

    [TestClass]
    public class AzureStorageTests : StorageTests
    {
        private AzureSettings? _settings;

        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] {new KeyValuePair<string, string>("Horde_Storage:StorageImplementations:0", HordeStorageSettings.StorageBackendImplementations.Azure.ToString())};
        }

        protected override async Task Seed(IServiceProvider provider)
        {
            _settings = provider.GetService<IOptionsMonitor<AzureSettings>>()!.CurrentValue;

            string connectionString = AzureBlobStore.GetConnectionString(_settings, provider);
            BlobContainerClient container = new BlobContainerClient(connectionString, TestNamespaceName.ToString());

            if (await container.ExistsAsync())
            {
                // if we have failed to run the cleanup for some reason we run it now
                await Teardown(provider);
            }

            await container.CreateAsync();
            BlobClient smallBlob = container.GetBlobClient(SmallFileHash.ToString());
            byte[] smallContents = Encoding.ASCII.GetBytes(SmallFileContents);
            await using MemoryStream smallBlobStream = new MemoryStream(smallContents);
            await smallBlob.UploadAsync(smallBlobStream);

            BlobClient anotherBlob = container.GetBlobClient(AnotherFileHash.ToString());
            byte[] anotherContents = Encoding.ASCII.GetBytes(AnotherFileContents);
            await using MemoryStream anotherContentsStream = new MemoryStream(anotherContents);
            await anotherBlob.UploadAsync(anotherContentsStream);

            BlobClient deleteBlob = container.GetBlobClient(DeleteFileHash.ToString());
            byte[] deleteContents = Encoding.ASCII.GetBytes(DeletableFileContents);
            await using MemoryStream deleteContentsStream = new MemoryStream(deleteContents);
            await deleteBlob.UploadAsync(deleteContentsStream);

            BlobClient oldBlob = container.GetBlobClient(OldBlobFileHash.ToString());
            byte[] oldBlobContents = Encoding.ASCII.GetBytes(OldFileContents);
            await using MemoryStream oldBlobContentsSteam = new MemoryStream(oldBlobContents);
            await oldBlob.UploadAsync(oldBlobContentsSteam);
        }

        protected override async Task Teardown(IServiceProvider provider)
        {
            string connectionString = AzureBlobStore.GetConnectionString( _settings!, provider);
            BlobContainerClient container = new BlobContainerClient(connectionString, TestNamespaceName.ToString());

            await container.DeleteAsync();
        }
    }

    
    [TestClass]
    public class FileSystemStoreTests : StorageTests
    {
        private readonly string _localTestDir;
        private readonly NamespaceId FooNamespace = new NamespaceId("foo");

        public FileSystemStoreTests()
        {
            _localTestDir = Path.Combine(Path.GetTempPath(), "IoFileSystemTests", Path.GetRandomFileName());
        }
        protected override IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[] { 
                new KeyValuePair<string, string>("Horde_Storage:StorageImplementations:0", HordeStorageSettings.StorageBackendImplementations.FileSystem.ToString()),
                new KeyValuePair<string, string>("Filesystem:RootDir", _localTestDir)
            };
        }

        protected override async Task Seed(IServiceProvider provider)
        {
            NamespaceId folderName = TestNamespaceName;
            
            Directory.CreateDirectory(_localTestDir);

            FileInfo smallFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, SmallFileHash);
            smallFileInfo.Directory?.Create();
            await File.WriteAllBytesAsync(
                smallFileInfo.FullName,
                Encoding.ASCII.GetBytes(SmallFileContents)
            );

            FileInfo anotherFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, AnotherFileHash);
            anotherFileInfo.Directory?.Create();
            await File.WriteAllBytesAsync(
                anotherFileInfo.FullName,
                Encoding.ASCII.GetBytes(AnotherFileContents)
            );
            
            FileInfo deleteFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, DeleteFileHash);
            deleteFileInfo.Directory?.Create();
            await File.WriteAllBytesAsync(
                deleteFileInfo.FullName,
                Encoding.ASCII.GetBytes(DeletableFileContents)
            );

            // a old file used to verify cutoff filtering
            FileInfo oldFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, OldBlobFileHash);
            oldFileInfo.Directory?.Create();
            await File.WriteAllBytesAsync(
                oldFileInfo.FullName,
                Encoding.ASCII.GetBytes(OldFileContents)
            );
            File.SetLastWriteTimeUtc(oldFileInfo.FullName, DateTime.Now.AddDays(-7));
        }

        protected override Task Teardown(IServiceProvider provider)
        {
            Directory.Delete(_localTestDir, true);

            return Task.CompletedTask;
        }

        // only a file system allows us to update the last modified time of the object to actually execute this test
        [TestMethod]
        public async Task ListOldBlobs()
        {
            FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
            Assert.IsNotNull(fsStore);

            // we fetch all objects that are more then a day old
            // as the old blob is set to be a week old this should be the only object returned
            DateTime cutoff = DateTime.Now.AddDays(-1);
            {
                BlobIdentifier[] blobs = await fsStore.ListObjects(TestNamespaceName).Where(tuple => tuple.Item2 < cutoff).Select(tuple => tuple.Item1).ToArrayAsync();
                Assert.AreEqual(OldBlobFileHash, blobs[0]);
            }
        }

        [TestMethod]
        public async Task ListNamespaces()
        {
            FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
            Assert.IsNotNull(fsStore);

            List<NamespaceId> namespaces = await fsStore.ListNamespaces().ToListAsync();
            Assert.AreEqual(1, namespaces.Count);
            
            await fsStore.PutObject(FooNamespace, Encoding.ASCII.GetBytes(SmallFileContents), SmallFileHash);
            
            namespaces = await fsStore.ListNamespaces().ToListAsync();
            Assert.AreEqual(2, namespaces.Count);
        }

        [TestMethod]
        public async Task GarbageCollect()
        {
            // Remove data added from test seeding
            Directory.Delete(_localTestDir, true);
            Directory.CreateDirectory(_localTestDir);
            
            FilesystemSettings fsSettings = Server!.Services.GetService<IOptionsMonitor<FilesystemSettings>>()!.CurrentValue;
            fsSettings.MaxSizeBytes = 600;
            FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
            Assert.IsNotNull(fsStore);

            using CancellationTokenSource cts = new CancellationTokenSource();
            Assert.IsTrue(await fsStore.CleanupInternal(cts.Token, batchSize: 2) == 0); // No garbage to collect, should return false
            
            FileInfo[] fooFiles = CreateFilesInNamespace(FooNamespace, 10);

            Assert.AreEqual(10 * 100, await fsStore.CalculateDiskSpaceUsed());

            Assert.IsTrue(await fsStore.CleanupInternal(cts.Token, batchSize: 2) > 0);

            Assert.AreEqual(5 * 100, await fsStore.CalculateDiskSpaceUsed());

            fooFiles.ToList().ForEach(x => x.Refresh());
            Assert.IsTrue(fooFiles[0].Exists); // Most recently accessed/modified
            Assert.IsTrue(fooFiles[1].Exists);
            Assert.IsTrue(fooFiles[2].Exists);
            Assert.IsTrue(fooFiles[3].Exists);
            Assert.IsTrue(fooFiles[4].Exists);
            Assert.IsFalse(fooFiles[5].Exists);
            Assert.IsFalse(fooFiles[6].Exists);
            Assert.IsFalse(fooFiles[7].Exists);
            Assert.IsFalse(fooFiles[8].Exists);
            Assert.IsFalse(fooFiles[9].Exists); // Least recently accessed/modified
        }
        
        [TestMethod]
        public void GetLeastRecentlyAccessedObjects()
        {
            FileInfo[] fooFiles = CreateFilesInNamespace(FooNamespace, 10);
            CreateFilesInNamespace(new NamespaceId("bar"), 10);

            FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
            Assert.IsNotNull(fsStore);

            Assert.AreEqual(10, (fsStore.GetLeastRecentlyAccessedObjects(FooNamespace)).ToArray().Length);

            FileInfo[] results = fsStore.GetLeastRecentlyAccessedObjects(FooNamespace, 3).ToArray();
            Assert.AreEqual(3, results.Length);
            Assert.AreEqual(fooFiles[7].LastAccessTime, results[2].LastAccessTime);
            Assert.AreEqual(fooFiles[8].LastAccessTime, results[1].LastAccessTime);
            Assert.AreEqual(fooFiles[9].LastAccessTime, results[0].LastAccessTime);
        }
        
        [TestMethod]
        public async Task CalculateUsedDiskSpace()
        {
            FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
            Assert.IsNotNull(fsStore);
            await fsStore.PutObject(FooNamespace, Encoding.ASCII.GetBytes(SmallFileContents), SmallFileHash);
            await fsStore.PutObject(FooNamespace, Encoding.ASCII.GetBytes(AnotherFileContents), AnotherFileHash);
            
            Assert.AreEqual(SmallFileContents.Length + AnotherFileContents.Length, await fsStore.CalculateDiskSpaceUsed(FooNamespace));
        }
        
        private FileInfo[] CreateFilesInNamespace(NamespaceId ns, int numFiles)
        {
            FileInfo[] files = new FileInfo[numFiles];
            for (int i = 0; i < numFiles; i++)
            {
                byte[] content = Encoding.ASCII.GetBytes(i + 1000 + new string('j', 96));
                BlobIdentifier bi = BlobIdentifier.FromBlob(content);
                FileInfo fi = FileSystemStore.GetFilesystemPath(_localTestDir, ns, bi);
                fi.Directory?.Create();
                File.WriteAllBytes(fi.FullName, content);
                fi.LastWriteTime = DateTime.UtcNow.AddDays(-i);
                fi.Refresh();
                files[i] = fi;
            }

            return files;
        }
    }

    public abstract class StorageTests
    {
        protected TestServer? Server { get; set; }
        protected NamespaceId TestNamespaceName { get; } = new NamespaceId("testbucket");

        private HttpClient? _httpClient;

        protected const string SmallFileContents = "Small file contents";
        protected const string AnotherFileContents = "Another file with contents";
        protected const string DeletableFileContents = "Delete Me";
        protected const string OldFileContents = "a old blob used for testing cutoff filtering";

        protected BlobIdentifier SmallFileHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(SmallFileContents));
        protected BlobIdentifier AnotherFileHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(AnotherFileContents));
        protected BlobIdentifier DeleteFileHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(DeletableFileContents));
        protected BlobIdentifier OldBlobFileHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(OldFileContents));

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
            Server = server;

            // Seed storage
            await Seed(Server.Services);
        }

        protected abstract IEnumerable<KeyValuePair<string, string>> GetSettings();

        protected abstract Task Seed(IServiceProvider serverServices);
        protected abstract Task Teardown(IServiceProvider serverServices);

        [TestCleanup]
        public async Task MyTeardown()
        {
            await Teardown(Server!.Services);
        }

        [TestMethod]
        public async Task GetSmallFile()
        {
            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{SmallFileHash}", UriKind.Relative));
            result.EnsureSuccessStatusCode();
            string content = await result.Content.ReadAsStringAsync();
            Assert.AreEqual(SmallFileContents, content);
        }

        [TestMethod]
        public async Task GetNotExistentFile()
        {
            byte[] payload = Encoding.ASCII.GetBytes("This content does not exist");
            ContentHash contentHash = ContentHash.FromBlob(payload);
            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));

            Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
        }

        [TestMethod]
        public async Task GetInvalidHash()
        {
            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/smallFile", UriKind.Relative));

            Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
        }

        [TestMethod]
        public async Task PutSmallBlob()
        {
            byte[] payload = Encoding.ASCII.GetBytes("I am a small blob");
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            BlobIdentifier contentHash = BlobIdentifier.FromBlob(payload);
            HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative), requestContent);

            result.EnsureSuccessStatusCode();
            InsertResponse content = await result.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(contentHash, content.Identifier);
        }

        [TestMethod]
        public async Task PostSmallBlob()
        {
            byte[] payload = Encoding.ASCII.GetBytes("I am a small blob");
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            BlobIdentifier contentHash = BlobIdentifier.FromBlob(payload);
            HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v1/s/{TestNamespaceName}", UriKind.Relative), requestContent);

            result.EnsureSuccessStatusCode();
            InsertResponse content = await result.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(contentHash, content.Identifier);
        }

        [TestMethod]
        public async Task DeleteBlob()
        {
            HttpResponseMessage result = await  _httpClient!.DeleteAsync(new Uri($"api/v1/s/{TestNamespaceName}/{DeleteFileHash}", UriKind.Relative));
            result.EnsureSuccessStatusCode();

            Assert.AreEqual(HttpStatusCode.NoContent, result.StatusCode);
        }

        [TestMethod]
        public async Task ListBlobs()
        {
            List<BlobIdentifier> validBlobHashes = new List<BlobIdentifier>
            {
                SmallFileHash,
                AnotherFileHash,
                DeleteFileHash,
                OldBlobFileHash
            };

            IBlobService blobService = Server!.Services.GetService<IBlobService>()!;

            BlobIdentifier[] oldObjects = await blobService.ListObjects(TestNamespaceName).Select(tuple => tuple.Item1).ToArrayAsync();

            Assert.AreEqual(4, oldObjects.Length);
            Assert.IsTrue(validBlobHashes.Contains(oldObjects[0]));
            Assert.IsTrue(validBlobHashes.Contains(oldObjects[1]));
            Assert.IsTrue(validBlobHashes.Contains(oldObjects[2]));
            Assert.IsTrue(validBlobHashes.Contains(oldObjects[3]));
        }
        
        [TestMethod]
        public async Task BlobExists()
        {
            {
                using HttpRequestMessage message = new(HttpMethod.Head, new Uri($"api/v1/s/{TestNamespaceName}/{SmallFileHash}", UriKind.Relative));
                HttpResponseMessage result = await _httpClient!.SendAsync(message);
                Assert.AreEqual(HttpStatusCode.OK, result.StatusCode);
            }

            {
                ContentHash newContent = ContentHash.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
                using HttpRequestMessage message = new (HttpMethod.Head, new Uri($"api/v1/s/{TestNamespaceName}/{newContent}", UriKind.Relative));
                HttpResponseMessage resultNew = await _httpClient!.SendAsync(message);
                Assert.AreEqual(HttpStatusCode.NotFound, resultNew.StatusCode);
                string content = await resultNew.Content.ReadAsStringAsync();
                ValidationProblemDetails result = JsonConvert.DeserializeObject<ValidationProblemDetails>(content)!;
                Assert.AreEqual($"Blob {newContent} not found", result.Title);
            }
        }

        [TestMethod]
        public async Task BlobExistsBatch()
        {
            BlobIdentifier newContent = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));

            var ops = new
            {
                Operations = new object[]
                {
                    new
                    {
                        Op = "HEAD",
                        Namespace = TestNamespaceName,
                        Id = SmallFileHash,
                    },
                    new
                    {
                        Op = "HEAD",
                        Namespace = TestNamespaceName,
                        Id = newContent,
                    }
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
            response.EnsureSuccessStatusCode();
            string content = await response.Content.ReadAsStringAsync();
            object result = JsonConvert.DeserializeObject(content)!;
            JArray? array = result as JArray;
            Assert.IsNotNull(array);
            Assert.AreEqual(2, array!.Count);
            Assert.IsNull(array[0].Value<string>());
            BlobIdentifier id = new BlobIdentifier(array[1].Value<string>()!);
            Assert.AreEqual(newContent, id);
        }

        [TestMethod]
        public async Task BatchOp()
        {
            var ops = new
            {
                Operations = new object[]
                {
                    new
                    {
                        Op = "GET",
                        Namespace = TestNamespaceName,
                        Id = SmallFileHash,
                    },
                    new
                    {
                        Op = "PUT",
                        Namespace = TestNamespaceName,
                        Id = DeleteFileHash,
                        Content = Encoding.ASCII.GetBytes(DeletableFileContents)
                    }
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
            response.EnsureSuccessStatusCode();
            string content = await response.Content.ReadAsStringAsync();
            object result = JsonConvert.DeserializeObject(content)!;
            Assert.IsTrue(result != null);
            JArray? array = result as JArray;
            Assert.IsNotNull(array);
            Assert.AreEqual(2, array!.Count);
            string base64Content = array[0].Value<string>()!;
            string convertedContent = Encoding.ASCII.GetString(Convert.FromBase64String(base64Content));
            Assert.AreEqual(SmallFileContents, convertedContent);
            BlobIdentifier identifier = new BlobIdentifier(array[1].Value<string>()!);
            Assert.AreEqual(DeleteFileHash, identifier);
        }

        [TestMethod]
        public async Task BatchOpBadRequest()
        {
            var ops = new
            {
                Operations = new object[]
                {
                    new
                    {
                        Op = "GET",
                        Namespace = TestNamespaceName,
                        Id =  BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("foo"))
                    },
                    new
                    {
                        Op = "PUT",
                        Namespace = TestNamespaceName,
                        // no content
                    },
                    new
                    {
                        Op = "notAValidEnumValue"
                    }
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
            Assert.AreEqual(HttpStatusCode.BadRequest, response.StatusCode);

            string content = await response.Content.ReadAsStringAsync();
            SerializableError? result = JsonConvert.DeserializeObject<SerializableError>(content);
            Assert.IsNotNull(result);

            Assert.AreEqual(2, result.Keys.Count);
        }

        [TestMethod]
        public async Task BatchOpBlobNotPresent()
        {
            var ops = new
            {
                Operations = new object[]
                {
                    new
                    {
                        Op = "HEAD",
                        Namespace = TestNamespaceName,
                        Id = "1DEE7232FE05FEFE623D2119626F103E05D3EE98", // random hash that does not exist
                    },
                }
            };

            HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);

            string content = await response.Content.ReadAsStringAsync();
            string[]? result = JsonConvert.DeserializeObject<string[]>(content);
            Assert.IsNotNull(result);

            Assert.AreEqual(1, result.Length);
            Assert.AreEqual("1DEE7232FE05FEFE623D2119626F103E05D3EE98", result[0]);
        }

        [TestMethod]
        public async Task MultipleBlobChecks()
        {
            BlobIdentifier newContent = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
            using HttpRequestMessage message = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exists?id={SmallFileHash}&id={newContent}", UriKind.Relative));
            HttpResponseMessage response = await _httpClient!.SendAsync(message);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
            Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

            string s = await response.Content.ReadAsStringAsync();
            HeadMultipleResponse? result = JsonConvert.DeserializeObject<HeadMultipleResponse>(s);

            Assert.IsNotNull(result);
            Assert.AreEqual(1, result.Needs.Length);
            Assert.AreEqual(newContent, result.Needs[0]);
        }

        [TestMethod]
        public async Task MultipleBlobChecksBodyCB()
        {
            BlobIdentifier newContent = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
            using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exist", UriKind.Relative));
            CbWriter writer = new CbWriter();
            writer.BeginUniformArray(CbFieldType.Hash);
            writer.WriteHashValue(SmallFileHash.AsIoHash());
            writer.WriteHashValue(newContent.AsIoHash());
            writer.EndUniformArray();
            byte[] buf = writer.ToByteArray();
            request.Content = new ByteArrayContent(buf);
            request.Content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

            HttpResponseMessage response = await _httpClient!.SendAsync(request);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
            Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

            string s = await response.Content.ReadAsStringAsync();
            HeadMultipleResponse? result = JsonConvert.DeserializeObject<HeadMultipleResponse>(s);

            Assert.IsNotNull(result);
            Assert.AreEqual(1, result.Needs.Length);
            Assert.AreEqual(newContent, result.Needs[0]);
        }

        [TestMethod]
        public async Task MultipleBlobChecksBodyJson()
        {
            BlobIdentifier newContent = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
            using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exist", UriKind.Relative));
            string jsonBody = JsonConvert.SerializeObject(new BlobIdentifier[] { SmallFileHash, newContent });
            request.Content = new StringContent(jsonBody, Encoding.UTF8, MediaTypeNames.Application.Json);

            HttpResponseMessage response = await _httpClient!.SendAsync(request);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
            Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

            string s = await response.Content.ReadAsStringAsync();
            HeadMultipleResponse? result = JsonConvert.DeserializeObject<HeadMultipleResponse>(s);

            Assert.IsNotNull(result);
            Assert.AreEqual(1, result.Needs.Length);
            Assert.AreEqual(newContent, result.Needs[0]);
        }

        [TestMethod]
        public async Task MultipleBlobCompactBinaryResponse()
        {
            BlobIdentifier newContent = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
            using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exists?id={SmallFileHash}&id={newContent}", UriKind.Relative));
            request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CustomMediaTypeNames.UnrealCompactBinary));
            HttpResponseMessage response = await _httpClient!.SendAsync(request);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
            Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, response.Content.Headers.ContentType!.MediaType!);
            byte[] data = await response.Content.ReadAsByteArrayAsync();
            CbObject cb = new CbObject(data);

            Assert.AreEqual(1, cb.Count());
            CbField? needs = cb["needs"];
            Assert.IsNotNull(needs);
            IoHash[] neededBlobs = needs!.AsArray().Select(field => field.AsHash()).ToArray();

            Assert.AreEqual(1, neededBlobs.Length);
            Assert.AreEqual(newContent, BlobIdentifier.FromIoHash(neededBlobs[0]));
        }

        [TestMethod]
        public async Task FullFlow()
        {
            byte[] payload = Encoding.ASCII.GetBytes("Foo bar");
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            BlobIdentifier contentHash = BlobIdentifier.FromBlob(payload);
            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            InsertResponse response = await putResponse.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(contentHash, response.Identifier);

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            CollectionAssert.AreEqual(payload, actual: await getResponse.Content.ReadAsByteArrayAsync());

            HttpResponseMessage deleteResponse = await _httpClient.DeleteAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));
            deleteResponse.EnsureSuccessStatusCode();
            Assert.AreEqual(HttpStatusCode.NoContent, deleteResponse.StatusCode);
        }

        /// <summary>
        /// write a large file (bigger then that C# can have in memory)
        /// </summary>
        
        [TestMethod]
        public async Task PutGetLargePayload()
        {
            // we submit a blob so large that it can not fit using the memory blob store
            IBlobStore? blobStore = Server?.Services.GetService<IBlobStore>();
            Assert.IsFalse(blobStore is MemoryBlobStore);

            if (blobStore is AzureBlobStore)
            {
                Assert.Inconclusive("Azure blob store gets internal server errors when receiving large blobs");
            }

            FileInfo fi = new FileInfo(Path.GetTempFileName());

            FileInfo tempOutputFile = new FileInfo(Path.GetTempFileName());

            try
            {
                {
                    await using FileStream fs = fi.OpenWrite();

                    byte[] block = new byte[1024 * 1024];
                    Array.Fill(block, (byte)'a');
                    // we want a file larger then 2GB, each block is 1 MB
                    int countOfBlocks = 2100;
                    for (int i = 0; i < countOfBlocks; i++)
                    {
                        await fs.WriteAsync(block, 0, block.Length);
                    }
                }

                BlobIdentifier blobIdentifier;
                {
                    await using FileStream fs = fi.OpenRead();
                    blobIdentifier = await BlobIdentifier.FromStream(fs);
                }

                {
                    await using FileStream fs = fi.OpenRead();
                    using StreamContent content = new StreamContent(fs);
                    content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
                    HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), content);
                    result.EnsureSuccessStatusCode();

                    InsertResponse response = await result.Content.ReadAsAsync<InsertResponse>();
                    Assert.AreEqual(blobIdentifier, response.Identifier);
                }
                
                {
                    // verify we can fetch the blob again
                    HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), HttpCompletionOption.ResponseHeadersRead);
                    result.EnsureSuccessStatusCode();
                    Stream s = await result.Content.ReadAsStreamAsync();

                    {
                        // stream this to disk so we have something to look at in case there is an error
                        await using FileStream fs = tempOutputFile.OpenWrite();
                        await s.CopyToAsync(fs);
                        fs.Close();
                        s.Close();

                        s = tempOutputFile.OpenRead();
                    }

                    BlobIdentifier downloadedBlobIdentifier = await BlobIdentifier.FromStream(s);
                    Assert.AreEqual(blobIdentifier, downloadedBlobIdentifier);
                    s.Close();
                }
            }
            finally
            {
                if (fi.Exists)
                {
                    fi.Delete();
                }

                if (tempOutputFile.Exists)
                {
                    tempOutputFile.Delete();
                }
            }
        }
    }

    public class InsertResponse
    {
        public BlobIdentifier? Identifier { get; set; }
    }
}
