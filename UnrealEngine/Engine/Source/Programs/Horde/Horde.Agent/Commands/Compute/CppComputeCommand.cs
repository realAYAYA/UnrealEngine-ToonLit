// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Horde.Agent.Parser;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Compute
{
	/// <summary>
	/// Installs the agent as a service
	/// </summary>
	[Command("cppcompute", "Executes a command through the C++ Compute API")]
	class CppComputeCommand : ComputeCommand
	{
		class JsonComputeTask
		{
			public string Executable { get; set; } = null!;
			public List<string> Arguments { get; set; } = new List<string>();
			public string WorkingDir { get; set; } = String.Empty;
			public Dictionary<string, string?> EnvVars { get; set; } = new Dictionary<string, string?>();
			public List<string> OutputPaths { get; set; } = new List<string>();
		}

		[CommandLine("-Task=", Required = true)]
		FileReference TaskFile { get; set; } = null!;

		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public CppComputeCommand(IServiceProvider serviceProvider, ILogger<CppComputeCommand> logger)
			: base(serviceProvider)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		protected override async Task<bool> HandleRequestAsync(IComputeLease lease, CancellationToken cancellationToken)
		{
			const int ControlChannelId = 0;

			// Read the task definition
			byte[] data = await FileReference.ReadAllBytesAsync(TaskFile, cancellationToken);
			JsonComputeTask jsonComputeTask = JsonSerializer.Deserialize<JsonComputeTask>(data, new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase })!;

			// Create a sandbox from the data to be uploaded
			MemoryStorageClient storage = new MemoryStorageClient();
			NodeLocator sandbox = await CreateSandboxAsync(TaskFile, storage, cancellationToken);

			// Open a socket and upload the sandbox
			using (AgentMessageChannel channel = lease.Socket.CreateAgentMessageChannel(ControlChannelId, 4 * 1024 * 1024, _logger))
			{
				await channel.UploadFilesAsync("", sandbox, storage, cancellationToken);

				await using (AgentManagedProcess process = await channel.ExecuteAsync(jsonComputeTask.Executable, jsonComputeTask.Arguments, jsonComputeTask.WorkingDir, jsonComputeTask.EnvVars, cancellationToken))
				{
					string? line;
					while ((line = await process.ReadLineAsync(cancellationToken)) != null)
					{
						_logger.LogInformation("Child Process: {Text}", line);
					}
				}
			}

			return true;
		}

		static async Task<NodeLocator> CreateSandboxAsync(FileReference taskFile, IStorageClient storage, CancellationToken cancellationToken)
		{
			await using IStorageWriter writer = storage.CreateWriter();

			DirectoryNode sandbox = new DirectoryNode();
			await sandbox.CopyFromDirectoryAsync(taskFile.Directory.ToDirectoryInfo(), new ChunkingOptions(), writer, null, cancellationToken);

			BlobHandle handle = await writer.FlushAsync(sandbox, cancellationToken);
			return handle.GetLocator();
		}
	}
}
