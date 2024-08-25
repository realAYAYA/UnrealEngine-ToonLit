// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using Horde.Agent.Leases;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Implements the message handling loop for an agent. Runs asynchronously until disposed.
	/// </summary>
	class WorkerService : BackgroundService, IDisposable
	{
		readonly ILogger _logger;
		readonly ISessionFactory _sessionFactory;
		readonly CapabilitiesService _capabilitiesService;
		readonly StatusService _statusService;
		readonly LeaseHandler[] _leaseHandlers;
		readonly LeaseLoggerFactory _leaseLoggerFactory;
		readonly IServiceProvider _serviceProvider;
		LeaseManager? _currentLeaseManager;

		public IReadOnlyList<string> PoolIds => _currentLeaseManager?.PoolIds ?? Array.Empty<string>();

		static readonly TimeSpan[] s_sessionBackOffTime =
		{
			TimeSpan.FromSeconds(5),
			TimeSpan.FromSeconds(10),
			TimeSpan.FromSeconds(30),
			TimeSpan.FromMinutes(1)
		};

		/// <summary>
		/// Constructor. Registers with the server and starts accepting connections.
		/// </summary>
		public WorkerService(ISessionFactory sessionFactory, CapabilitiesService capabilitiesService, StatusService statusService, IEnumerable<LeaseHandler> leaseHandlers, LeaseLoggerFactory leaseLoggerFactory, IServiceProvider serviceProvider, ILogger<WorkerService> logger)
		{
			_sessionFactory = sessionFactory;
			_capabilitiesService = capabilitiesService;
			_statusService = statusService;
			_logger = logger;
			_leaseHandlers = leaseHandlers.ToArray();
			_leaseLoggerFactory = leaseLoggerFactory;
			_serviceProvider = serviceProvider;
		}

		/// <summary>
		/// Executes the ServerTaskAsync method and swallows the exception for the task being cancelled. This allows waiting for it to terminate.
		/// </summary>
		/// <param name="stoppingToken">Indicates that the service is trying to stop</param>
		protected override async Task ExecuteAsync(CancellationToken stoppingToken)
		{
			try
			{
				await ExecuteInnerAsync(stoppingToken);
			}
			catch (Exception ex)
			{
				_logger.LogCritical(ex, "Unhandled exception");
			}
		}

		/// <summary>
		/// Background task to cycle access tokens and update the state of the agent with the server.
		/// </summary>
		/// <param name="stoppingToken">Indicates that the service is trying to stop</param>
		internal async Task ExecuteInnerAsync(CancellationToken stoppingToken)
		{
			// Show the current client id
			string version = AgentApp.Version;
			_logger.LogInformation("Version: {Version}", version);

			// Print the server info
			_logger.LogInformation("Arguments: {Arguments}", Environment.CommandLine);

			// await TelemetryService.LogProblematicFilterDriversAsync(_logger, stoppingToken);

			// Keep trying to start an agent session with the server
			int failureCount = 0;
			while (!stoppingToken.IsCancellationRequested)
			{
				SessionResult? result = null;
				_statusService.Set(AgentStatusMessage.Starting);

				Stopwatch sessionTime = Stopwatch.StartNew();

				await using (AsyncServiceScope scope = _serviceProvider.CreateAsyncScope())
				{
					try
					{
						Task<IDisposable> mutexTask = SingleInstanceMutex.AcquireAsync("Global\\HordeAgent-DB828ACB-0AA5-4D32-A62A-21D4429B1014", stoppingToken);

						Task delayTask = Task.Delay(TimeSpan.FromSeconds(1.0), stoppingToken);
						if (Task.WhenAny(mutexTask, delayTask) == delayTask)
						{
							_logger.LogInformation("Another agent instance is already running. Waiting for it to terminate.");
						}

						using IDisposable mutex = await mutexTask;

						await using (ISession session = await _sessionFactory.CreateAsync(stoppingToken))
						{
							_currentLeaseManager = new LeaseManager(session, _capabilitiesService, _statusService, _leaseHandlers, _leaseLoggerFactory, _logger);
							result = await _currentLeaseManager.RunAsync(false, stoppingToken);
						}

						failureCount = 0;
					}
					catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
					{
						throw;
					}
					catch (Exception ex)
					{
						if (sessionTime.Elapsed < TimeSpan.FromMinutes(5.0))
						{
							failureCount++;
						}
						else
						{
							failureCount = 1;
						}

						TimeSpan backOffTime = s_sessionBackOffTime[Math.Min(failureCount - 1, s_sessionBackOffTime.Length - 1)];
						_logger.LogInformation("Session failure #{FailureNum}. Waiting {Time} and restarting. ({Message})", failureCount, backOffTime, ex.Message);
						_statusService.Set(false, 0, $"Unable to start session: {ex.Message}");
						await Task.Delay(backOffTime, stoppingToken);
					}
				}

				if (result != null)
				{
					if (result.Outcome == SessionOutcome.BackOff)
					{
						await Task.Delay(TimeSpan.FromSeconds(30.0), stoppingToken);
					}
					else if (result.Outcome == SessionOutcome.Terminate)
					{
						break;
					}
					else if (result.Outcome == SessionOutcome.RunCallback)
					{
						await result.CallbackAsync!(_logger, stoppingToken);
					}
				}

				if (sessionTime.Elapsed < TimeSpan.FromSeconds(2.0))
				{
					_logger.LogInformation("Waiting 5 seconds before restarting session...");
					await Task.Delay(TimeSpan.FromSeconds(5.0), stoppingToken);
				}
			}
		}
	}
}
