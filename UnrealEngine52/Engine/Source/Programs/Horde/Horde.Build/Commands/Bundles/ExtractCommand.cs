// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Build.Storage;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Commands.Bundles
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

			TreeReader reader = new TreeReader(store, serviceProvider.GetRequiredService<IMemoryCache>(), serviceProvider.GetRequiredService<ILogger<ExtractCommand>>());
			CommitNode commit = await reader.ReadNodeAsync<CommitNode>(RefName);
			logger.LogInformation("Extracting {Number}: {Description} to {OutputDir}", commit.Number, (commit.Message ?? String.Empty).Replace("\n", "\\n", StringComparison.Ordinal), OutputDir);

			DirectoryNode contents = await commit.Contents.ExpandAsync(reader);
			await contents.CopyToDirectoryAsync(reader, OutputDir.ToDirectoryInfo(), logger, CancellationToken.None);

			return 0;
		}
	}
}
