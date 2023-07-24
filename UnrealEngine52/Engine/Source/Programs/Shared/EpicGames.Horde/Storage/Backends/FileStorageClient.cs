// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes data to files on disk.
	/// </summary>
	public class FileStorageClient : StorageClientBase
	{
		readonly DirectoryReference _rootDir;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Root directory for storing blobs</param>
		/// <param name="logger">Logger interface</param>
		public FileStorageClient(DirectoryReference rootDir, ILogger logger)
		{
			_rootDir = rootDir;
			_logger = logger;

			DirectoryReference.CreateDirectory(_rootDir);
		}

		FileReference GetRefFile(RefName name) => FileReference.Combine(_rootDir, name.ToString() + ".ref");
		FileReference GetBlobFile(BlobLocator id) => FileReference.Combine(_rootDir, id.Inner.ToString() + ".blob");

		#region Blobs

		/// <inheritdoc/>
		public override Task<Stream> ReadBlobAsync(BlobLocator id, CancellationToken cancellationToken = default)
		{
			FileReference file = GetBlobFile(id);
			_logger.LogInformation("Reading {File}", file);
			return Task.FromResult<Stream>(FileReference.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read));
		}

		/// <inheritdoc/>
		public override async Task<Stream> ReadBlobRangeAsync(BlobLocator id, int offset, int length, CancellationToken cancellationToken = default)
		{
			Stream stream = await ReadBlobAsync(id, cancellationToken);
			stream.Seek(offset, SeekOrigin.Begin);
			return stream;
		}

		/// <inheritdoc/>
		public override async Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BlobLocator id = BlobLocator.Create(HostId.Empty, prefix);
			FileReference file = GetBlobFile(id);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);

			using (FileStream fileStream = FileReference.Open(file, FileMode.Create, FileAccess.Write, FileShare.ReadWrite))
			{
				await stream.CopyToAsync(fileStream, cancellationToken);
			}

			return id;
		}

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public override IAsyncEnumerable<NodeHandle> FindNodesAsync(Utf8String alias, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			FileReference.Delete(file);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override async Task<NodeHandle?> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			if (!FileReference.Exists(file))
			{
				return null;
			}

			_logger.LogInformation("Reading {File}", file);
			string text = await FileReference.ReadAllTextAsync(file, cancellationToken);
			return NodeHandle.Parse(text);
		}

		/// <inheritdoc/>
		public override async Task WriteRefTargetAsync(RefName name, NodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);
			await FileReference.WriteAllTextAsync(file, target.ToString());
		}

		#endregion
	}
}
