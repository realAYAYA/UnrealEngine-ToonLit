// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Compute.Buffers
{
	/// <summary>
	/// In-process buffer used to store compute messages
	/// </summary>
	public sealed class PooledBuffer : ComputeBuffer
	{
		class Resources : ResourcesBase
		{
			readonly GCHandle _headerHandle;
			readonly IMemoryOwner<byte>[] _chunkOwners;

			readonly AsyncEvent _writerEvent = new AsyncEvent();
			readonly AsyncEvent _readerEvent = new AsyncEvent();

			public Resources(HeaderPtr headerPtr, GCHandle headerHandle, Memory<byte>[] chunks, IMemoryOwner<byte>[] chunkOwners)
				: base(headerPtr, chunks)
			{
				_headerHandle = headerHandle;
				_chunkOwners = chunkOwners;
			}

			public static unsafe Resources Create(int numChunks, int chunkLength, int numReaders)
			{
				byte[] header = new byte[HeaderSize];
				GCHandle headerHandle = GCHandle.Alloc(header, GCHandleType.Pinned);
				HeaderPtr headerPtr = new HeaderPtr((ulong*)headerHandle.AddrOfPinnedObject().ToPointer(), numReaders, numChunks, chunkLength);

				Memory<byte>[] chunks = new Memory<byte>[numChunks];
				IMemoryOwner<byte>[] chunkOwners = new IMemoryOwner<byte>[numChunks];

				for (int idx = 0; idx < numChunks; idx++)
				{
					chunkOwners[idx] = MemoryPool<byte>.Shared.Rent(chunkLength);
					chunks[idx] = chunkOwners[idx].Memory.Slice(0, chunkLength);
				}

				return new Resources(headerPtr, headerHandle, chunks, chunkOwners);
			}

			public override void Dispose()
			{
				for (int idx = 0; idx < _chunkOwners.Length; idx++)
				{
					_chunkOwners[idx].Dispose();
				}
				_headerHandle.Free();
			}

			/// <inheritdoc/>
			public override void SetReadEvent(int readerIdx) => _readerEvent.Set();

			/// <inheritdoc/>
			public override void ResetReadEvent(int readerIdx) => _readerEvent.Reset();

			/// <inheritdoc/>
			public override Task WaitForReadEvent(int readerIdx, CancellationToken cancellationToken) => _readerEvent.Task.WaitAsync(cancellationToken);

			/// <inheritdoc/>
			public override void SetWriteEvent() => _writerEvent.Set();

			/// <inheritdoc/>
			public override void ResetWriteEvent() => _writerEvent.Reset();

			/// <inheritdoc/>
			public override Task WaitForWriteEvent(CancellationToken cancellationToken) => _writerEvent.Task.WaitAsync(cancellationToken);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="capacity">Total capacity of the buffer</param>
		public PooledBuffer(int capacity)
			: this(2, capacity / 2)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="numChunks">Number of chunks in the buffer</param>
		/// <param name="chunkLength">Length of each chunk</param>
		/// <param name="numReaders">Number of readers for this buffer</param>
		public PooledBuffer(int numChunks, int chunkLength, int numReaders = 1)
			: base(Resources.Create(numChunks, chunkLength, numReaders))
		{
		}

		private PooledBuffer(ResourcesBase resources)
			: base(resources)
		{
		}

		/// <inheritdoc/>
		public override IComputeBuffer AddRef()
		{
			_resources.AddRef();
			return new PooledBuffer(_resources);
		}
	}
}
