// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Linq;
using System.Threading.Tasks;
using System.IO;
using System.Threading;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging.Abstractions;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class ChunkingTests
	{
		[TestMethod]
		public void BuzHashTests()
		{
			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			const int WindowSize = 128;

			uint rollingHash = 0;
			for (int maxIdx = 0; maxIdx < data.Length + WindowSize; maxIdx++)
			{
				int minIdx = maxIdx - WindowSize;

				if (maxIdx < data.Length)
				{
					rollingHash = BuzHash.Add(rollingHash, data[maxIdx]);
				}

				int length = Math.Min(maxIdx + 1, data.Length) - Math.Max(minIdx, 0);
				uint cleanHash = BuzHash.Add(0, data.AsSpan(Math.Max(minIdx, 0), length));
				Assert.AreEqual(rollingHash, cleanHash);

				if (minIdx >= 0)
				{
					rollingHash = BuzHash.Sub(rollingHash, data[minIdx], length);
				}
			}
		}

		[TestMethod]
		public async Task FixedSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(64, 64, 64);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(4, 4, 4);

			await TestChunkingAsync(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new LeafChunkedDataNodeOptions(32, 64, 96);
			options.InteriorOptions = new InteriorChunkedDataNodeOptions(1, 4, 12);

			await TestChunkingAsync(options);
		}

		async Task TestChunkingAsync(ChunkingOptions options)
		{
			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			MemoryStorageClient store = new MemoryStorageClient();
			BundleReader reader = new BundleReader(store, cache, NullLogger.Instance);
			await using IStorageWriter writer = store.CreateWriter();

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			for (int idx = 0; idx < data.Length; idx++)
			{
				data[idx] = (byte)idx;
			}

			NodeRef<ChunkedDataNode> handle;

			const int NumIterations = 100;
			{
				ChunkedDataWriter fileWriter = new ChunkedDataWriter(writer, options);

				for (int idx = 0; idx < NumIterations; idx++)
				{
					await fileWriter.AppendAsync(data, CancellationToken.None);
				}

				handle = await fileWriter.FlushAsync(CancellationToken.None);
			}

			ChunkedDataNode root = await handle.ExpandAsync();

			byte[] result;
			using (MemoryStream stream = new MemoryStream())
			{
				await root.CopyToStreamAsync(stream, CancellationToken.None);
				result = stream.ToArray();
			}

			Assert.AreEqual(NumIterations * data.Length, result.Length);

			for (int idx = 0; idx < NumIterations; idx++)
			{
				ReadOnlyMemory<byte> spanData = result.AsMemory(idx * data.Length, data.Length);
				Assert.IsTrue(spanData.Span.SequenceEqual(data));
			}

			await CheckSizes(reader, root, options, true);
		}

		async Task CheckSizes(BundleReader reader, ChunkedDataNode node, ChunkingOptions options, bool rightmost)
		{
			if (node is LeafChunkedDataNode leafNode)
			{
				Assert.IsTrue(rightmost || leafNode.Data.Length >= options.LeafOptions.MinSize);
				Assert.IsTrue(leafNode.Data.Length <= options.LeafOptions.MaxSize);
			}
			else
			{
				InteriorChunkedDataNode interiorNode = (InteriorChunkedDataNode)node;

				Assert.IsTrue(rightmost || interiorNode.Children.Count >= options.InteriorOptions.MinChildCount);
				Assert.IsTrue(interiorNode.Children.Count <= options.InteriorOptions.MaxChildCount);

				int childCount = interiorNode.Children.Count;
				for (int idx = 0; idx < childCount; idx++)
				{
					ChunkedDataNode childNode = await interiorNode.Children[idx].ExpandAsync(CancellationToken.None);
					await CheckSizes(reader, childNode, options, idx == childCount - 1);
				}
			}
		}
	}
}
