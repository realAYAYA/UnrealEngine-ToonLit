// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Contrib.HttpClient;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Storage
{
    [TestClass]
    public class PeerBlobTest
    {
        protected const string S3FileContent = "Foo bar content that goes into s3";
        protected const string OtherPeerContent = "This content will be faked from another instance";

        protected BlobIdentifier S3FileContentHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(S3FileContent));
        protected BlobIdentifier OtherPeerContentHash { get; } = BlobIdentifier.FromBlob(Encoding.ASCII.GetBytes(OtherPeerContent));

        private IAmazonS3? _s3;
        protected NamespaceId TestNamespaceName { get; } = new NamespaceId("testbucket");
        protected TestServer? Server { get; set; }

        private HttpClient? _httpClient;
        private Mock<HttpMessageHandler>? _otherPeerMock;

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

            _otherPeerMock = new Mock<HttpMessageHandler>();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<JupiterStartup>()
                .ConfigureTestServices(collection =>
                {
                    string storageLayerName = nameof(FileSystemStore);
                    // configure a faked remote peer instance that can serve this blob
                    _otherPeerMock.SetupRequest($"http://other-peer-instance.com/api/v1/blobs/{TestNamespaceName}/{S3FileContentHash}?storageLayers={storageLayerName}").ReturnsResponse(HttpStatusCode.OK, S3FileContent).Verifiable();
                    _otherPeerMock.SetupRequest($"http://other-peer-instance.com/api/v1/blobs/{TestNamespaceName}/{OtherPeerContentHash}?storageLayers={storageLayerName}").ReturnsResponse(HttpStatusCode.OK, OtherPeerContent).Verifiable();

                    IHttpClientFactory mockFactory = _otherPeerMock.CreateClientFactory();

                    Mock.Get(mockFactory).Setup(x => x.CreateClient("other-peer-instance.com"))
                    .Returns(() =>
                    {
                        HttpClient? client = _otherPeerMock.CreateClient();
                        client.BaseAddress = new Uri("http://other-peer-instance.com");
                        return client;
                    });

                    collection.AddSingleton<IHttpClientFactory>(mockFactory);
                })

            );
            _httpClient = server.CreateClient();
            Server = server;

            // Seed storage
            await Seed(Server.Services);
        }

        [TestCleanup]
        public async Task MyTeardown()
        {
            await Teardown(Server!.Services);
        }
        protected IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.Peer.ToString()),
                new KeyValuePair<string, string>("UnrealCloudDDC:StorageImplementations:1", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
                new KeyValuePair<string, string>("ServiceDiscovery:Peers:0", "other-peer-instance.com"),
                new KeyValuePair<string, string>("S3:BucketName", $"tests-{TestNamespaceName}")
            };
        }

        protected async Task Seed(IServiceProvider provider)
        {
            string S3BucketName = $"tests-{TestNamespaceName}";

            _s3 = provider.GetService<IAmazonS3>();
            Assert.IsNotNull(_s3);
            if (await _s3!.DoesS3BucketExistAsync(S3BucketName))
            {
                // if we have failed to run the cleanup for some reason we run it now
                await Teardown(provider);
            }
        }

        protected async Task Teardown(IServiceProvider provider)
        {
            _s3 = provider.GetService<IAmazonS3>();
            Assert.IsNotNull(_s3);

            string S3BucketName = $"tests-{TestNamespaceName}";
            ListObjectsResponse response = await _s3!.ListObjectsAsync(S3BucketName);
            List<KeyVersion> objectKeys = response.S3Objects.Select(o => new KeyVersion { Key = o.Key }).ToList();
            await _s3.DeleteObjectsAsync(new DeleteObjectsRequest { BucketName = S3BucketName, Objects = objectKeys });

            await _s3.DeleteBucketAsync(S3BucketName);
        }

        [TestMethod]
        public async Task ForceFetchBlobFromS3()
        {
            byte[] s3ContentBytes = Encoding.ASCII.GetBytes(S3FileContent);
            using ByteArrayContent requestContent = new ByteArrayContent(s3ContentBytes);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = S3FileContent.Length;
            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{S3FileContentHash}", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            InsertResponse response = await putResponse.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(S3FileContentHash, response.Identifier);

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{S3FileContentHash}?storageLayers=AmazonS3Store", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            CollectionAssert.AreEqual(s3ContentBytes, actual: await getResponse.Content.ReadAsByteArrayAsync());

            _otherPeerMock!.VerifyNoOtherCalls();

        }

        
        [TestMethod]
        public async Task ForceFetchBlobFromNoneExistentLayer()
        {
            byte[] s3ContentBytes = Encoding.ASCII.GetBytes(S3FileContent);
            using ByteArrayContent requestContent = new ByteArrayContent(s3ContentBytes);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = S3FileContent.Length;
            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{S3FileContentHash}", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            InsertResponse response = await putResponse.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(S3FileContentHash, response.Identifier);

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{S3FileContentHash}?storageLayers=ThisImplemenationDoesNotExist", UriKind.Relative));
            Assert.AreEqual(HttpStatusCode.NotFound, getResponse.StatusCode);
        }

        [TestMethod]
        public async Task FetchBlobFromOtherInstance()
        {
            byte[] payload = Encoding.ASCII.GetBytes(OtherPeerContent);
            using ByteArrayContent requestContent = new ByteArrayContent(payload);
            requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            requestContent.Headers.ContentLength = payload.Length;
            HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{OtherPeerContentHash}", UriKind.Relative), requestContent);
            putResponse.EnsureSuccessStatusCode();
            InsertResponse response = await putResponse.Content.ReadAsAsync<InsertResponse>();
            Assert.AreEqual(OtherPeerContentHash, response.Identifier);

            HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{OtherPeerContentHash}", UriKind.Relative));
            getResponse.EnsureSuccessStatusCode();
            CollectionAssert.AreEqual(payload, actual: await getResponse.Content.ReadAsByteArrayAsync());

            string storageLayerName = nameof(FileSystemStore);
            _otherPeerMock!.VerifyRequest(HttpMethod.Get, new Uri($"http://other-peer-instance.com/api/v1/blobs/{TestNamespaceName}/{OtherPeerContentHash}?storageLayers={storageLayerName}"));
            _otherPeerMock!.VerifyNoOtherCalls();
        }
    }
}
