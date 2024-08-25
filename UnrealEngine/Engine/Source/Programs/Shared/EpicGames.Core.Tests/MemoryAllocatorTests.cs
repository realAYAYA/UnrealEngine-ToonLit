// Copyright Epic Games, Inc. All Rights Reserved.

using System.Buffers;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Core.Tests
{
	[TestClass]
	public class MemoryAllocatorTests
	{
		[TestMethod]
		public void TestGlobalAllocator()
		{
			GlobalHeapAllocator allocator = new GlobalHeapAllocator();

			using IMemoryOwner<byte> handle = allocator.Alloc(1024, null);
			handle.Memory.Span[0] = 123;
		}

		[TestMethod]
		public void TestVirtualAllocator()
		{
			VirtualMemoryAllocator allocator = new VirtualMemoryAllocator();

			using IMemoryOwner<byte> handle = allocator.Alloc(1024, null);
			handle.Memory.Span[0] = 123;
		}
	}
}
