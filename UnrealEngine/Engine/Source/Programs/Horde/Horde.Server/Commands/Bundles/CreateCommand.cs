// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Perforce;
using Horde.Server.Storage;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Bundles
{
	[Command("bundle", "create", "Creates a bundle from a folder on the local hard drive")]
	class CreateCommand : Command
	{
		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("default-ref");

		[CommandLine("-Namespace=")]
		public NamespaceId NamespaceId { get; set; } = Namespace.Artifacts;

		[CommandLine("-InputDir=", Required = true)]
		public DirectoryReference InputDir { get; set; } = null!;

		[CommandLine("-OutputDir=")]
		public DirectoryReference? OutputDir { get; set; }

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;

		public CreateCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			IStorageClient storageClient;
			if (OutputDir == null)
			{
				StorageService storageService = serviceProvider.GetRequiredService<StorageService>();
				storageClient = await storageService.GetClientAsync(NamespaceId, CancellationToken.None);
			}
			else
			{
				FileStorageClient fileStorageClient = new FileStorageClient(OutputDir, logger);
				storageClient = fileStorageClient;
			}
		
			await using IStorageWriter writer = storageClient.CreateWriter(RefName);

			DirectoryNode node = new DirectoryNode(DirectoryFlags.None);
			await node.CopyFromDirectoryAsync(InputDir.ToDirectoryInfo(), new ChunkingOptions(), writer, new CopyStatsLogger(logger), CancellationToken.None);

			await storageClient.WriteNodeAsync(RefName, node);
			return 0;
		}
	}
}
