// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Leases;
using Horde.Agent.Utility;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Implements the message handling loop for an agent. Runs asynchronously until disposed.
	/// </summary>
	class WorkerService : BackgroundService, IDisposable
	{
		readonly ILogger _logger;
		readonly IOptions<AgentSettings> _settings;
		readonly SessionFactoryService _sessionFactoryService;
		readonly CapabilitiesService _capabilitiesService;
		readonly LeaseHandler[] _leaseHandlers;
		readonly IServiceProvider _serviceProvider;

		/// <summary>
		/// Constructor. Registers with the server and starts accepting connections.
		/// </summary>
		public WorkerService(IOptions<AgentSettings> settings, SessionFactoryService sessionFactoryService, CapabilitiesService capabilitiesService, IEnumerable<LeaseHandler> leaseHandlers, IServiceProvider serviceProvider, ILogger<WorkerService> logger)
		{
			_settings = settings;
			_sessionFactoryService = sessionFactoryService;
			_capabilitiesService = capabilitiesService;
			_logger = logger;
			_leaseHandlers = leaseHandlers.ToArray();
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
			string version = Program.Version;
			_logger.LogInformation("Version: {Version}", version);

			// Print the server info
			_logger.LogInformation("Arguments: {Arguments}", Environment.CommandLine);

			// Keep trying to start an agent session with the server
			while (!stoppingToken.IsCancellationRequested)
			{
				SessionResult result = SessionResult.Continue;

				Stopwatch sessionTime = Stopwatch.StartNew();
				await using (AsyncServiceScope scope = _serviceProvider.CreateAsyncScope())
				{
					try
					{
						using Mutex singleInstanceMutex = new(false, "Global\\HordeAgent-DB828ACB-0AA5-4D32-A62A-21D4429B1014");
						await WaitForMutexAsync(singleInstanceMutex, stoppingToken);

						await using (ISession session = await _sessionFactoryService.CreateAsync(stoppingToken))
						{
							LeaseManager leaseManager = new LeaseManager(session, _capabilitiesService, _leaseHandlers, _settings, _logger);
							result = await leaseManager.RunAsync(false, stoppingToken);
						}
					}
					catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
					{
						throw;
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Exception while executing session. Restarting. ({Message})", ex.Message);
					}
				}

				if (result == SessionResult.Terminate)
				{
					break;
				}
				else if (result == SessionResult.Shutdown || result == SessionResult.Restart)
				{
					bool restart = (result == SessionResult.Restart);
					_logger.LogInformation("Initiating shutdown (restart={Restart})", restart);

					if (Shutdown.InitiateShutdown(restart, _logger))
					{
						for (int idx = 10; idx > 0; idx--)
						{
							_logger.LogInformation("Waiting for shutdown ({Count})", idx);
							try
							{
								await Task.Delay(TimeSpan.FromSeconds(60.0), stoppingToken);
								_logger.LogInformation("Shutdown aborted.");
							}
							catch (OperationCanceledException)
							{
								_logger.LogInformation("Agent is shutting down.");
								return;
							}
						}
					}
				}
				
				if (sessionTime.Elapsed < TimeSpan.FromSeconds(2.0))
				{
					_logger.LogInformation("Waiting 5 seconds before restarting session...");
					await Task.Delay(TimeSpan.FromSeconds(5.0), stoppingToken);
				}
			}
		}

		async Task WaitForMutexAsync(Mutex mutex, CancellationToken stoppingToken)
		{
			try
			{
				if (!mutex.WaitOne(0))
				{
					_logger.LogError("Another instance of HordeAgent is already running. Waiting until it terminates.");
					while (!mutex.WaitOne(0))
					{
						stoppingToken.ThrowIfCancellationRequested();
						await Task.Delay(TimeSpan.FromSeconds(1.0), stoppingToken);
					}
				}
			}
			catch (AbandonedMutexException)
			{
			}
		}
	}
}
