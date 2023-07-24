// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Bundles
{
	[Command("bundle", "create", "Creates a bundle from a folder on the local hard drive")]
	class BundleCreate : Command
	{
		[CommandLine("-Namespace=", Description = "Namespace to use for storage")]
		public NamespaceId NamespaceId { get; set; } = new NamespaceId("default");

		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("default-bundle");

		[CommandLine("-InputDir=", Required = true)]
		public DirectoryReference InputDir { get; set; } = null!;

		readonly IStorageClientFactory _storageClientFactory;

		public BundleCreate(IStorageClientFactory storageClientFactory)
		{
			_storageClientFactory = storageClientFactory;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			IStorageClient store = await _storageClientFactory.GetClientAsync(NamespaceId);

			using TreeWriter writer = new TreeWriter(store, prefix: RefName.Text);

			DirectoryNode node = new DirectoryNode(DirectoryFlags.None);

			Stopwatch timer = Stopwatch.StartNew();

			ChunkingOptions options = new ChunkingOptions();
			await node.CopyFromDirectoryAsync(InputDir.ToDirectoryInfo(), options, writer, CancellationToken.None);

			await writer.WriteAsync(RefName, node);

			logger.LogInformation("Time: {Time}", timer.Elapsed.TotalSeconds);
			return 0;
		}
	}
}
