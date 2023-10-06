// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Tracks the current status of the agent
	/// </summary>
	class StatusService : IHostedService, IAsyncDisposable
	{
		const int NumPipes = 10;

		private AgentStatusMessage _current;

		readonly BackgroundTask _task;
		readonly ILogger _logger;

		/// <summary>
		/// The current agent status
		/// </summary>
		public AgentStatusMessage Current => _current;

		/// <summary>
		/// Constructor
		/// </summary>
		public StatusService(ILogger<StatusService> logger)
		{
			_current = AgentStatusMessage.Starting;
			_task = new BackgroundTask(RunPipeServer);
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _task.DisposeAsync();
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			_task.Start();
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _task.StopAsync();
		}

		/// <summary>
		/// Sets the current status
		/// </summary>
		public void Set(AgentStatusMessage status)
		{
			Interlocked.Exchange(ref _current, status);
			_logger.LogDebug("Updating status: {@Status}", status);
		}

		/// <summary>
		/// Sets the current status
		/// </summary>
		public void Set(bool healthy, int numLeases, string message) => Set(new AgentStatusMessage(healthy, numLeases, message));

		/// <summary>
		/// Sets a status description
		/// </summary>
		public void SetDescription(string description) => Update(status => new AgentStatusMessage(status.Healthy, status.NumLeases, description));

		/// <summary>
		/// Updates the status using a custom funciton
		/// </summary>
		/// <param name="updateFunc">Function to take the existing status and create an updated version</param>
		public void Update(Func<AgentStatusMessage, AgentStatusMessage> updateFunc)
		{
			for (; ; )
			{
				AgentStatusMessage info = _current;
				if (Interlocked.CompareExchange(ref _current, updateFunc(info), info) == info)
				{
					break;
				}
			}
		}

		private async Task RunPipeServer(CancellationToken cancellationToken)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				List<Task> tasks = new List<Task>();
				for (int idx = 0; idx < NumPipes; idx++)
				{
					tasks.Add(RunSinglePipeServer(cancellationToken));
				}
				await Task.WhenAll(tasks);
			}
		}

		[SupportedOSPlatform("windows")]
		private async Task RunSinglePipeServer(CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				try
				{
					await RunPipeServerInternalAsync(cancellationToken);
				}
				catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
				{
					break;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while running pipe server: {Message}", ex.Message);
				}

				await Task.Delay(TimeSpan.FromSeconds(30), cancellationToken);
			}
		}

		[SupportedOSPlatform("windows")]
		private async Task RunPipeServerInternalAsync(CancellationToken cancellationToken)
		{
			AgentMessageBuffer request = new AgentMessageBuffer();
			AgentMessageBuffer response = new AgentMessageBuffer();

			_logger.LogDebug("Creating pipe for status updates");

			PipeSecurity pipeSecurity = new PipeSecurity();
			pipeSecurity.AddAccessRule(new PipeAccessRule(WindowsIdentity.GetCurrent().Name, PipeAccessRights.FullControl, AccessControlType.Allow));
			pipeSecurity.AddAccessRule(new PipeAccessRule("Users", PipeAccessRights.ReadWrite, AccessControlType.Allow));

			using (NamedPipeServerStream pipeServer = NamedPipeServerStreamAcl.Create(AgentMessagePipe.PipeName, PipeDirection.InOut, NumPipes, PipeTransmissionMode.Byte, PipeOptions.Asynchronous, 0, 0, pipeSecurity))
			{
				await pipeServer.WaitForConnectionAsync(cancellationToken);
				_logger.LogDebug("Received pipe connection");

				while (await request.TryReadAsync(pipeServer, cancellationToken))
				{
					switch (request.Type)
					{
						case AgentMessageType.GetStatusRequest:
							response.Set(AgentMessageType.GetStatusResponse, Current);
							break;
						default:
							response.Set(AgentMessageType.InvalidResponse);
							break;
					}
					await response.SendAsync(pipeServer, cancellationToken);
				}
			}
		}
	}
}
