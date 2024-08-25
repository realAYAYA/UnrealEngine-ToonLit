// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Tools;
using Microsoft.Extensions.Logging;

namespace Horde.Commands
{
	[Command("tool", "upload", "Uploads a tool from a local directory")]
	class ToolUpload : Command
	{
		[CommandLine("-Id=")]
		[Description("Identifier for the tool to upload")]
		public ToolId ToolId { get; set; }

		[CommandLine("-Version=")]
		[Description("Optional version number for the new upload")]
		public string? Version { get; set; }

		[CommandLine("-InputDir=")]
		[Description("Directory containing files to upload for the tool")]
		public DirectoryReference InputDir { get; set; } = null!;

		readonly HordeHttpClient _hordeHttpClient;
		readonly HttpStorageClientFactory _httpStorageClientFactory;

		public ToolUpload(HordeHttpClient httpClient, HttpStorageClientFactory httpStorageClientFactory)
		{
			_hordeHttpClient = httpClient;
			_httpStorageClientFactory = httpStorageClientFactory;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using IStorageClient storageClient = _httpStorageClientFactory.CreateClientWithPath($"api/v1/tools/{ToolId}");

			IBlobRef<DirectoryNode> target;
			await using (IBlobWriter writer = storageClient.CreateBlobWriter())
			{
				target = await writer.WriteFilesAsync(InputDir);
			}

			ToolDeploymentId deploymentId = await _hordeHttpClient.CreateToolDeploymentAsync(ToolId, Version, null, null, target.GetRefValue());
			logger.LogInformation("Created deployment {DeploymentId}", deploymentId);

			return 0;
		}
	}
}
