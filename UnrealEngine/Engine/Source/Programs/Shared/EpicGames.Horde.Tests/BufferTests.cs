// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO.Pipelines;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BufferTests
	{
		const int ChannelId = 0;

		[TestMethod]
		public async Task TestSimpleBuffer()
		{
			using PooledBuffer buffer = new PooledBuffer(2, 1024);
			buffer.Writer.AdvanceWritePosition(10);
			await buffer.Reader.WaitToReadAsync(9);
			buffer.Reader.AdvanceReadPosition(9);
			await buffer.Reader.WaitToReadAsync(1);
		}

		[TestMethod]
		public void TestOverflow()
		{
			using PooledBuffer buffer = new PooledBuffer(2, 20);

			// Fill up the first chunk
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(10, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(0, buffer.Writer.GetWriteBuffer().Length);

			Task waitToWriteTask = buffer.Writer.WaitToWriteAsync(1).AsTask();
			Assert.IsTrue(waitToWriteTask.IsCompleted);

			// Fill up the second chunk
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(10, buffer.Writer.GetWriteBuffer().Length);
			buffer.Writer.AdvanceWritePosition(10);
			Assert.AreEqual(0, buffer.Writer.GetWriteBuffer().Length);

			waitToWriteTask = buffer.Writer.WaitToWriteAsync(1).AsTask();
			Assert.IsFalse(waitToWriteTask.IsCompleted);

			// Wait for data to be read
			Assert.AreEqual(20, buffer.Reader.GetReadBuffer().Length);
			buffer.Reader.AdvanceReadPosition(10);
			Assert.IsFalse(waitToWriteTask.IsCompleted);

			Assert.AreEqual(10, buffer.Reader.GetReadBuffer().Length);
			buffer.Reader.AdvanceReadPosition(10);
			Assert.AreEqual(0, buffer.Reader.GetReadBuffer().Length);

			Task waitToReadTask = buffer.Reader.WaitToReadAsync(1).AsTask();
			Assert.IsTrue(waitToReadTask.IsCompleted);

			Assert.AreEqual(20, buffer.Reader.GetReadBuffer().Length);
			Assert.IsTrue(waitToWriteTask.IsCompleted);

			// Make sure both reader and writer have something to work with
			Assert.AreEqual(20, buffer.Reader.GetReadBuffer().Length);
			Assert.AreEqual(20, buffer.Writer.GetWriteBuffer().Length);
		}

		[TestMethod]
		public async Task TestPooledBuffer()
		{
			await TestProducerConsumerAsync(length => new PooledBuffer(length), CancellationToken.None);
		}

		[TestMethod]
		public async Task TestSharedMemoryBuffer()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				await TestProducerConsumerAsync(length => SharedMemoryBuffer.CreateNew(null, length), CancellationToken.None);
			}
		}

		static async Task TestProducerConsumerAsync(Func<int, IComputeBuffer> createBuffer, CancellationToken cancellationToken)
		{
			const int Length = 8000;

			Pipe sourceToTargetPipe = new Pipe();
			Pipe targetToSourcePipe = new Pipe();

			await using ComputeSocket producerSocket = new ComputeSocket(new PipeTransport(targetToSourcePipe.Reader, sourceToTargetPipe.Writer), ComputeSocketEndpoint.Local, NullLogger.Instance);
			await using ComputeSocket consumerSocket = new ComputeSocket(new PipeTransport(sourceToTargetPipe.Reader, targetToSourcePipe.Writer), ComputeSocketEndpoint.Remote, NullLogger.Instance);

			using IComputeBuffer consumerBuffer = createBuffer(Length);
			consumerSocket.AttachRecvBuffer(ChannelId, consumerBuffer.Writer);

			byte[] input = RandomNumberGenerator.GetBytes(Length);
			Task producerTask = RunProducerAsync(producerSocket, input);

			byte[] output = new byte[Length];
			await RunConsumerAsync(consumerBuffer.Reader, output);

			await producerTask;
			Assert.IsTrue(input.SequenceEqual(output));
		}

		static async Task RunProducerAsync(ComputeSocket socket, ReadOnlyMemory<byte> input)
		{
			int offset = 0;
			while (offset < input.Length)
			{
				int length = Math.Min(input.Length - offset, 100);
				await socket.SendAsync(ChannelId, input.Slice(offset, length));
				await Task.Delay(10);
				offset += length;
			}
			await socket.MarkCompleteAsync(ChannelId);
		}

		static async Task RunConsumerAsync(IComputeBufferReader reader, Memory<byte> output)
		{
			int offset = 0;
			while (!reader.IsComplete)
			{
				ReadOnlyMemory<byte> memory = reader.GetReadBuffer();
				if (memory.Length == 0)
				{
					await reader.WaitToReadAsync(1, CancellationToken.None);
					continue;
				}

				int length = Math.Min(memory.Length, 7);
				memory.Slice(0, length).CopyTo(output.Slice(offset));
				reader.AdvanceReadPosition(length);
				offset += length;
			}
		}
	}
}
