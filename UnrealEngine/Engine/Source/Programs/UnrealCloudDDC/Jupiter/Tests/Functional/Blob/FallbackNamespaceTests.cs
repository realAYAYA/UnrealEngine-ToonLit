// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.DependencyInjection;

namespace Jupiter.FunctionalTests.Storage
{

    [TestClass]
    public class FallbackNamespaceTests
    {
        private static TestServer? _server;
        private HttpClient? _httpClient;

        private readonly NamespaceId TestNamespace = new NamespaceId("first-namespace");
        private readonly NamespaceId FallbackNamespace = new NamespaceId("fallback-namespace");

        protected const string FileContents = "This is some test contents for fallback namespaces";
        protected static readonly byte[] FileContentsBytes = Encoding.ASCII.GetBytes(FileContents);
        protected BlobIdentifier FileHash { get; } = BlobIdentifier.FromBlob(FileContentsBytes);

        [TestInitialize]
        public async Task Setup()

        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", true)
                .AddEnvironmentVariables()
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
            _httpClient = server.CreateClient();

            await SeedDb(server.Services);
        }

        private async Task SeedDb(IServiceProvider provider)
        {
            IBlobService blobStore = provider.GetService<IBlobService>()!;

            await blobStore.PutObject(FallbackNamespace, FileContentsBytes, FileHash);
        }

        [TestMethod]
        public async Task GetBlobFromFallbackNamespace()
        {
            // we request the data from the first namespace but it only exists in the fallback namespace
            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{FileHash}", UriKind.Relative));
            result.EnsureSuccessStatusCode();
            string content = await result.Content.ReadAsStringAsync();
            Assert.AreEqual(FileContents, content);
        }

        [TestMethod]
        public async Task GetBlobFromFallbackNamespaceWithoutAccess()
        {
            // the second namespace is configured to fallback to a namespace we lack access to
            NamespaceId TestNamespace2 = new NamespaceId("second-namespace");
            
            // we request the data from the first namespace but it only exists in the fallback namespace
            HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespace2}/{FileHash}", UriKind.Relative));
            Assert.AreEqual(HttpStatusCode.Forbidden, result.StatusCode);
        }
    }
}
