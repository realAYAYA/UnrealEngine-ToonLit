// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Contrib.HttpClient;
using Microsoft.Extensions.Logging.Abstractions;

namespace Jupiter.FunctionalTests.Status
{
	[TestClass]
	public class PeerStatusTests
	{
		[TestMethod]
		public async Task GetPeerConnectionAsync()
		{
			ClusterSettings settings = new ClusterSettings
			{
				Peers = new PeerSettings[]
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

				}.ToList()
			};
			IOptionsMonitor<ClusterSettings> settingsMock = Mock.Of<IOptionsMonitor<ClusterSettings>>(_ => _.CurrentValue == settings);

			JupiterSettings jupiterSettings = new JupiterSettings();
			IOptionsMonitor<JupiterSettings> jupiterSettingsMock = Mock.Of<IOptionsMonitor<JupiterSettings>>(_ => _.CurrentValue == jupiterSettings);

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string endpoint = "/health/live";
			// this emulates response times from calling the different endpoints
			handler.SetupRequest("http://sitea.com/public" + endpoint).ReturnsResponse(HttpStatusCode.OK).Callback(() => Task.Delay(220).Wait()).Verifiable();
			handler.SetupRequest("http://sitea.com/internal" + endpoint).ReturnsResponse(HttpStatusCode.OK).Callback(() => Task.Delay(200).Wait()).Verifiable();
			handler.SetupRequest("http://siteb.com/public" + endpoint).ReturnsResponse(HttpStatusCode.OK).Callback(() => Task.Delay(420).Wait()).Verifiable();
			handler.SetupRequest("http://siteb.com/internal" + endpoint).ReturnsResponse(HttpStatusCode.OK).Callback(() => Task.Delay(400).Wait()).Verifiable();

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			await using PeerStatusService statusService = new(settingsMock, jupiterSettingsMock, httpClientFactory, NullLogger<PeerStatusService>.Instance);

			await statusService.UpdatePeerStatusAsync(CancellationToken.None);

			handler.Verify();

			// as we are actually measuring the time it takes to call our mocked handler the latency expected is not going to be exact so we allow some delta
			PeerStatus? siteAPeerStatus = statusService.GetPeerStatus("siteA");
			Assert.IsNotNull(siteAPeerStatus);
			Assert.AreEqual(220, siteAPeerStatus.Latency, 50);
			Assert.IsTrue(siteAPeerStatus.Reachable);

			PeerStatus? siteBPeerStatus = statusService.GetPeerStatus("siteB");
			Assert.IsNotNull(siteBPeerStatus);
			Assert.AreEqual(450, siteBPeerStatus.Latency, 100);
			Assert.IsTrue(siteBPeerStatus.Reachable);
		}

		[TestMethod]
		public async Task PeerUnreachableAsync()
		{
			ClusterSettings settings = new ClusterSettings
			{
				Peers = new PeerSettings[]
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
				}.ToList()
			};
			IOptionsMonitor<ClusterSettings> settingsMock = Mock.Of<IOptionsMonitor<ClusterSettings>>(_ => _.CurrentValue == settings);

			JupiterSettings jupiterSettings = new JupiterSettings();
			IOptionsMonitor<JupiterSettings> jupiterSettingsMock = Mock.Of<IOptionsMonitor<JupiterSettings>>(_ => _.CurrentValue == jupiterSettings);

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string endpoint = "/health/live";
			handler.SetupRequest("http://sitea.com/public" + endpoint).ReturnsResponse(HttpStatusCode.OK).Callback(() => Task.Delay(220).Wait()).Verifiable();
			handler.SetupRequest("http://sitea.com/internal" + endpoint).Throws<SocketException>().Verifiable();

			// site b is not reachable at all
			handler.SetupRequest("http://siteb.com/public" + endpoint).Throws<SocketException>().Verifiable();
			handler.SetupRequest("http://siteb.com/internal" + endpoint).Throws<SocketException>().Verifiable();

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			await using PeerStatusService statusService = new(settingsMock, jupiterSettingsMock, httpClientFactory, NullLogger<PeerStatusService>.Instance);

			await statusService.UpdatePeerStatusAsync(CancellationToken.None);

			handler.Verify();

			// as we are actually measuring the time it takes to call our mocked handler the latency expected is not going to be exact so we allow some delta
			PeerStatus? siteAPeerStatus = statusService.GetPeerStatus("siteA");
			Assert.IsNotNull(siteAPeerStatus);
			// verify that we ignore the failing internal endpoint
			Assert.AreEqual(220, siteAPeerStatus.Latency, 50);
			Assert.IsTrue(siteAPeerStatus.Reachable);

			PeerStatus? siteBPeerStatus = statusService.GetPeerStatus("siteB");
			Assert.IsNotNull(siteBPeerStatus);
			Assert.AreEqual(int.MaxValue, siteBPeerStatus.Latency);
			Assert.IsFalse(siteBPeerStatus.Reachable);
		}
	}
}
