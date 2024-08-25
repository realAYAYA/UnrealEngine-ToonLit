// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation.Blob;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Contrib.HttpClient;
using Serilog;
using Logger = Serilog.Core.Logger;
using Jupiter.Common;

namespace Jupiter.FunctionalTests.Storage
{
	[TestClass]
	public class BlobReplicationTests
	{ 
		protected NamespaceId TestNamespaceName { get; } = new NamespaceId("test-namespace");

		[TestMethod]
		public async Task ReplicateBlobFromRegionAsync()
		{
			// 2 regions with data 
			// region A - will not have data
			// region B - will have data

			string contents = "This is a random string of content";
			byte[] bytes = Encoding.ASCII.GetBytes(contents);
			BlobId blobIdentifier = BlobId.FromBlob(bytes);
			
			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();

			// site b has the content and will serve it
			handler.SetupRequest($"http://siteB.com/internal/api/v1/blobs/{TestNamespaceName}/{blobIdentifier}?allowOndemandReplication=false").ReturnsResponse(HttpStatusCode.OK,
				message => { message.Content = new ReadOnlyMemoryContent(bytes);}
			).Verifiable();

			handler.SetupRequest($"http://siteA.com/internal/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();
			handler.SetupRequest($"http://siteA.com/public/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();

			handler.SetupRequest($"http://siteB.com/internal/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();
			handler.SetupRequest($"http://siteB.com/public/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();

			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(new[] { 
					new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Memory.ToString()), 
					new KeyValuePair<string, string?>("UnrealCloudDDC:EnableOnDemandReplication", true.ToString()),
				})
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			using TestServer server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.ConfigureTestServices(collection =>
				{
					collection.Configure<ClusterSettings>(settings =>
					{
						settings.Peers = new PeerSettings[]
						{
							new PeerSettings()
							{
								Name = "siteA",
								FullName = "siteA",
								Endpoints = new PeerEndpoints[]
								{
									new PeerEndpoints()
									{
										Url = new Uri("http://siteA.com/internal"),
										IsInternal = true
									},
									new PeerEndpoints()
									{
										Url = new Uri("http://siteA.com/public")
									},
								}.ToList()
							},
							new PeerSettings()
							{
								Name = "siteB",
								FullName = "siteB",
								Endpoints = new PeerEndpoints[]
								{
									new PeerEndpoints()
									{
										Url = new Uri("http://siteB.com/internal"),
										IsInternal = true
									},
									new PeerEndpoints()
									{
										Url = new Uri("http://siteB.com/public")
									},
								}.ToList()
							},

						}.ToList();
					});
					collection.Configure<NamespaceSettings>(settings =>
					{
						settings.Policies = new Dictionary<string, NamespacePolicy>()
						{
							{
								TestNamespaceName.ToString(), new NamespacePolicy()
								{
									OnDemandReplication = true,
									Acls = new List<AclEntry>()
									{
										new AclEntry()
										{
											Actions = new List<JupiterAclAction>()
											{
												JupiterAclAction.ReadObject,
											},
											Claims = new List<string>()
											{
												"*"
											}
										}
									}
								}
							}
						};
					});

					collection.AddSingleton<IHttpClientFactory>(handler.CreateClientFactory());
				})
				.UseStartup<JupiterStartup>()
			);

			IBlobIndex? blobIndex = server.Services.GetService<IBlobIndex>();
			Assert.IsNotNull(blobIndex);

			await blobIndex.AddBlobToIndexAsync(TestNamespaceName, blobIdentifier, "siteB");

			HttpClient httpClient = server.CreateClient();

			HttpResponseMessage response = await httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative));
			response.EnsureSuccessStatusCode();

			byte[] responseData = await response.Content.ReadAsByteArrayAsync();
			CollectionAssert.AreEqual(bytes, responseData);

			handler.Verify();
		}

				
		[TestMethod]
		public async Task ReplicateBlobNotPresentAsync()
		{
			// verify calling the blob endpoint when the blob is missing still returns a 404

			string contents = "This is a random string of content";
			byte[] bytes = Encoding.ASCII.GetBytes(contents);
			BlobId blobIdentifier = BlobId.FromBlob(bytes);
			
			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();

			handler.SetupRequest($"http://siteA.com/internal/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();
			handler.SetupRequest($"http://siteA.com/public/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();

			handler.SetupRequest($"http://siteB.com/internal/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();
			handler.SetupRequest($"http://siteB.com/public/health/live").ReturnsResponse(HttpStatusCode.OK).Verifiable();

			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Memory.ToString()) })
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			using TestServer server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.ConfigureTestServices(collection =>
				{
					collection.Configure<ClusterSettings>(settings =>
					{
						settings.Peers = new PeerSettings[]
						{
							new PeerSettings()
							{
								Name = "siteA",
								FullName = "siteA",
								Endpoints = new PeerEndpoints[]
								{
									new PeerEndpoints()
									{
										Url = new Uri("http://siteA.com/internal"),
										IsInternal = true
									},
									new PeerEndpoints()
									{
										Url = new Uri("http://siteA.com/public")
									},
								}.ToList()
							},
							new PeerSettings()
							{
								Name = "siteB",
								FullName = "siteB",
								Endpoints = new PeerEndpoints[]
								{
									new PeerEndpoints()
									{
										Url = new Uri("http://siteB.com/internal"),
										IsInternal = true
									},
									new PeerEndpoints()
									{
										Url = new Uri("http://siteB.com/public")
									},
								}.ToList()
							},

						}.ToList();
					});
					collection.AddSingleton<IHttpClientFactory>(handler.CreateClientFactory());
				})
				.UseStartup<JupiterStartup>()
			);

			HttpClient httpClient = server.CreateClient();
			HttpResponseMessage response = await httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative));
			Assert.IsTrue(response.StatusCode == HttpStatusCode.NotFound);
		}
	}
}
