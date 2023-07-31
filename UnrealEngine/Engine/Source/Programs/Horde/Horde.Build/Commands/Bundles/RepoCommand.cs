// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Git;
using EpicGames.Horde.Storage.Nodes;
using Horde.Build.Perforce;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Commands.Bundles
{
	[Command("bundle", "repo", "Synthesizes a Git repo from a bundle to the local hard drive")]
	class RepoCommand : Command
	{
		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = new RefName("default-ref");

		[CommandLine("-OutputDir=", Required = true)]
		public DirectoryReference OutputDir { get; set; } = null!;

		protected readonly IConfiguration _configuration;
		protected readonly ILoggerProvider _loggerProvider;

		public RepoCommand(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			_configuration = configuration;
			_loggerProvider = loggerProvider;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using ServiceProvider serviceProvider = Startup.CreateServiceProvider(_configuration, _loggerProvider);

			ITreeStore store = serviceProvider.GetRequiredService<ITreeStore<ReplicationService>>();

			ReplicationNode node = await store.ReadTreeAsync<ReplicationNode>(RefName);
			DirectoryNode root = await node.Contents.ExpandAsync();

			Sha1Hash tree = await WriteTreeRecursiveAsync(root, CancellationToken.None);
			Sha1Hash commit = await WriteCommitObjectAsync(tree, CancellationToken.None);
			logger.LogInformation("Commit: {Sha}", commit);

			await WriteTextFileAsync("HEAD", "ref: refs/heads/main");
			await WriteTextFileAsync("refs/heads/main", commit.ToString());

			return 0;
		}

		async Task<Sha1Hash> WriteTreeRecursiveAsync(DirectoryNode root, CancellationToken cancellationToken)
		{
			Sha1Hash hash = await WriteTreeObjectAsync(root, cancellationToken);

			foreach (DirectoryEntry entry in root.Directories)
			{
				DirectoryNode dir = await entry.ExpandAsync(cancellationToken);
				await WriteTreeRecursiveAsync(dir, cancellationToken);
			}

			foreach (FileEntry entry in root.Files)
			{
				await WriteBlobObjectAsync(entry, cancellationToken);
			}

			return hash;
		}

		async Task<Sha1Hash> WriteCommitObjectAsync(Sha1Hash tree, CancellationToken cancellationToken)
		{
			GitCommit commit = new GitCommit(tree, "Test");
			byte[] data = commit.Serialize();
			return await WriteObjectAsync(data, cancellationToken);
		}

		async Task<Sha1Hash> WriteTreeObjectAsync(DirectoryNode dir, CancellationToken cancellationToken)
		{
			GitTree tree = dir.AsGitTree();
			byte[] data = tree.Serialize();
			return await WriteObjectAsync(data, cancellationToken);
		}

		async Task WriteBlobObjectAsync(FileEntry entry, CancellationToken cancellationToken)
		{
			FileReference output = GetBlobPath(OutputDir, entry.GitHash);
			DirectoryReference.CreateDirectory(output.Directory);

			using (FileStream stream = FileReference.Open(output, FileMode.Create))
			{
				using (ZLibStream compressed = new ZLibStream(stream, CompressionLevel.Fastest, true))
				{
					await GitObject.WriteHeaderAsync(compressed, GitObjectType.Blob, entry.Length, cancellationToken);

					FileNode node = await entry.ExpandAsync(cancellationToken);
					await node.CopyToStreamAsync(compressed, cancellationToken);
				}
			}
		}

		async Task<Sha1Hash> WriteObjectAsync(byte[] data, CancellationToken cancellationToken)
		{
			Sha1Hash hash = Sha1Hash.Compute(data);

			FileReference output = GetBlobPath(OutputDir, hash);
			DirectoryReference.CreateDirectory(output.Directory);

			using (FileStream stream = FileReference.Open(output, FileMode.Create))
			{
				using (ZLibStream compressed = new ZLibStream(stream, CompressionLevel.Fastest, true))
				{
					await compressed.WriteAsync(data, cancellationToken);
				}
			}

			return hash;
		}

		static FileReference GetBlobPath(DirectoryReference rootDir, Sha1Hash hash)
		{
			string str = hash.ToString();
			return FileReference.Combine(rootDir, $".git/objects/{str.Substring(0, 2)}/{str.Substring(2)}");
		}

		async Task WriteTextFileAsync(string path, string text)
		{
			FileReference file = FileReference.Combine(OutputDir, ".git", path);
			DirectoryReference.CreateDirectory(file.Directory);
			await FileReference.WriteAllTextAsync(file, text);
		}
	}
}
