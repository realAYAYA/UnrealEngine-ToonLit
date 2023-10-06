// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.IO.MemoryMappedFiles;

namespace EpicGames.Core
{
	/// <summary>
	/// Implements an unmarshlled view of a memory mapped file
	/// </summary>
	public sealed unsafe class MemoryMappedView : IDisposable
	{
		sealed unsafe class MemoryWrapper : MemoryManager<byte>
		{
			private readonly byte* _pointer;
			private readonly int _length;

			public MemoryWrapper(byte* pointer, int length)
			{
				_pointer = pointer;
				_length = length;
			}

			/// <inheritdoc/>
			public override Span<byte> GetSpan() => new Span<byte>(_pointer, _length);

			/// <inheritdoc/>
			public override MemoryHandle Pin(int elementIndex) => new MemoryHandle(_pointer + elementIndex);

			/// <inheritdoc/>
			public override void Unpin() { }

			/// <inheritdoc/>
			protected override void Dispose(bool disposing) { }
		}

		readonly MemoryMappedViewAccessor _memoryMappedViewAccessor;
		byte* _data;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="memoryMappedViewAccessor"></param>
		public MemoryMappedView(MemoryMappedViewAccessor memoryMappedViewAccessor)
		{
			_memoryMappedViewAccessor = memoryMappedViewAccessor;
			memoryMappedViewAccessor.SafeMemoryMappedViewHandle.AcquirePointer(ref _data);
		}

		/// <summary>
		/// Gets a pointer to the data
		/// </summary>
		public byte* GetPointer() => _data;

		/// <summary>
		/// Gets a memory object for the given range
		/// </summary>
		/// <param name="offset"></param>
		/// <param name="length"></param>
		/// <returns></returns>
		public Memory<byte> GetMemory(long offset, int length)
		{
			MemoryWrapper wrapper = new MemoryWrapper(_data + offset, length);
			return wrapper.Memory;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_data != null)
			{
				_memoryMappedViewAccessor.SafeMemoryMappedViewHandle.ReleasePointer();
				_data = null;
			}
		}
	}
}
