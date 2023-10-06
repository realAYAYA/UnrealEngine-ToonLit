// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;

namespace Horde.Server.Commands.Bundles
{
	[Command("bundle", "writetest", "Synthetic benchmark for bundle write performance")]
	class BundleWriteTestCommand : Command
	{
		class FakeStorageClient : IStorageClient
		{
			readonly BundleReader _reader;

			public FakeStorageClient()
			{
				_reader = new BundleReader(this, null, NullLogger.Instance);
			}

			public Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => Task.CompletedTask;
			public Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public Task AddAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public Task RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String alias, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default) => throw new NotImplementedException();
			public Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default) => Task.FromResult(BlobLocator.Create(HostId.Empty));
			public Task WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default) => Task.CompletedTask;

			public BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null) => new BundleWriter(this, _reader, refName, options);

			IStorageWriter IStorageClient.CreateWriter(EpicGames.Horde.Storage.RefName refName) => CreateWriter(refName);
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			FakeStorageClient store = new FakeStorageClient();

			BundleOptions options = new BundleOptions();
			options.CompressionFormat = BundleCompressionFormat.None;
			await using IStorageWriter writer = store.CreateWriter(options: options);

			ChunkingOptions chunkingOptions = new ChunkingOptions();
//			chunkingOptions.LeafOptions = new ChunkingOptionsForNodeType(64 * 1024);

			ChunkedDataNode node = new LeafChunkedDataNode();

			byte[] buffer = new byte[64 * 1024];
			RandomNumberGenerator.Fill(buffer);

			Stopwatch timer = Stopwatch.StartNew();
			long length = 0;
			double nextTime = 2.0;

			ChunkedDataWriter fileNodeWriter = new ChunkedDataWriter(writer, chunkingOptions);
			for (; ; )
			{
				await fileNodeWriter.AppendAsync(buffer, default);
				length += buffer.Length;

				double time = timer.Elapsed.TotalSeconds;
				if (time > nextTime)
				{
					long size = GC.GetTotalMemory(true);
					logger.LogInformation("Written {Length:n0}mb in {Time:0.000}s ({Rate:n0}mb/s) (heap size: {Size:n0}mb)", length / (1024 * 1024), time, length / (1024 * 1024 * time), size / (1024.0 * 1024.0));
					nextTime = time + 2.0;
				}
			}
		}
	}
}
