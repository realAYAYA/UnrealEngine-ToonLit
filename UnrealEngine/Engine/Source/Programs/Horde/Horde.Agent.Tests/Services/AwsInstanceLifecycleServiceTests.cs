// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Services;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Protected;

namespace Horde.Agent.Tests.Services;

/// <summary>
/// Fake implementation of the AWS EC2 metadata server (IMDS)
/// </summary>
public class FakeAwsImds
{
	public const string BaseUrl = "http://169.254.169.254/latest/meta-data";
	public const string SpotInstanceData = "{\"action\": \"terminate\", \"time\": \"2017-09-18T08:22:00Z\"}";
	public const string OnDemand = "on-demand";
	public const string Spot = "spot";

	private readonly Mock<HttpMessageHandler> _mock;
	public string InstanceLifeCycle { get; set; } = OnDemand;
	public string? TargetLifecycleState { get; set; }
	public string? SpotInstanceAction { get; set; }

	public FakeAwsImds()
	{
		_mock = new(MockBehavior.Strict);
		_mock
			.Protected()
			.Setup<Task<HttpResponseMessage>>(
				"SendAsync",
				ItExpr.IsAny<HttpRequestMessage>(),
				ItExpr.IsAny<CancellationToken>()
			)
			.Returns(SendAsync);

		// Ignore Dispose() calls
		_mock.Protected().Setup("Dispose", ItExpr.IsAny<bool>());
	}

	public HttpClient GetHttpClient()
	{
		return new(_mock.Object);
	}

	private Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
	{
		switch (request.RequestUri!.ToString())
		{
			case $"{BaseUrl}/": return Task.FromResult(CreateResponse(HttpStatusCode.OK, "(list of sub-paths)"));
			case $"{BaseUrl}/autoscaling/target-lifecycle-state": return Task.FromResult(CreateResponse(HttpStatusCode.OK, TargetLifecycleState));
			case $"{BaseUrl}/spot/instance-action": return Task.FromResult(CreateResponse(HttpStatusCode.OK, SpotInstanceAction));
			case $"{BaseUrl}/instance-life-cycle": return Task.FromResult(CreateResponse(HttpStatusCode.OK, InstanceLifeCycle));
			default: return Task.FromResult(CreateResponse(HttpStatusCode.InternalServerError, "Invalid state"));
		}
	}

	private static HttpResponseMessage CreateResponse(HttpStatusCode status, string? data)
	{
		return data != null
			? new HttpResponseMessage { StatusCode = status, Content = new StringContent(data) }
			: new HttpResponseMessage { StatusCode = HttpStatusCode.NotFound, Content = new StringContent("Not found") };
	}
}

[TestClass]
public sealed class AwsInstanceLifecycleServiceTests : IAsyncDisposable, IDisposable
{
	private readonly StatusService _statusService;
	private readonly HttpClient _httpClient;
	private readonly FakeAwsImds _fakeImds = new();
	private readonly AwsInstanceLifecycleService _service;
	private readonly FileReference _terminationSignalFile;
	private Ec2TerminationInfo? _info;

	public AwsInstanceLifecycleServiceTests()
	{
		using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
		{
			builder.SetMinimumLevel(LogLevel.Debug);
			builder.AddSimpleConsole(options => { options.SingleLine = true; });
		});

		DirectoryInfo tempDir = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), "horde-agent-test-" + Path.GetRandomFileName()));
		AgentSettings settings = new() { WorkingDir = new DirectoryReference(tempDir) };
		_terminationSignalFile = settings.GetTerminationSignalFile();

		_statusService = new(new TestOptionsMonitor<AgentSettings>(settings), loggerFactory.CreateLogger<StatusService>());
		_httpClient = _fakeImds.GetHttpClient();
		_service = new AwsInstanceLifecycleService(_statusService, _httpClient, new OptionsWrapper<AgentSettings>(settings), loggerFactory.CreateLogger<AwsInstanceLifecycleService>());
		_service._timeToLiveAsg = TimeSpan.FromMilliseconds(10);
		_service._timeToLiveSpot = TimeSpan.FromMilliseconds(20);
		_service._terminationBufferTime = TimeSpan.FromMilliseconds(2);

		AwsInstanceLifecycleService.TerminationWarningDelegate origWarningCallback = _service._terminationWarningCallback;
		_service._terminationWarningCallback = (info, ct) =>
		{
			_info = info;
			Ec2TerminationInfo newInfo = new(_info.State, _info.IsSpot, _info.TimeToLive, DateTime.UnixEpoch + TimeSpan.FromSeconds(2222), _info.Reason);
			return origWarningCallback(newInfo, ct);
		};
		_service._terminationCallback = (info, _) =>
		{
			_info = info;
			return Task.CompletedTask;
		};
	}

	[TestMethod]
	[Timeout(5000)]
	public async Task Terminate_Asg_CallbackHasCorrectParametersAsync()
	{
		_fakeImds.TargetLifecycleState = "Terminated";
		await _service.MonitorInstanceLifecycleAsync(CancellationToken.None);
		Assert.AreEqual(Ec2InstanceState.TerminatingAsg, _info!.State);
		Assert.IsFalse(_info!.IsSpot);
		Assert.AreEqual(8, _info!.TimeToLive.TotalMilliseconds); // 10 ms for ASG, minus 2 ms for termination buffer
	}

	[TestMethod]
	[Timeout(5000)]
	public async Task Terminate_Spot_CallbackHasCorrectParametersAsync()
	{
		_fakeImds.SpotInstanceAction = FakeAwsImds.SpotInstanceData;
		_fakeImds.InstanceLifeCycle = FakeAwsImds.Spot;
		await _service.MonitorInstanceLifecycleAsync(CancellationToken.None);
		Assert.AreEqual(Ec2InstanceState.TerminatingSpot, _info!.State);
		Assert.IsTrue(_info!.IsSpot);
		Assert.AreEqual(18, _info!.TimeToLive.TotalMilliseconds); // 20 ms for spot, minus 2 ms for termination buffer
	}

	[TestMethod]
	[Timeout(5000)]
	public async Task Terminate_Spot_WritesSignalFileAsync()
	{
		_fakeImds.SpotInstanceAction = FakeAwsImds.SpotInstanceData;
		_fakeImds.InstanceLifeCycle = FakeAwsImds.Spot;
		Assert.IsFalse(File.Exists(_terminationSignalFile.FullName));

		await _service.MonitorInstanceLifecycleAsync(CancellationToken.None);

		string data = await File.ReadAllTextAsync(_terminationSignalFile.FullName);
		Assert.AreEqual("v1\n18\n2222000\nAWS EC2 Spot interruption\n", data); // 20 ms for spot, minus 2 ms for termination buffer
	}

	public void Dispose()
	{
		_httpClient.Dispose();
		_service.Dispose();
	}

	public ValueTask DisposeAsync()
	{
		return _statusService.DisposeAsync();
	}
}