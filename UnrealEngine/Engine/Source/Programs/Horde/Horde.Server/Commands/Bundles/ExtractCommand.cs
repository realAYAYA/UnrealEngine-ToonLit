// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Storage;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Bundles
{
	[Command("bundle", "extract", "Extracts data from a bundle to the local hard drive")]
	internal class ExtractCommand : Command
	{
		[CommandLine("-Ns=")]
		public NamespaceId NamespaceId { get; set; } = Namespace.Perforce;

		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("ue5-main");

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		readonly IConfiguration _configuration;
		readonly ILoggerProvider _loggerProvider;

		public ExtractCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			StorageService storageService = serviceProvider.GetRequiredService<StorageService>();

			IStorageClient store = await storageService.GetClientAsync(NamespaceId, default);

			CommitNode commit = await store.ReadNodeAsync<CommitNode>(RefName);
			logger.LogInformation("Extracting {Number}: {Description} to {OutputDir}", commit.Number, (commit.Message ?? String.Empty).Replace("\n", "\\n", StringComparison.Ordinal), OutputDir);

			DirectoryNode contents = await commit.Contents.ExpandAsync();
			await contents.CopyToDirectoryAsync(OutputDir.ToDirectoryInfo(), logger, CancellationToken.None);

			return 0;
		}
	}
}
