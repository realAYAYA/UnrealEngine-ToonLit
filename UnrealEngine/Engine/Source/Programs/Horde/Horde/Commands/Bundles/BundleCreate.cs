// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Bundles
{
	[Command("bundle", "create", "Creates a bundle from a folder on the local hard drive")]
	class BundleCreate : StorageCommandBase
	{
		[CommandLine("-File=", Description = "Output file for the bundle ref")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		public string? Ref { get; set; }

		[CommandLine("-Input=", Required = true, Description = "Input file or directory")]
		public string Input { get; set; } = null!;

		[CommandLine("-Filter=", Description = "Filter for files to include, in P4 syntax (eg. Foo/...).")]
		public string Filter { get; set; } = "...";

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			// Create the storage client
			IStorageClient store;
			if (File != null)
			{
				store = new FileStorageClient(File.Directory, logger);
			}
			else if (!String.IsNullOrEmpty(Ref))
			{
				store = await CreateStorageClientAsync(logger);
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified.");
			}

			// Gather the input files
			DirectoryReference baseDir;
			List<FileReference> files = new List<FileReference>();

			if (System.IO.File.Exists(Input))
			{
				FileReference file = new FileReference(Input);
				baseDir = file.Directory;
				files.Add(file);
			}
			else if (Directory.Exists(Input))
			{
				baseDir = new DirectoryReference(Input);
				FileFilter filter = new FileFilter(Filter.Split(';'));
				files.AddRange(filter.ApplyToDirectory(baseDir, true));
			}
			else
			{
				logger.LogError("{Path} does not exist", Input);
				return 1;
			}

			// Create the bundle
			Stopwatch timer = Stopwatch.StartNew();

			await using (IStorageWriter writer = store.CreateWriter())
			{
				ChunkingOptions options = new ChunkingOptions();

				DirectoryNode node = await DirectoryNode.CreateAsync(baseDir, files.ConvertAll(x => x.ToFileInfo()), options, writer, null, CancellationToken.None);
				NodeRef<DirectoryNode> nodeRef = await writer.WriteNodeAsync(node, CancellationToken.None);

				await writer.FlushAsync();

				if (File != null)
				{
					logger.LogInformation("Writing {File}", File);
					await FileReference.WriteAllTextAsync(File, nodeRef.Handle.ToString());
				}
				else
				{
					logger.LogInformation("Writing ref {Ref}", Ref);
					await store.WriteRefTargetAsync(new RefName(Ref!), nodeRef.Handle);
				}
			}

			logger.LogInformation("Time: {Time}", timer.Elapsed.TotalSeconds);
			return 0;
		}
	}
}
