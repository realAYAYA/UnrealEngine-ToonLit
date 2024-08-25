// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Perforce;

/// <summary>
/// Fake implementation of IHealthMonitor for tests
/// </summary>
/// <typeparam name="T"></typeparam>
public class FakeHealthMonitor<T> : IHealthMonitor<T>
{
	/// <summary>Name set by SetName()</summary>
	public string Name { get; set; } = "<not-set>";

	/// <summary>Updates sent</summary>
	public List<(HealthStatus result, string? message, DateTimeOffset? timestamp)> Updates { get; } = [];

	/// <inheritdoc/>
	public void SetName(string name) { Name = name; }

	/// <inheritdoc/>
	public Task UpdateAsync(HealthStatus result, string? message = null, DateTimeOffset? timestamp = null)
	{
		Updates.Add((result, message, timestamp));
		return Task.CompletedTask;
	}
}

[TestClass]
public class PerforceLoadBalancerTests : TestSetup
{
	private const string PerforceHostname = "";

	private readonly ILogger<PerforceLoadBalancer> _logger = CreateConsoleLogger<PerforceLoadBalancer>();

	[TestMethod]
	public async Task StatusHealthyViaInfoAsync()
	{
		CheckPerforceServerIsSet();
		GlobalConfig gc = GetConfig(PerforceHostname);
		PerforceLoadBalancer plb = Create(gc, "");
		await plb.UpdateHealthTestOnlyAsync("myCluster", PerforceHostname, null, CancellationToken.None);
		IPerforceServer server = (await plb.GetServersAsync(CancellationToken.None)).First();
		Assert.AreEqual(PerforceServerStatus.Healthy, server.Status);
	}

	[TestMethod]
	public async Task StatusHealthyViaHttpAsync()
	{
		CheckPerforceServerIsSet();
		GlobalConfig gc = GetConfig(PerforceHostname);
		PerforceLoadBalancer plb = Create(gc, GetHealthCheckJsonForStatus(PerforceServerStatus.Healthy));
		await plb.UpdateHealthTestOnlyAsync("myCluster", PerforceHostname, "http://stub-handler-is-used", CancellationToken.None);
		IPerforceServer server = (await plb.GetServersAsync(CancellationToken.None)).First();
		Assert.AreEqual(PerforceServerStatus.Healthy, server.Status);
	}

	[TestMethod]
	public async Task StatusUnhealthyViaHttpAsync()
	{
		CheckPerforceServerIsSet();
		GlobalConfig gc = GetConfig(PerforceHostname);
		PerforceLoadBalancer plb = Create(gc, GetHealthCheckJsonForStatus(PerforceServerStatus.Unhealthy));
		await plb.UpdateHealthTestOnlyAsync("myCluster", PerforceHostname, "http://stub-handler-is-used", CancellationToken.None);
		IPerforceServer server = (await plb.GetServersAsync(CancellationToken.None)).First();
		Assert.AreEqual(PerforceServerStatus.Unhealthy, server.Status);
	}

	private static GlobalConfig GetConfig(string perforceHost)
	{
		PerforceCluster perforceCluster = new() { Servers = [new PerforceServer { ServerAndPort = perforceHost }] };
		GlobalConfig gc = new() { PerforceClusters = [perforceCluster] };
		return gc;
	}

	private static string GetHealthCheckJsonForStatus(PerforceServerStatus status)
	{
		return status switch
		{
			PerforceServerStatus.Healthy => GetHealthCheckJson("green"),
			PerforceServerStatus.Degraded => GetHealthCheckJson("yellow"),
			PerforceServerStatus.Unhealthy => GetHealthCheckJson("red"),
			_ => GetHealthCheckJson("this-is-not-valid"),
		};
	}

	private static string GetHealthCheckJson(string output)
	{
		string result = """{"checker": "edge_traffic_lights", "output": "%OUTPUT%"}""".Replace("%OUTPUT%", output, StringComparison.Ordinal);
		return """{"results": [%RESULT%]}""".Replace("%RESULT%", result, StringComparison.Ordinal);
	}

	private static void CheckPerforceServerIsSet()
	{
		if (String.IsNullOrEmpty(PerforceHostname))
		{
			Assert.Inconclusive($"Set {nameof(PerforceHostname)} to a valid Perforce server to run load balancer tests");
		}
	}

	private PerforceLoadBalancer Create(GlobalConfig gc, string httpCheckResponse)
	{
#pragma warning disable CA2000 // Dispose objects before losing scope
		HttpClient httpClient = new(new StubMessageHandler(HttpStatusCode.OK, httpCheckResponse));
		return new(MongoService, GetRedisServiceSingleton(), LeaseCollection, Clock, httpClient, new TestOptionsMonitor<GlobalConfig>(gc), new FakeHealthMonitor<PerforceLoadBalancer>(), Tracer, _logger);
#pragma warning restore CA2000 // Dispose objects before losing scope		
	}

	private class StubMessageHandler(HttpStatusCode statusCode, string content) : HttpMessageHandler
	{
		public string Content { get; set; } = content;
		protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			return Task.FromResult(new HttpResponseMessage(statusCode) { Content = new StringContent(Content) });
		}
	}
}

