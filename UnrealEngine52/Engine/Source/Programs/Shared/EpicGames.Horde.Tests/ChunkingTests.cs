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
			options.LeafOptions = new ChunkingOptionsForNodeType(64, 64, 64);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 4, IoHash.NumBytes * 4, IoHash.NumBytes * 4);

			await TestChunkingAsync(options);
		}

		[TestMethod]
		public async Task VariableSizeChunkingTests()
		{
			ChunkingOptions options = new ChunkingOptions();
			options.LeafOptions = new ChunkingOptionsForNodeType(32, 64, 96);
			options.InteriorOptions = new ChunkingOptionsForNodeType(IoHash.NumBytes * 1, IoHash.NumBytes * 4, IoHash.NumBytes * 12);

			await TestChunkingAsync(options);
		}

		async Task TestChunkingAsync(ChunkingOptions options)
		{
			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());

			MemoryStorageClient store = new MemoryStorageClient();
			TreeReader reader = new TreeReader(store, cache, NullLogger.Instance);
			using TreeWriter writer = new TreeWriter(store, new TreeOptions());

			byte[] data = new byte[4096];
			new Random(0).NextBytes(data);

			for (int idx = 0; idx < data.Length; idx++)
			{
				data[idx] = (byte)idx;
			}

			NodeHandle handle;

			const int NumIterations = 100;
			{
				FileNodeWriter fileWriter = new FileNodeWriter(writer, options);

				for (int idx = 0; idx < NumIterations; idx++)
				{
					await fileWriter.AppendAsync(data, CancellationToken.None);
				}

				handle = await fileWriter.FlushAsync(CancellationToken.None);
			}

			FileNode root = await reader.ReadNodeAsync<FileNode>(handle.Locator);

			byte[] result;
			using (MemoryStream stream = new MemoryStream())
			{
				await root.CopyToStreamAsync(reader, stream, CancellationToken.None);
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

		async Task CheckSizes(TreeReader reader, FileNode node, ChunkingOptions options, bool rightmost)
		{
			if (node is LeafFileNode leafNode)
			{
				Assert.IsTrue(rightmost || leafNode.Data.Length >= options.LeafOptions.MinSize);
				Assert.IsTrue(leafNode.Data.Length <= options.LeafOptions.MaxSize);
			}
			else
			{
				InteriorFileNode interiorNode = (InteriorFileNode)node;

				Assert.IsTrue(rightmost || interiorNode.Children.Count * IoHash.NumBytes >= options.InteriorOptions.MinSize);
				Assert.IsTrue(interiorNode.Children.Count <= options.InteriorOptions.MaxSize);

				int childCount = interiorNode.Children.Count;
				for (int idx = 0; idx < childCount; idx++)
				{
					FileNode childNode = await interiorNode.Children[idx].ExpandAsync(reader, CancellationToken.None);
					await CheckSizes(reader, childNode, options, idx == childCount - 1);
				}
			}
		}
	}
}
