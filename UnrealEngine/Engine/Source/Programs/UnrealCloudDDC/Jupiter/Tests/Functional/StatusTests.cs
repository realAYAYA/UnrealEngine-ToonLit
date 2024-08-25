// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Json;
using System.Threading.Tasks;
using Jupiter.Controllers;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Status
{
	[TestClass]
	public class StatusTests: IDisposable
	{
		private HttpClient? _httpClient;
		private TestServer? _server;

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			_server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.ConfigureTestServices(collection =>
				{
					collection.Configure<ClusterSettings>(settings =>
					{
						settings.Peers = new PeerSettings[]
						{
							new PeerSettings
							{
								Name = "use",
								FullName = "us-east-1",
								Endpoints = new PeerEndpoints[]
								{
									new PeerEndpoints { Url = new Uri("http://use-internal.jupiter.com"), IsInternal = true },
									new PeerEndpoints { Url = new Uri("http://use.jupiter.com"), IsInternal = false },
								}.ToList()
							}
						}.ToList();
					});
				})
				.UseStartup<JupiterStartup>()
			);
			_httpClient = _server.CreateClient();

			await Task.CompletedTask;
		}

		[TestMethod]
		public async Task GetPeerConnectionAsync()
		{
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/status/peers", UriKind.Relative));
			result.EnsureSuccessStatusCode();
			PeersResponse? peersResponse = await result.Content.ReadFromJsonAsync<PeersResponse>();
			Assert.IsNotNull(peersResponse);
			Assert.AreEqual("test", peersResponse.CurrentSite);

			Assert.AreEqual(1, peersResponse.Peers.Count);

			Assert.AreEqual("use", peersResponse.Peers[0].Site);
			Assert.AreEqual("us-east-1", peersResponse.Peers[0].FullName);
			Assert.AreEqual(1, peersResponse.Peers[0].Endpoints.Count);
			Assert.AreEqual(new Uri("http://use.jupiter.com"), peersResponse.Peers[0].Endpoints[0]);
		}

		
		[TestMethod]
		public async Task GetPeerConnectionInternalAsync()
		{
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/status/peers?includeInternalEndpoints=true", UriKind.Relative));
			result.EnsureSuccessStatusCode();
			PeersResponse? peersResponse = await result.Content.ReadFromJsonAsync<PeersResponse>();
			Assert.IsNotNull(peersResponse);
			Assert.AreEqual("test", peersResponse.CurrentSite);

			Assert.AreEqual(1, peersResponse.Peers.Count);

			Assert.AreEqual("use", peersResponse.Peers[0].Site);
			Assert.AreEqual("us-east-1", peersResponse.Peers[0].FullName);
			Assert.AreEqual(2, peersResponse.Peers[0].Endpoints.Count);

			Assert.AreEqual(new Uri("http://use-internal.jupiter.com"), peersResponse.Peers[0].Endpoints[0]);
			Assert.AreEqual(new Uri("http://use.jupiter.com"), peersResponse.Peers[0].Endpoints[1]);
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
