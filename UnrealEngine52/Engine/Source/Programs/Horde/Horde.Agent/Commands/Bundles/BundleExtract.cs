// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Bundles
{
	[Command("bundle", "extract", "Extracts data from a bundle to the local hard drive")]
	internal class BundleExtract : Command
	{
		[CommandLine("-Namespace=", Description = "Namespace to use for storage")]
		public NamespaceId NamespaceId { get; set; } = new NamespaceId("default");

		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("default-bundle");

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		readonly IStorageClientFactory _storageClientFactory;

		public BundleExtract(IStorageClientFactory storageClientFactory)
		{
			_storageClientFactory = storageClientFactory;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			IStorageClient store = await _storageClientFactory.GetClientAsync(NamespaceId);

			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			TreeReader reader = new TreeReader(store, cache, logger);

			Stopwatch timer = Stopwatch.StartNew();

			DirectoryNode node = await reader.ReadNodeAsync<DirectoryNode>(RefName);
			await node.CopyToDirectoryAsync(reader, OutputDir.ToDirectoryInfo(), logger, CancellationToken.None);

			logger.LogInformation("Elapsed: {Time}s", timer.Elapsed.TotalSeconds);
			return 0;
		}
	}
}
