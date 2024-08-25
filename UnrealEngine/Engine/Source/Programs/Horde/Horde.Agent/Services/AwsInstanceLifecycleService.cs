// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using System.Text;
using EpicGames.Core;
using Horde.Agent.Utility;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Services;

/// <summary>
/// Describes state of local EC2 instance the agent is running on
/// </summary>
public enum Ec2InstanceState
{
	/// <summary>
	/// Normal state
	/// </summary>
	InService,

	/// <summary>
	/// Termination is caused by a spot interruption.
	/// </summary>
	TerminatingSpot,

	/// <summary>
	/// Termination is caused by the auto-scaling group
	/// Can be caused by capacity re-balancing, scale-ins etc.
	/// </summary>
	TerminatingAsg
}

internal class Ec2TerminationInfo
{
	public Ec2InstanceState State { get; init; }
	public bool IsSpot { get; init; }
	public TimeSpan TimeToLive { get; init; }
	public DateTime TerminateAt { get; init; }
	public string Reason { get; init; }

	public Ec2TerminationInfo(Ec2InstanceState state, bool isSpot, TimeSpan timeToLive, DateTime terminateAt, string reason)
	{
		State = state;
		IsSpot = isSpot;
		TimeToLive = timeToLive;
		TerminateAt = terminateAt;
		Reason = reason;
	}
}

/// <summary>
/// Monitors the local EC2 instance lifecycle state. In particular, auto-scaling group and spot instance events.
/// </summary>
class AwsInstanceLifecycleService : BackgroundService
{
	/// <summary>
	/// Name of the HTTP client used for requests to IMDS
	/// </summary>
	public const string HttpClientName = "Horde.HttpAwsInstanceClient";

	private const string BaseUri = "http://169.254.169.254/latest/meta-data";
	private readonly StatusService _statusService;
	private readonly HttpClient _httpClient;
	private readonly FileReference _terminationSignalFile;
	private readonly ILogger<AwsInstanceLifecycleService> _logger;
	private readonly TimeSpan _pollInterval = TimeSpan.FromSeconds(5);

	internal delegate Task TerminationWarningDelegate(Ec2TerminationInfo info, CancellationToken cancellationToken);
	internal delegate Task TerminationDelegate(Ec2TerminationInfo info, CancellationToken cancellationToken);
	internal TerminationWarningDelegate _terminationWarningCallback;
	internal TerminationDelegate _terminationCallback;

	/// <summary>
	/// Time to live for EC2 instance once a termination is detected coming from the auto-scaling group (ASG)
	/// In practice, this is dictated by the lifecycle hook set for the ASG.
	/// Set to 120 sec to mimic the TTL for spot interruption, leading to similar handling of both for now.
	/// </summary>
	internal TimeSpan _timeToLiveAsg = TimeSpan.FromSeconds(120);

	/// <summary>
	/// Time to live for EC2 instance once a spot interruption is detected. Strictly defined by AWS EC2.
	/// </summary>
	internal TimeSpan _timeToLiveSpot = TimeSpan.FromSeconds(120); // Strictly defined by AWS EC2

	/// <summary>
	/// Duration of the time-to-live to allocate towards shutting down the Horde agent and the machine itself.
	/// Example: if TTL is 120 seconds, 90 seconds will be reported in the termination warning.
	/// </summary>
	internal TimeSpan _terminationBufferTime = TimeSpan.FromSeconds(30);

	/// <summary>
	/// Constructor
	/// </summary>
	public AwsInstanceLifecycleService(StatusService statusService, HttpClient httpClient, IOptions<AgentSettings> settings, ILogger<AwsInstanceLifecycleService> logger)
	{
		_statusService = statusService;
		_httpClient = httpClient;
		_httpClient.Timeout = TimeSpan.FromSeconds(10);
		_terminationWarningCallback = OnTerminationWarningAsync;
		_terminationCallback = OnTerminationAsync;
		_logger = logger;
		_terminationSignalFile = settings.Value.GetTerminationSignalFile();
	}

	private async Task<Ec2InstanceState> GetStateAsync(CancellationToken cancellationToken)
	{
		HttpResponseMessage spotRes = await _httpClient.GetAsync(new Uri(BaseUri + "/spot/instance-action"), cancellationToken);
		if (spotRes.StatusCode == HttpStatusCode.OK)
		{
			return Ec2InstanceState.TerminatingSpot;
		}

		HttpResponseMessage asgRes = await _httpClient.GetAsync(new Uri(BaseUri + "/autoscaling/target-lifecycle-state"), cancellationToken);
		if (asgRes.StatusCode == HttpStatusCode.OK)
		{
			string state = await asgRes.Content.ReadAsStringAsync(cancellationToken);
			if (state == "Terminated")
			{
				return Ec2InstanceState.TerminatingAsg;
			}
		}

		return Ec2InstanceState.InService;
	}

	private async Task<bool> IsSpotInstanceAsync(CancellationToken cancellationToken)
	{
		HttpResponseMessage res = await _httpClient.GetAsync(new Uri(BaseUri + "/instance-life-cycle"), cancellationToken);
		if (res.StatusCode == HttpStatusCode.OK)
		{
			string state = await res.Content.ReadAsStringAsync(cancellationToken);
			return state == "spot";
		}

		return false;
	}

	private async Task<bool> IsImdsAvailableAsync(CancellationToken cancellationToken)
	{
		try
		{
			HttpResponseMessage res = await _httpClient.GetAsync(new Uri(BaseUri + "/"), cancellationToken);
			return res.StatusCode == HttpStatusCode.OK;
		}
		catch (Exception)
		{
			// Timed out or other error. Can safely assume the metadata server is not available.
			return false;
		}
	}

	internal async Task MonitorInstanceLifecycleAsync(CancellationToken cancellationToken)
	{
		if (!await IsImdsAvailableAsync(cancellationToken))
		{
			_logger.LogInformation("EC2 metadata server (IMDS) not available. Will not monitor EC2 lifecycle state");
			return;
		}

		_logger.LogInformation("Monitoring EC2 instance lifecycle state...");
		while (!cancellationToken.IsCancellationRequested)
		{
			try
			{
				Ec2InstanceState state = await GetStateAsync(cancellationToken);
				if (state != Ec2InstanceState.InService)
				{
					bool isSpot = await IsSpotInstanceAsync(cancellationToken);
					TimeSpan ttl = GetTimeToLive(state);
					_logger.LogInformation("EC2 instance is terminating. IsSpot={IsSpot} Reason={InstanceState} TimeToLive={Ttk} ms", isSpot, state, ttl.TotalMilliseconds);

					ttl -= _terminationBufferTime;
					ttl = ttl.Ticks >= 0 ? ttl : TimeSpan.Zero;
					DateTime terminateAt = DateTime.UtcNow + ttl;
					Ec2TerminationInfo info = new(state, isSpot, ttl, terminateAt, GetReason(state));

					await _terminationWarningCallback(info, cancellationToken);
					await Task.Delay(ttl, cancellationToken);
					await _terminationCallback(info, cancellationToken);
					return;
				}
			}
			catch (TaskCanceledException) when (!cancellationToken.IsCancellationRequested)
			{
				// Ignore HTTP timeouts from the IMDS server and let loop try again
				// Unclear why timeouts are seen in the first place from the metadata server.
				// It should not be under load at all but it's possible an agent performing heavy work can affect this.
			}
			catch (Exception e)
			{
				_logger.LogError(e, "Unhandled exception during EC2 instance monitoring");
			}

			await Task.Delay(_pollInterval, cancellationToken);
		}
	}

	/// <summary>
	/// Determine time to live for the current EC2 instance once a terminating state has been detected
	/// </summary>
	/// <param name="state">Current state</param>
	/// <returns>Time to live</returns>
	/// <exception cref="ArgumentException"></exception>
	private TimeSpan GetTimeToLive(Ec2InstanceState state)
	{
		return state switch
		{
			Ec2InstanceState.TerminatingAsg => _timeToLiveAsg,
			Ec2InstanceState.TerminatingSpot => _timeToLiveSpot,
			_ => throw new ArgumentException($"Invalid state {state}")
		};
	}

	private static string GetReason(Ec2InstanceState state)
	{
		return state switch
		{
			Ec2InstanceState.TerminatingAsg => "AWS EC2 ASG termination",
			Ec2InstanceState.TerminatingSpot => "AWS EC2 Spot interruption",
			_ => throw new ArgumentException($"Invalid state {state}")
		};
	}

	private async Task OnTerminationWarningAsync(Ec2TerminationInfo info, CancellationToken cancellationToken)
	{
		// Signal to server we are disabled, setting state to paused preventing new leases getting scheduled
		_statusService.IsBusy = true;

		// Create and write the termination signal file, containing the time-to-live for the EC2 instance.
		// Workloads executed by the agent that support this protocol can pick this up and prepare/clean up prior to termination
		await WriteTerminationSignalFileAsync(info, cancellationToken);
	}

	private Task OnTerminationAsync(Ec2TerminationInfo info, CancellationToken cancellationToken)
	{
		if (info.IsSpot)
		{
			_logger.LogInformation("Shutting down");
			return Shutdown.ExecuteAsync(false, _logger, cancellationToken);
		}

		return Task.CompletedTask;
	}

	private Task WriteTerminationSignalFileAsync(Ec2TerminationInfo info, CancellationToken cancellationToken)
	{
		StringBuilder sb = new(100);
		sb.Append("v1\n");
		sb.Append($"{info.TimeToLive.TotalMilliseconds}\n");
		sb.Append($"{new DateTimeOffset(info.TerminateAt).ToUnixTimeMilliseconds()}\n");
		sb.Append($"{info.Reason}\n");
		return File.WriteAllTextAsync(_terminationSignalFile.FullName, sb.ToString(), cancellationToken);
	}

	/// <inheritdoc/>
	protected override async Task ExecuteAsync(CancellationToken stoppingToken)
	{
		await MonitorInstanceLifecycleAsync(stoppingToken);
	}
}

