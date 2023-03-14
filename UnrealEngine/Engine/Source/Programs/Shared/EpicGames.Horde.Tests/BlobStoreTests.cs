// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class BlobStoreTests
	{
		readonly FileReference _file;

		public BlobStoreTests()
		{
			_file = new FileReference("test.dat");
			FileReference.Delete(_file);
		}

		[TestMethod]
		public void EmptyObject()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());
			BlobHandle handle = store.AddRaw(Array.Empty<byte>());

			Memory<byte> data = store.GetData(handle.FirstCell);
			Assert.AreEqual(0, data.Length);
		}

		static byte[] MakeBlockWithIndex(int size, int index)
		{
			byte[] data = new byte[size];
			BinaryPrimitives.WriteInt32LittleEndian(data, index);
			return data;
		}

		[TestMethod]
		public void LargeObjectPacking()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());

			long baseNumCellBytes = store.NumAllocatedCellBytes;
			long baseNumPageBytes = store.NumAllocatedPageBytes;

			store.AddRaw(MakeBlockWithIndex(1000, 1));
			Assert.AreEqual(1, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 1024, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(1000, 2));
			Assert.AreEqual(2, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 2048, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(1000, 3));
			Assert.AreEqual(3, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 3072, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(1000, 4));
			Assert.AreEqual(4, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 4096, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(1000, 5));
			Assert.AreEqual(5, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 5120, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 8192, store.NumAllocatedPageBytes);
		}

		[TestMethod]
		public void SmallObjectPacking()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());

			long baseNumCellBytes = store.NumAllocatedCellBytes;
			long baseNumPageBytes = store.NumAllocatedPageBytes;

			store.AddRaw(MakeBlockWithIndex(64, 1));
			Assert.AreEqual(1, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 64, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(64, 2));
			Assert.AreEqual(2, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 128, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);

			store.AddRaw(MakeBlockWithIndex(64, 3));
			Assert.AreEqual(3, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 192, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096, store.NumAllocatedPageBytes);
		}

		[TestMethod]
		public void FillItems()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());
			for (int idx = 0; idx < 2048; idx++)
			{
				byte[] data = MakeBlockWithIndex(64, idx);
				store.AddRaw(data);
			}
			Assert.AreEqual(2048, store.NumItems);
		}

		[TestMethod]
		public void HugeObjects()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());

			long baseNumCellBytes = store.NumAllocatedCellBytes;
			long baseNumPageBytes = store.NumAllocatedPageBytes;

			byte[] data = new byte[4096 + 4096 + 2000];

			Random random = new Random(0);
			random.NextBytes(data);

			BlobHandle handle = store.AddRaw(data);
			Assert.AreEqual(data.Length, store.GetLength(handle));
			Assert.AreEqual(1, store.NumItems);
			Assert.AreEqual(baseNumCellBytes + 4096 + 4096 + 2048, store.NumAllocatedCellBytes);
			Assert.AreEqual(baseNumPageBytes + 4096 + 4096 + 4096, store.NumAllocatedPageBytes);

			byte[] storedData = new byte[store.GetLength(handle)];
			int copiedDataLength = store.ReadData(handle, storedData);

			Assert.AreEqual(storedData.Length, copiedDataLength);
			Assert.IsTrue(storedData.AsSpan().SequenceEqual(data));
		}

		[TestMethod]
		public void Resize()
		{
			FileInfo file1 = new FileInfo(_file.FullName); // header
			file1.Delete();

			FileInfo file2 = new FileInfo($"{_file}.2.tmp"); // obj1
			file2.Delete();

			FileInfo file3 = new FileInfo($"{_file}.3.tmp"); // index
			file3.Delete();

			FileInfo file4 = new FileInfo($"{_file}.4.tmp");
			file4.Delete();

			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions { ReservedSize = 4096, ExpandStepSize = 4096 });

			ObjectHandle obj1 = store.AddObject(IoHash.Zero, new List<ObjectHandle>(), new byte[4096]);

			file1.Refresh(); // header
			Assert.IsTrue(file1.Exists);
			Assert.AreEqual(4096, file1.Length);

			file2.Refresh(); // obj1
			Assert.IsTrue(file2.Exists);
			Assert.AreEqual(4096, file2.Length);

			file3.Refresh(); // obj1 refs
			Assert.IsTrue(file3.Exists);
			Assert.AreEqual(4096, file3.Length);

			file4.Refresh();
			Assert.IsFalse(file4.Exists);

			store.Save();

			file1.Refresh();
			Assert.AreEqual(4096 * 4, file1.Length);

			file2.Refresh();
			Assert.IsFalse(file2.Exists);

			file3.Refresh();
			Assert.IsFalse(file3.Exists);
		}

		[TestMethod]
		public void RefCounts()
		{
			using LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions());

			ObjectHandle obj1 = store.AddObject(IoHash.Zero, new List<ObjectHandle>(), new byte[] { 1, 2, 3 });
			ObjectHandle obj2 = store.AddObject(IoHash.Zero, new List<ObjectHandle> { obj1 }, new byte[] { 4, 5, 6, 7 });

			Assert.AreEqual(2, store.NumItems);
			Assert.AreEqual(7, store.NumAllocatedItemBytes);
			Assert.AreEqual(64 * 4, store.NumAllocatedCellBytes);
			Assert.AreEqual(4096, store.NumAllocatedPageBytes);

			store.AddRef(obj1);
			store.ReleaseRef(obj2);
			store.Save();

			Assert.AreEqual(1, store.NumItems);
			Assert.AreEqual(3, store.NumAllocatedItemBytes);
			Assert.AreEqual(64 * 2, store.NumAllocatedCellBytes);
			Assert.AreEqual(8192, store.NumAllocatedPageBytes); // index + data

			store.ReleaseRef(obj1);
			store.Save();

			Assert.AreEqual(0, store.NumItems);
			Assert.AreEqual(0, store.NumAllocatedItemBytes);
			Assert.AreEqual(0, store.NumAllocatedCellBytes);
			Assert.AreEqual(4096, store.NumAllocatedPageBytes); // index only
		}

		[TestMethod]
		public void BasicObjectLookup()
		{
			using (LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions { ObjectHashSize = 128 }))
			{
				Random random = new Random(0);

				byte[] data = new byte[random.Next(8192)];
				random.NextBytes(data);

				IoHash hash = IoHash.Compute(data);
				store.AddObject(hash, new List<ObjectHandle>(), data);

				ObjectHandle obj = store.FindObject(hash);
				BlobHandle blob = store.GetObjectData(obj);

				long length = store.GetLength(blob);
				Assert.AreEqual(data.Length, length);
			}
		}

		[TestMethod]
		public void ObjectLookup()
		{
			Dictionary<IoHash, byte[]> objects = new Dictionary<IoHash, byte[]>();
			using (LocalBlobStore store = LocalBlobStore.CreateNew(_file, new LocalBlobStoreOptions { ObjectHashSize = 128 }))
			{
				Random random = new Random(0);

				for (int idx = 0; idx < 1024; idx++)
				{
					byte[] data = new byte[random.Next(8192)];
					random.NextBytes(data);

					IoHash hash = IoHash.Compute(data);
					objects[hash] = data;

					store.AddObject(hash, new List<ObjectHandle>(), data);
				}

				store.Save();
			}

			using (LocalBlobStore store = LocalBlobStore.OpenExisting(_file, new LocalBlobStoreOptions { ObjectHashSize = 128 }))
			{
				foreach ((IoHash hash, byte[] data) in objects)
				{
					ObjectHandle obj = store.FindObject(hash);
					Assert.IsTrue(obj.IsValid());

					BlobHandle blob = store.GetObjectData(obj);
					Assert.AreEqual(data.Length, store.GetLength(blob));

					byte[] copy = new byte[data.Length];
					store.ReadData(blob, copy);

					Assert.IsTrue(data.AsSpan().SequenceEqual(copy));
				}
			}
		}

			/*
			[TestMethod]
			public void HugeObjects2()
			{
				const int NumItems = 5;
				for (int idx = 0; idx < NumItems; idx++)
				{
					byte[] data = MakeBlockWithIndex(2000 + (idx * 2048), idx);
					_store.Add(data);
				}

				Assert.AreEqual(NumItems, _store.NumItems);

				using (LruCache.View view = _store.LockView())
				{
					for (int idx = 0; idx < NumItems; idx++)
					{
						byte[] data = MakeBlockWithIndex(2000 + (idx * 2048), idx);
						IoHash hash = IoHash.Compute(data);
						ReadOnlyMemory<byte> otherData = view.Get(hash);
						Assert.IsTrue(data.AsSpan().SequenceEqual(otherData.Span));
					}
				}
			}

			[TestMethod]
			public async Task SmallObjectSerialization()
			{
				for (int idx = 0; idx < 1000; idx++)
				{
					byte[] data = MakeBlockWithIndex(4 + idx, idx);
					_store.Add(data);
				}

				CheckSmallObjects();

				Assert.AreEqual(1000, _store.NumItems);
				long numBytes = _store.NumBytes;
				long numBytesWithBlockSlack = _store.NumBytesWithBlockSlack;
				long numBytesWithPageSlack = _store.NumBytesWithPageSlack;

				await _store.SaveAsync();
				_store.Dispose();

				_store = await LruCache.OpenAsync(_indexFile, _dataFile);

				Assert.AreEqual(1000, _store.NumItems);
				Assert.AreEqual(numBytes, _store.NumBytes);
				Assert.AreEqual(numBytesWithBlockSlack, _store.NumBytesWithBlockSlack);
				Assert.AreEqual(numBytesWithPageSlack, _store.NumBytesWithPageSlack);

				CheckSmallObjects();
			}

			void CheckSmallObjects()
			{
				using (LruCache.View view = _store.LockView())
				{
					for (int idx = 0; idx < 1000; idx++)
					{
						byte[] data = MakeBlockWithIndex(4 + idx, idx);
						IoHash hash = IoHash.Compute(data);
						ReadOnlyMemory<byte> otherData = view.Get(hash);
						Assert.IsTrue(data.AsSpan().SequenceEqual(otherData.Span));
					}
				}
			}

			[TestMethod]
			public async Task LargeObjectSerialization()
			{
				const int NumItems = 10;
				for (int idx = 0; idx < NumItems; idx++)
				{
					byte[] data = MakeBlockWithIndex(2000 + (idx * 2048), idx);
					_store.Add(data);
				}

				CheckLargeObjects(NumItems);

				Assert.AreEqual(NumItems, _store.NumItems);
				long numBytes = _store.NumBytes;
				long numBytesWithBlockSlack = _store.NumBytesWithBlockSlack;
				long numBytesWithPageSlack = _store.NumBytesWithPageSlack;

				await _store.SaveAsync();
				_store.Dispose();

				_store = await LruCache.OpenAsync(_indexFile, _dataFile);

				Assert.AreEqual(NumItems, _store.NumItems);
				Assert.AreEqual(numBytes, _store.NumBytes);
				Assert.AreEqual(numBytesWithBlockSlack, _store.NumBytesWithBlockSlack);
				Assert.AreEqual(numBytesWithPageSlack, _store.NumBytesWithPageSlack);

				CheckLargeObjects(NumItems);
			}

			void CheckLargeObjects(int numItems)
			{
				using (LruCache.View view = _store.LockView())
				{
					for (int idx = 0; idx < numItems; idx++)
					{
						byte[] data = MakeBlockWithIndex(2000 + (idx * 2048), idx);
						IoHash hash = IoHash.Compute(data);
						ReadOnlyMemory<byte> otherData = view.Get(hash);
						Assert.IsTrue(data.AsSpan().SequenceEqual(otherData.Span));
					}
				}
			}

			[TestMethod]
			public async Task ExpireItems()
			{
				for (int idx = 0; idx < 1000; idx++)
				{
					byte[] data = MakeBlockWithIndex(4 + idx, idx);
					_store.Add(data);
				}

				long trimSize = _store.NumBytesWithBlockSlack;
				_store.NextGeneration();

				List<IoHash> testHashes = new List<IoHash>();
				for (int idx = 1000; idx < 2000; idx++)
				{
					byte[] data = MakeBlockWithIndex(4 + idx, idx);
					testHashes.Add(_store.Add(data));
				}

				long desiredSize = _store.NumBytesWithBlockSlack - trimSize;
				await _store.TrimAsync(desiredSize);

				Assert.AreEqual(desiredSize, _store.NumBytesWithBlockSlack);

				using (LruCache.View view = _store.LockView())
				{
					for (int idx = 0; idx < 100; idx++)
					{
						ReadOnlyMemory<byte> memory = view.Get(testHashes[idx]);
						Assert.IsFalse(memory.IsEmpty);
					}
				}
			}*/
		}
	}
