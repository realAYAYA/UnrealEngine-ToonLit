// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using System.IO.MemoryMappedFiles;
using System.IO;
using System.Threading.Tasks;
using System.Threading;
using System.Runtime.InteropServices;

namespace EpicGames.Horde.Compute.Buffers
{
	/// <summary>
	/// Core implementation of <see cref="SharedMemoryBuffer"/>
	/// </summary>
	public sealed class SharedMemoryBuffer : ComputeBuffer
	{
		class Resources : ResourcesBase
		{
			readonly MemoryMappedFile _memoryMappedFile;
			readonly MemoryMappedViewAccessor _memoryMappedViewAccessor;
			readonly MemoryMappedView _memoryMappedView;

			readonly Native.EventHandle _readerEvent;
			readonly Native.EventHandle _writerEvent;

			public string Name { get; }

			public Resources(string name, HeaderPtr headerPtr, Memory<byte>[] chunks, MemoryMappedFile memoryMappedFile, MemoryMappedViewAccessor memoryMappedViewAccessor, MemoryMappedView memoryMappedView, Native.EventHandle readerEvent, Native.EventHandle writerEvent)
				: base(headerPtr, chunks)
			{
				Name = name;

				_memoryMappedFile = memoryMappedFile;
				_memoryMappedViewAccessor = memoryMappedViewAccessor;
				_memoryMappedView = memoryMappedView;

				_readerEvent = readerEvent;
				_writerEvent = writerEvent;
			}

			public override void Dispose()
			{
				_readerEvent.Dispose();
				_writerEvent.Dispose();

				_memoryMappedView.Dispose();
				_memoryMappedViewAccessor.Dispose();
				_memoryMappedFile.Dispose();
			}

			/// <inheritdoc/>
			public override void SetReadEvent(int readerIdx) => _readerEvent.Set();

			/// <inheritdoc/>
			public override void ResetReadEvent(int readerIdx) => _readerEvent.Reset();

			/// <inheritdoc/>
			public override Task WaitForReadEvent(int readerIdx, CancellationToken cancellationToken) => _readerEvent.WaitOneAsync(cancellationToken);

			/// <inheritdoc/>
			public override void SetWriteEvent() => _writerEvent.Set();

			/// <inheritdoc/>
			public override void ResetWriteEvent() => _writerEvent.Reset();

			/// <inheritdoc/>
			public override Task WaitForWriteEvent(CancellationToken cancellationToken) => _writerEvent.WaitOneAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public string Name => ((Resources)_resources).Name;

		/// <summary>
		/// Constructor
		/// </summary>
		private SharedMemoryBuffer(ResourcesBase resources)
			: base(resources)
		{
		}

		/// <inheritdoc/>
		public override IComputeBuffer AddRef()
		{
			_resources.AddRef();
			return new SharedMemoryBuffer(_resources);
		}

		/// <summary>
		/// Create a new shared memory buffer
		/// </summary>
		/// <param name="name">Name of the buffer</param>
		/// <param name="capacity">Capacity of the buffer</param>
		public static SharedMemoryBuffer CreateNew(string? name, long capacity)
		{
			int numChunks = (int)Math.Max(4, capacity / Int32.MaxValue);
			return CreateNew(name, numChunks, (int)(capacity / numChunks));
		}

		/// <summary>
		/// Create a new shared memory buffer
		/// </summary>
		/// <param name="name">Name of the buffer</param>
		/// <param name="numChunks">Number of chunks in the buffer</param>
		/// <param name="chunkLength">Length of each chunk</param>
		public static unsafe SharedMemoryBuffer CreateNew(string? name, int numChunks, int chunkLength)
		{
			long capacity = HeaderSize + (numChunks * sizeof(ulong)) + (numChunks * chunkLength);

			name ??= $"Local\\COMPUTE_{Guid.NewGuid()}";

			MemoryMappedFile memoryMappedFile = MemoryMappedFile.CreateNew($"{name}_M", capacity, MemoryMappedFileAccess.ReadWrite, MemoryMappedFileOptions.None, HandleInheritability.Inheritable);
			MemoryMappedViewAccessor memoryMappedViewAccessor = memoryMappedFile.CreateViewAccessor();
			MemoryMappedView memoryMappedView = new MemoryMappedView(memoryMappedViewAccessor);

			Native.EventHandle writerEvent = Native.EventHandle.CreateNew($"{name}_W", EventResetMode.ManualReset, true, HandleInheritability.Inheritable);
			Native.EventHandle readerEvent = Native.EventHandle.CreateNew($"{name}_R0", EventResetMode.ManualReset, true, HandleInheritability.Inheritable);

			HeaderPtr headerPtr = new HeaderPtr((ulong*)memoryMappedView.GetPointer(), 1, numChunks, chunkLength);
			Memory<byte>[] chunks = CreateChunks(headerPtr.NumChunks, headerPtr.ChunkLength, memoryMappedView);

			return new SharedMemoryBuffer(new Resources(name, headerPtr, chunks, memoryMappedFile, memoryMappedViewAccessor, memoryMappedView, readerEvent, writerEvent));
		}

		/// <summary>
		/// Open an existing buffer by name
		/// </summary>
		/// <param name="name">Name of the buffer to open</param>
		public static unsafe SharedMemoryBuffer OpenExisting(string name)
		{
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				throw new NotSupportedException();
			}

			MemoryMappedFile memoryMappedFile = MemoryMappedFile.OpenExisting($"{name}_M");
			MemoryMappedViewAccessor memoryMappedViewAccessor = memoryMappedFile.CreateViewAccessor();
			MemoryMappedView memoryMappedView = new MemoryMappedView(memoryMappedViewAccessor);

			Native.EventHandle readerEvent = Native.EventHandle.OpenExisting($"{name}_R0");
			Native.EventHandle writerEvent = Native.EventHandle.OpenExisting($"{name}_W");

			HeaderPtr headerPtr = new HeaderPtr((ulong*)memoryMappedView.GetPointer());
			Memory<byte>[] chunks = CreateChunks(headerPtr.NumChunks, headerPtr.ChunkLength, memoryMappedView);

			return new SharedMemoryBuffer(new Resources(name, headerPtr, chunks, memoryMappedFile, memoryMappedViewAccessor, memoryMappedView, readerEvent, writerEvent));
		}

		static Memory<byte>[] CreateChunks(int numChunks, int chunkLength, MemoryMappedView memoryMappedView)
		{
			Memory<byte>[] chunks = new Memory<byte>[numChunks];
			for (int chunkIdx = 0; chunkIdx < numChunks; chunkIdx++)
			{
				int chunkOffset = HeaderSize + (chunkLength * chunkIdx);
				chunks[chunkIdx] = memoryMappedView.GetMemory(chunkOffset, chunkLength);
			}
			return chunks;
		}

		internal static string GetName(IComputeBufferReader reader)
		{
			Resources resources = (Resources)((ReaderImpl)reader).GetResources();
			return resources.Name;
		}

		internal static string GetName(IComputeBufferWriter writer)
		{
			Resources resources = (Resources)((WriterImpl)writer).GetResources();
			return resources.Name;
		}
	}
}
