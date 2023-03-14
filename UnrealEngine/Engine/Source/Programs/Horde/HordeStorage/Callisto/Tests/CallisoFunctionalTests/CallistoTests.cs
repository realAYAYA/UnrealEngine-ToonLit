// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using Callisto.Controllers;
using Callisto.Implementation;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Newtonsoft.Json.Linq;
using Serilog;
using Serilog.Core;

namespace Callisto.FunctionalTests
{
    [TestClass]
    public class CallistoTests
    {
        private TestServer _server = null!;
        private HttpClient _httpClient = null!;
        private readonly NamespaceId TestNamespace = new NamespaceId("test");
        private readonly NamespaceId TestExistingNamespace = new NamespaceId("test-with-contents");
        private readonly NamespaceId FullFlowNamespace = new NamespaceId("full-flow");

        private IConfiguration _configuration = null!;

        [TestInitialize]
        public async Task Setup()
        {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                // we are not reading the base appSettings here as we want exact control over what runs in the tests
                .AddJsonFile("appsettings.Testing.json", true)
                .Build();

            _configuration = configuration;

            Logger logger = new LoggerConfiguration()
                .ReadFrom.Configuration(configuration)
                .CreateLogger();

            TestServer server = new TestServer(new WebHostBuilder()
                .UseConfiguration(configuration)
                .UseEnvironment("Testing")
                .UseSerilog(logger)
                .UseStartup<CallistoStartup>()
            );
            _httpClient = server.CreateClient();
            _server = server;

            // Seed S3 storage
            await Seed();
        }

        private async Task Seed()
        {
            IOptionsMonitor<CallistoSettings> settings = _server.Services.GetService<IOptionsMonitor<CallistoSettings>>()!;

            FileTransactionLog writer = new FileTransactionLog(settings, root: new DirectoryInfo(settings.CurrentValue.TransactionLogRoot), TestExistingNamespace);
            await writer.NewTransaction(new AddTransactionEvent("Foo", "Bar", blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"), }, new [] {"HQ"}.ToList()));
            await writer.NewTransaction(new AddTransactionEvent("Foo", "Bar", blobs: new[] {new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A34"), }, new [] {"HQ"}.ToList()));
        }

        [TestCleanup]
        public void Teardown()
        {
            CallistoSettings settings = new CallistoSettings();
            _configuration!.GetSection("Callisto").Bind(settings);

            Assert.IsNotNull(settings.TransactionLogRoot);
#pragma warning disable CS8604 // Possible null reference argument.
            string tempDir = Path.Combine(settings.TransactionLogRoot, TestNamespace.ToString());
#pragma warning restore CS8604 // Possible null reference argument.
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, true);
            }

#pragma warning disable CS8604 // Possible null reference argument.
            string tempDir2 = Path.Combine(settings.TransactionLogRoot, TestExistingNamespace.ToString());
#pragma warning restore CS8604 // Possible null reference argument.
            if (Directory.Exists(tempDir2))
            {
                Directory.Delete(tempDir2, true);
            }

#pragma warning disable CS8604 // Possible null reference argument.
            string tempDir3 = Path.Combine(settings.TransactionLogRoot, FullFlowNamespace.ToString());
#pragma warning restore CS8604 // Possible null reference argument.
            if (Directory.Exists(tempDir3))
            {
                Directory.Delete(tempDir3, true);
            }
        }

        [TestMethod]
        public async Task GetEvent()
        {
            const int startOffset = 0;
            HttpResponseMessage response =
                await _httpClient!.GetAsync(new Uri($"api/v1/t/{TestExistingNamespace}/{startOffset}", UriKind.Relative));
            response.EnsureSuccessStatusCode();
            dynamic ter = await response.Content.ReadAsAsync<dynamic>();
            Assert.IsNotNull(ter.generation);

            JArray events = ter.events;
            Assert.AreEqual(2, events.Count);
            AddTransactionEvent? tEvent = events[0].ToObject<AddTransactionEvent>();

            Assert.AreEqual(0, tEvent!.Identifier);
            Assert.AreEqual(70, tEvent!.NextIdentifier);
            Assert.AreEqual("Foo", tEvent!.Name);
            Assert.AreEqual("Bar", tEvent.Bucket);
            Assert.IsNotNull(tEvent.Blobs);
            Assert.AreEqual(1, tEvent.Blobs!.Length);
            Assert.AreEqual(new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"), actual: tEvent.Blobs.First());
        }

        [TestMethod]
        public async Task GetMismatchedOffsetEvent()
        {
            const int startOffset = 50; // this is not a valid offset, so we should get the object at offset 70
            HttpResponseMessage response = await _httpClient!.GetAsync(new Uri($"api/v1/t/{TestExistingNamespace}/{startOffset}", UriKind.Relative));
            response.EnsureSuccessStatusCode();
            dynamic ter = await response.Content.ReadAsAsync<dynamic>();
            Assert.IsNotNull(ter.generation);

            JArray events = ter.events;
            Assert.AreEqual(1, events.Count);
            AddTransactionEvent? tEvent = events[0].ToObject<AddTransactionEvent>();

            Assert.AreEqual(70, tEvent!.Identifier);
            Assert.AreEqual(140, tEvent!.NextIdentifier);
            Assert.AreEqual("Foo", tEvent!.Name);
            Assert.AreEqual("Bar", tEvent.Bucket);
            Assert.IsNotNull(tEvent.Blobs);
            Assert.AreEqual(1, tEvent.Blobs!.Length);
            Assert.AreEqual(new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A34"), actual: tEvent.Blobs.First());
        }

        [TestMethod]    
        public async Task GetNonExistentEvent()
        {
            // fetch the event after the second event, this will not exist
            const int startOffset = 77;
            HttpResponseMessage response = await _httpClient!.GetAsync(new Uri($"api/v1/t/{TestExistingNamespace}/{startOffset}", UriKind.Relative));
            response.EnsureSuccessStatusCode();
            dynamic ter = await response.Content.ReadAsAsync<dynamic>();
            Assert.IsNotNull(ter.generation);
            JArray events = ter.events;
            Assert.AreEqual(0, events.Count);
        }

        [TestMethod]
        public async Task AddNewEvent()
        {
            var request = new
            {
                Type = "Add",
                Name = "foo",
                Bucket = "bar",
                Blobs = new[]
                {
                    new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33")
                },
            };

            HttpResponseMessage response =
                await _httpClient!.PostAsJsonAsync(requestUri: $"api/v1/t/{TestNamespace}", request);
            response.EnsureSuccessStatusCode();
            NewTransactionResponse r = await response.Content.ReadAsAsync<NewTransactionResponse>();
            long offset = r.Offset;
            Assert.AreEqual(0, offset);
        }

        [TestMethod]
        public async Task AddAndRemoveEvent()
        {
            {
                var request = new
                {
                    Type = "Add",
                    Name = "addEvent",
                    Bucket = "foo",
                    Blobs = new[]
                    {
                        "0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"
                    },
                };

                HttpResponseMessage response =
                    await _httpClient!.PostAsJsonAsync(requestUri: $"api/v1/t/{TestNamespace}", request);
                response.EnsureSuccessStatusCode();
                NewTransactionResponse r = await response.Content.ReadAsAsync<NewTransactionResponse>();
                long offset = r.Offset;
                Assert.AreEqual(0, offset);
            }
            {
                var request = new
                {
                    Type = "Remove",
                    Name = "addEvent",
                    Bucket = "foo",
                };

                HttpResponseMessage response =
                    await _httpClient!.PostAsJsonAsync(requestUri: $"api/v1/t/{TestNamespace}", request);
                response.EnsureSuccessStatusCode();
                NewTransactionResponse r = await response.Content.ReadAsAsync<NewTransactionResponse>();
                long offset = r.Offset;
                Assert.AreEqual(75, offset);
            }
        }

        [TestMethod]
        public async Task FullFlow()
        {
            {
                var request = new
                {
                    Type = "Add",
                    Name = "addEvent",
                    Bucket = "foo",
                    Blobs = new[]
                    {
                        "0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"
                    },
                };

                HttpResponseMessage response =
                    await _httpClient!.PostAsJsonAsync(requestUri: $"api/v1/t/{FullFlowNamespace}", request);
                response.EnsureSuccessStatusCode();
                NewTransactionResponse r = await response.Content.ReadAsAsync<NewTransactionResponse>();
                long offset = r.Offset;
                Assert.AreEqual(0, offset);
            }
            {
                var request = new
                {
                    Type = "Remove",
                    Name = "addEvent",
                    Bucket = "foo",
                };

                HttpResponseMessage response =
                    await _httpClient!.PostAsJsonAsync(new Uri($"api/v1/t/{FullFlowNamespace}", UriKind.Relative), request);
                response.EnsureSuccessStatusCode();
                NewTransactionResponse r = await response.Content.ReadAsAsync<NewTransactionResponse>();
                long offset = r.Offset;
                Assert.AreEqual(75, offset);
            }

            {
                const int startOffset = 0;
                HttpResponseMessage response =
                    await _httpClient!.GetAsync(new Uri($"api/v1/t/{FullFlowNamespace}/{startOffset}", UriKind.Relative));
                response.EnsureSuccessStatusCode();
                dynamic ter = await response.Content.ReadAsAsync<dynamic>();
                JArray events = ter.events;
                Assert.AreEqual(2, events.Count);
                AddTransactionEvent? addEvent = events[0].ToObject<AddTransactionEvent>();

                Assert.AreEqual("addEvent", addEvent!.Name);
                Assert.AreEqual(0, addEvent.Identifier);
                Assert.AreEqual(75, addEvent.NextIdentifier);
                Assert.AreEqual("foo", addEvent.Bucket);
                Assert.IsNotNull(addEvent.Blobs);
                Assert.AreEqual(1, addEvent.Blobs!.Length);
                Assert.AreEqual(new BlobIdentifier("0BEEC7B5EA3F0FDBC95D0DD47F3C5BC275DA8A33"), actual: addEvent.Blobs.First());

                RemoveTransactionEvent? removeEvent = events[1].ToObject<RemoveTransactionEvent>();

                Assert.AreEqual("addEvent", removeEvent!.Name);
                Assert.AreEqual(75, removeEvent.Identifier);
                Assert.AreEqual(125, removeEvent.NextIdentifier);
                Assert.AreEqual("foo", removeEvent.Bucket);
            }
        }
    }
}
