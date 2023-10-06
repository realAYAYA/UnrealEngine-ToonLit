// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// In-process buffer used to store compute messages
	/// </summary>
	public abstract class ComputeBuffer : IComputeBuffer
	{
		const int MaxChunks = 16;
		const int MaxReaders = 16;

		internal const int HeaderSize = (2 + MaxChunks + MaxReaders) * sizeof(ulong);

		/// <summary>
		/// Tracked state of the buffer
		/// </summary>
		internal readonly unsafe struct HeaderPtr
		{
			readonly ulong* _data;

			public HeaderPtr(ulong* data) => _data = data;

			public HeaderPtr(ulong* data, int numReaders, int numChunks, int chunkLength)
			{
				_data = data;

				data[0] = ((ulong)(uint)numChunks << 32) | (uint)numReaders;
				data[1] = (uint)chunkLength;

				GetChunkStatePtr(0).StartWriting(numReaders);
			}

			public int NumReaders => (int)_data[0];
			public int NumChunks => (int)(_data[0] >> 32);
			public int ChunkLength => (int)_data[1];
			public int WriteChunkIdx
			{
				get => (int)(_data[1] >> 32);
				set => _data[1] = ((ulong)value << 32) | (uint)ChunkLength;
			}

			public ChunkStatePtr GetChunkStatePtr(int chunkIdx) => new ChunkStatePtr(_data + 2 + chunkIdx);

			public ReaderStatePtr GetReaderStatePtr(int readerIdx) => new ReaderStatePtr(_data + 2 + MaxChunks + readerIdx);
		}

		/// <summary>
		/// Write state for a chunk
		/// </summary>
		internal enum WriteState
		{
			/// <summary>
			/// Writer has moved to the next chunk
			/// </summary>
			MovedToNext = 0,

			/// <summary>
			/// Chunk is still being appended to
			/// </summary>
			Writing = 2,

			/// <summary>
			/// This chunk marks the end of the stream
			/// </summary>
			Complete = 3,
		}

		/// <summary>
		/// Stores the state of a chunk in a 64-bit value, which can be updated atomically
		/// </summary>
		internal record struct ChunkState(ulong Value)
		{
			// Written length of this chunk
			public int Length => (int)(Value & 0x7fffffff);

			// Set of flags which are set for each reader that still has to read from a chunk
			public int ReaderFlags => (int)((Value >> 31) & 0x7fffffff);

			// State of the writer
			public WriteState WriteState => (WriteState)(Value >> 62);

			// Constructor
			public ChunkState(WriteState writerState, int readerFlags, int length) : this(((ulong)writerState << 62) | ((ulong)readerFlags << 31) | (uint)length) { }

			// Test whether a particular reader is still referencing the chunk
			public bool HasReaderFlag(int readerIdx) => (Value & (1UL << (readerIdx + 31))) != 0;

			/// <inheritdoc/>
			public override string ToString() => $"{WriteState}, Length: {Length}, Readers: {ReaderFlags}";
		}

		/// <summary>
		/// Wraps a pointer to the state of a chunk
		/// </summary>
		internal readonly unsafe struct ChunkStatePtr
		{
			readonly ulong* _data;

			public ChunkStatePtr(ulong* data) => _data = data;

			// Current value of the chunk state
			public ChunkState Value => new ChunkState(Interlocked.CompareExchange(ref *_data, 0, 0));

			// Written length of this chunk
			public int Length => Value.Length;

			// Set of flags which are set for each reader that still has to read from a chunk
			public int ReaderFlags => Value.ReaderFlags;

			// State of the writer
			public WriteState WriteState => Value.WriteState;

			// Append data to the chunk
			public void Append(int length) => Interlocked.Add(ref *_data, (uint)length);

			// Mark the chunk as being written to
			public void StartWriting(int numReaders) => Interlocked.Exchange(ref *_data, new ChunkState(WriteState.Writing, (1 << numReaders) - 1, 0).Value);

			// Move to the next chunk
			public void MarkComplete() => Interlocked.Or(ref *_data, (ulong)WriteState.Complete << 62);

			// Clear the reader flag
			public void FinishReading(int readerIdx) => Interlocked.And(ref *_data, ~(1UL << (readerIdx + 31)));

			// Move to the next chunk
			public void FinishWriting() => Interlocked.And(ref *_data, ~((ulong)WriteState.Writing << 62));

			/// <inheritdoc/>
			public override string ToString() => Value.ToString();
		}

		/// <summary>
		/// Encodes the state of a reader in a 64-bit value
		/// </summary>
		internal record struct ReaderState(ulong Value)
		{
			public ReaderState(int chunkIdx, int offset) : this(((ulong)chunkIdx << 32) | (uint)offset) { }

			public int ChunkIdx => (int)(Value >> 32);
			public int Offset => (int)Value;

			public override string ToString() => $"Chunk {ChunkIdx}, Offset {Offset}";
		}

		/// <summary>
		/// Pointer to a reader state value
		/// </summary>
		internal readonly unsafe struct ReaderStatePtr
		{
			readonly ulong* _data;

			public ReaderStatePtr(ulong* data) => _data = data;

			public ReaderState Value
			{
				get => new ReaderState(Interlocked.CompareExchange(ref *_data, 0, 0));
				set => Interlocked.Exchange(ref *_data, value.Value);
			}

			public int ChunkIdx => Value.ChunkIdx;
			public int Offset => Value.Offset;

			public void Advance(int length) => Interlocked.Add(ref *_data, (ulong)length);

			public override string ToString() => Value.ToString();
		}

		/// <summary>
		/// State shared between buffer instances
		/// </summary>
		internal abstract class ResourcesBase : IDisposable
		{
			public HeaderPtr HeaderPtr { get; }
			public Memory<byte>[] Chunks { get; }
			int _refCount = 1;

			public ResourcesBase(HeaderPtr headerPtr, Memory<byte>[] chunks)
			{
				HeaderPtr = headerPtr;
				Chunks = chunks;
			}

			public void AddRef()
			{
				Interlocked.Increment(ref _refCount);
			}

			public void Release()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					Dispose();
				}
			}

			/// <summary>
			/// Dispose of the 
			/// </summary>
			public abstract void Dispose();

			/// <summary>
			/// Signals a read event
			/// </summary>
			public abstract void SetReadEvent(int readerIdx);

			/// <summary>
			/// Signals read events for every reader
			/// </summary>
			public void SetAllReadEvents()
			{
				for (int readerIdx = 0; readerIdx < HeaderPtr.NumReaders; readerIdx++)
				{
					SetReadEvent(readerIdx);
				}
			}

			/// <summary>
			/// Resets a read event
			/// </summary>
			public abstract void ResetReadEvent(int readerIdx);

			/// <summary>
			/// Waits for a read event to be signalled
			/// </summary>
			public abstract Task WaitForReadEvent(int readerIdx, CancellationToken cancellationToken);

			/// <summary>
			/// Signals the write event
			/// </summary>
			public abstract void SetWriteEvent();

			/// <summary>
			/// Resets the write event
			/// </summary>
			public abstract void ResetWriteEvent();

			/// <summary>
			/// Waits for the write event to be signalled
			/// </summary>
			public abstract Task WaitForWriteEvent(CancellationToken cancellationToken);

#pragma warning disable IDE0051 // Remove unused private members
			// For debugging purposes only
			ChunkState[] ChunkStates => Enumerable.Range(0, HeaderPtr.NumChunks).Select(x => HeaderPtr.GetChunkStatePtr(x).Value).ToArray();
			ReaderState[] ReaderStates => Enumerable.Range(0, HeaderPtr.NumReaders).Select(x => HeaderPtr.GetReaderStatePtr(x).Value).ToArray();
			int WriteChunkIdx => HeaderPtr.WriteChunkIdx;
#pragma warning restore IDE0051 // Remove unused private members
		}

		internal class ReaderImpl : IComputeBufferReader
		{
			readonly HeaderPtr _headerPtr;
			readonly Memory<byte>[] _chunks;
			ResourcesBase _resources;
			readonly int _readerIdx;

			public ReaderImpl(ResourcesBase resources, int readerIdx)
			{
				_headerPtr = resources.HeaderPtr;
				_chunks = resources.Chunks;
				_resources = resources;
				_readerIdx = readerIdx;
			}

			public ResourcesBase GetResources() => _resources;

			/// <inheritdoc/>
			public IComputeBufferReader AddRef()
			{
				_resources.AddRef();
				return new ReaderImpl(_resources, _readerIdx);
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_resources != null)
				{
					_resources.Release();
					_resources = null!;
				}
			}

			/// <inheritdoc/>
			public bool IsComplete
			{
				get
				{
					ReaderState readerState = _headerPtr.GetReaderStatePtr(_readerIdx).Value;
					ChunkState chunkState = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx).Value;
					return chunkState.WriteState == WriteState.Complete && readerState.Offset == chunkState.Length;
				}
			}

			/// <inheritdoc/>
			public void AdvanceReadPosition(int length)
			{
				ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(_readerIdx);
				readerStatePtr.Advance(length);
			}

			/// <inheritdoc/>
			public ReadOnlyMemory<byte> GetReadBuffer()
			{
				ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(_readerIdx);
				ReaderState readerState = readerStatePtr.Value;

				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx);
				ChunkState chunkState = chunkStatePtr.Value;

				if (chunkState.HasReaderFlag(_readerIdx))
				{
					return _chunks[readerState.ChunkIdx].Slice(readerState.Offset, chunkState.Length - readerState.Offset);
				}
				else
				{
					return ReadOnlyMemory<byte>.Empty;
				}
			}

			/// <inheritdoc/>
			public async ValueTask<bool> WaitToReadAsync(int minLength, CancellationToken cancellationToken = default)
			{
				for (; ; )
				{
					ReaderStatePtr readerStatePtr = _headerPtr.GetReaderStatePtr(_readerIdx);
					ReaderState readerState = readerStatePtr.Value;

					ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(readerState.ChunkIdx);
					ChunkState chunkState = chunkStatePtr.Value;

					if (!chunkState.HasReaderFlag(_readerIdx))
					{
						// Wait until the current chunk is readable
						_resources.ResetReadEvent(_readerIdx);
						if (!chunkState.HasReaderFlag(_readerIdx))
						{
							await _resources.WaitForReadEvent(_readerIdx, cancellationToken);
						}
					}
					else if (readerState.Offset + minLength <= chunkState.Length)
					{
						// We have enough data in the chunk to be able to read a message
						return true;
					}
					else if (chunkState.WriteState == WriteState.Writing)
					{
						// Wait until there is more data in the chunk
						_resources.ResetReadEvent(_readerIdx);
						if (_headerPtr.GetChunkStatePtr(readerState.ChunkIdx).Value == chunkState)
						{
							await _resources.WaitForReadEvent(_readerIdx, cancellationToken);
						}
					}
					else if (readerState.Offset < chunkState.Length || chunkState.WriteState == WriteState.Complete)
					{
						// Cannot read the requested amount of data from this chunk.
						return false;
					}
					else if (chunkState.WriteState == WriteState.MovedToNext)
					{
						// Move to the next chunk
						chunkStatePtr.FinishReading(_readerIdx);
						_resources.SetWriteEvent();

						int chunkIdx = readerStatePtr.ChunkIdx + 1;
						if (chunkIdx == _chunks.Length)
						{
							chunkIdx = 0;
						}

						readerStatePtr.Value = new ReaderState(chunkIdx, 0);
					}
					else
					{
						throw new NotImplementedException($"Invalid write state for buffer: {chunkState.WriteState}");
					}
				}
			}
		}

		internal class WriterImpl : IComputeBufferWriter
		{
			readonly HeaderPtr _headerPtr;
			readonly Memory<byte>[] _chunks;
			ResourcesBase _resources;

			public WriterImpl(ResourcesBase resources)
			{
				_headerPtr = resources.HeaderPtr;
				_chunks = resources.Chunks;
				_resources = resources;
			}

			public ResourcesBase GetResources() => _resources;

			/// <inheritdoc/>
			public IComputeBufferWriter AddRef()
			{
				_resources.AddRef();
				return new WriterImpl(_resources);
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_resources != null)
				{
					_resources.Release();
					_resources = null!;
				}
			}

			/// <inheritdoc/>
			public void AdvanceWritePosition(int size)
			{
				if (size > 0)
				{
					ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);
					ChunkState chunkState = chunkStatePtr.Value;

					Debug.Assert(chunkState.WriteState == WriteState.Writing);
					chunkStatePtr.Append(size);

					_resources.SetAllReadEvents();
				}
			}

			/// <inheritdoc/>
			public Memory<byte> GetWriteBuffer()
			{
				ChunkState state = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx).Value;
				if (state.WriteState == WriteState.Writing)
				{
					return _chunks[_headerPtr.WriteChunkIdx].Slice(state.Length);
				}
				else
				{
					return Memory<byte>.Empty;
				}
			}

			/// <inheritdoc/>
			public bool MarkComplete()
			{
				ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);
				if (chunkStatePtr.WriteState != WriteState.Complete)
				{
					chunkStatePtr.MarkComplete();
					_resources.SetAllReadEvents();
					return true;
				}
				return false;
			}

			/// <inheritdoc/>
			public async ValueTask WaitToWriteAsync(int minSize, CancellationToken cancellationToken = default)
			{
				if (minSize > _headerPtr.ChunkLength)
				{
					throw new ArgumentException("Requested read size is larger than chunk size.", nameof(minSize));
				}

				for (; ; )
				{
					ChunkStatePtr chunkStatePtr = _headerPtr.GetChunkStatePtr(_headerPtr.WriteChunkIdx);

					ChunkState chunkState = chunkStatePtr.Value;
					if (chunkState.WriteState == WriteState.Writing)
					{
						int length = chunkState.Length;
						if (length + minSize <= _headerPtr.ChunkLength)
						{
							return;
						}
						chunkStatePtr.FinishWriting();
					}

					int nextChunkIdx = _headerPtr.WriteChunkIdx + 1;
					if (nextChunkIdx == _chunks.Length)
					{
						nextChunkIdx = 0;
					}

					ChunkStatePtr nextChunkStatePtr = _headerPtr.GetChunkStatePtr(nextChunkIdx);
					while (nextChunkStatePtr.ReaderFlags != 0)
					{
						await _resources.WaitForWriteEvent(cancellationToken);
						_resources.ResetWriteEvent();
					}

					_headerPtr.WriteChunkIdx = nextChunkIdx;
					nextChunkStatePtr.StartWriting(_headerPtr.NumReaders);

					_resources.SetAllReadEvents();
				}
			}
		}

		internal ResourcesBase _resources;

		/// <inheritdoc/>
		public IComputeBufferReader Reader { get; }

		/// <inheritdoc/>
		public IComputeBufferWriter Writer { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="resources">Resources shared between instances of the buffer</param>
		internal ComputeBuffer(ResourcesBase resources)
		{
			_resources = resources;
			Reader = new ReaderImpl(resources, 0);
			Writer = new WriterImpl(resources);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overriable dispose method
		/// </summary>
		protected virtual void Dispose(bool disposing)
		{
			if (_resources != null)
			{
				_resources.Release();
				_resources = null!;
			}
		}

		/// <inheritdoc/>
		public abstract IComputeBuffer AddRef();
	}
}
