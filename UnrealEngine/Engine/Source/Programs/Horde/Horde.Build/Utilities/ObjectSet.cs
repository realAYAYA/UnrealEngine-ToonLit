// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Information about a blob stored in a blob pack file
	/// </summary>
	class ObjectPackEntry
	{
		public IoHash Hash { get; }
		public int Offset { get; }
		public int Length { get; }
		public IoHash[] Refs { get; }

		public ObjectPackEntry(IoHash hash, int offset, int length, IoHash[] refs)
		{
			Hash = hash;
			Offset = offset;
			Length = length;
			Refs = refs;
		}
	}

	/// <summary>
	/// Index for a blob pack
	/// </summary>
	[CbConverter(typeof(ObjectPackIndexConverter))]
	class ObjectPackIndex
	{
		public DateTime Time { get; }
		public ObjectPackEntry[] Blobs { get; }
		public IoHash DataHash { get; }
		public int _dataSize;

		readonly Dictionary<IoHash, ObjectPackEntry> _hashToInfo;

		public ObjectPackIndex(DateTime time, ObjectPackEntry[] blobs, IoHash dataHash, int dataSize)
		{
			Time = time;
			Blobs = blobs;
			DataHash = dataHash;
			_dataSize = dataSize;

			_hashToInfo = blobs.ToDictionary(x => x.Hash, x => x);
		}

		public bool Contains(IoHash hash) => _hashToInfo.ContainsKey(hash);

		public bool TryGetEntry(IoHash hash, [NotNullWhen(true)] out ObjectPackEntry? blobInfo) => _hashToInfo.TryGetValue(hash, out blobInfo);
	}

	/// <summary>
	/// Converter for BlobPackIndex objects
	/// </summary>
	class ObjectPackIndexConverter : CbConverterBase<ObjectPackIndex>
	{
		class EncodeFormat
		{
			[CbField("time")]
			public DateTime Time { get; set; }

			[CbField("exports")]
			public IoHash[]? Exports { get; set; }

			[CbField("lengths")]
			public int[]? Lengths { get; set; }

			[CbField("refs")]
			public IoHash[][]? Refs { get; set; }

			[CbField("data")]
			public CbBinaryAttachment DataHash { get; set; }

			[CbField("size")]
			public int DataSize { get; set; }
		}

		/// <inheritdoc/>
		public override ObjectPackIndex Read(CbField field)
		{
			EncodeFormat format = CbSerializer.Deserialize<EncodeFormat>(field);

			ObjectPackEntry[] objects = new ObjectPackEntry[format.Exports!.Length];

			int offset = 0;
			for (int idx = 0; idx < format.Exports.Length; idx++)
			{
				objects[idx] = new ObjectPackEntry(format.Exports[idx], offset, format.Lengths![idx], format.Refs![idx]);
				offset += format.Lengths[idx];
			}

			return new ObjectPackIndex(format.Time, objects, format.DataHash, format.DataSize);
		}

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ObjectPackIndex index)
		{
			writer.BeginObject();
			WriteInternal(writer, index);
			writer.EndObject();
		}

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, ObjectPackIndex index)
		{
			writer.BeginObject(name);
			WriteInternal(writer, index);
			writer.EndObject();
		}

		static void WriteInternal(CbWriter writer, ObjectPackIndex index)
		{
			EncodeFormat format = new EncodeFormat();
			format.Time = index.Time;
			format.Exports = index.Blobs.ConvertAll(x => x.Hash).ToArray();
			format.Lengths = index.Blobs.ConvertAll(x => x.Length).ToArray();
			format.Refs = index.Blobs.ConvertAll(x => x.Refs).ToArray();
			format.DataHash = index.DataHash;
			format.DataSize = index._dataSize;
			CbSerializer.Serialize(writer, format);
		}
	}

	/// <summary>
	/// Helper class to maintain a set of small objects, re-packing blobs according to a heuristic to balance download performance with churn.
	/// </summary>
	class ObjectSet
	{
		readonly IStorageClient _storageClient;
		readonly NamespaceId _namespaceId;
		public int MaxPackSize { get; }

		public HashSet<IoHash> RootSet { get; set; } = new HashSet<IoHash>();

		DateTime _time;

		int _nextPackSize;
		byte[] _nextPackData;
		readonly List<ObjectPackEntry> _nextPackEntries = new List<ObjectPackEntry>();
		readonly Dictionary<IoHash, ObjectPackEntry> _nextPackHashToEntry = new Dictionary<IoHash, ObjectPackEntry>();

		public List<ObjectPackIndex> PackIndexes { get; } = new List<ObjectPackIndex>();
		readonly List<Task> _writeTasks = new List<Task>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="storageClient"></param>
		/// <param name="namespaceId"></param>
		/// <param name="maxPackSize"></param>
		/// <param name="time">The initial update time; used to determine the age of blobs</param>
		public ObjectSet(IStorageClient storageClient, NamespaceId namespaceId, int maxPackSize, DateTime time)
		{
			_storageClient = storageClient;
			_namespaceId = namespaceId;
			MaxPackSize = maxPackSize;

			_nextPackData = null!;

			SetTime(time);
			Reset();
		}

		/// <summary>
		/// Reset the current state of the next blob
		/// </summary>
		void Reset()
		{
			_nextPackSize = 0;
			_nextPackData = new byte[MaxPackSize];
			_nextPackEntries.Clear();
			_nextPackHashToEntry.Clear();
		}

		/// <summary>
		/// Reset the current timestamp
		/// </summary>
		/// <param name="time">The new timestamp</param>
		public void SetTime(DateTime time)
		{
			_time = time;
		}

		/// <summary>
		/// Copies an existing entry into storage
		/// </summary>
		/// <param name="hash">Hash of the data</param>
		/// <param name="data">The data buffer</param>
		/// <param name="refs">References to other objects</param>
		public void Add(IoHash hash, ReadOnlySpan<byte> data, ReadOnlySpan<IoHash> refs)
		{
			if (!_nextPackHashToEntry.ContainsKey(hash))
			{
				// Create enough space for the new data
				CreateSpace(data.Length);

				// Copy the data into the buffer
				data.CopyTo(_nextPackData.AsSpan(_nextPackSize));

				// Add the blob
				ObjectPackEntry entry = new ObjectPackEntry(hash, _nextPackSize, data.Length, refs.ToArray());
				_nextPackHashToEntry.Add(hash, entry);
				_nextPackEntries.Add(entry);
				_nextPackSize += data.Length;
			}
		}

		/// <summary>
		/// Adds an item to the packer
		/// </summary>
		/// <param name="size">Size of the data</param>
		/// <param name="readData">Delegate to copy the data into a span</param>
		/// <param name="refs">References to other objects</param>
		public IoHash Add(int size, Action<Memory<byte>> readData, IoHash[] refs)
		{
			// Get the last blob and make sure there's enough space in it
			CreateSpace(size);

			// Copy the data into the new blob
			Memory<byte> output = _nextPackData.AsMemory(_nextPackSize, size);
			readData(output);

			// Update the metadata for it
			IoHash hash = IoHash.Compute(output.Span);
			if (!_nextPackHashToEntry.ContainsKey(hash))
			{
				ObjectPackEntry entry = new ObjectPackEntry(hash, _nextPackSize, size, refs);
				_nextPackHashToEntry.Add(hash, entry);
				_nextPackEntries.Add(entry);
				_nextPackSize += size;
			}
			return hash;
		}

		/// <summary>
		/// Finds data for an object with the given hash, from the current pack files
		/// </summary>
		/// <param name="hash"></param>
		/// <returns></returns>
		public async Task<ReadOnlyMemory<byte>> GetObjectDataAsync(IoHash hash)
		{
			await Task.WhenAll(_writeTasks);

			ObjectPackEntry? entry;
			if (_nextPackHashToEntry.TryGetValue(hash, out entry))
			{
				return _nextPackData.AsMemory(entry.Offset, entry.Length);
			}

			foreach (ObjectPackIndex pack in PackIndexes)
			{
				if (pack.TryGetEntry(hash, out entry))
				{
					ReadOnlyMemory<byte> packData = await _storageClient.ReadBlobToMemoryAsync(_namespaceId, pack.DataHash);
					return packData.Slice(entry.Offset, entry.Length);
				}
			}

			return default;
		}

		/// <summary>
		/// Tries to find an entry for the given hash from the current set of pack files
		/// </summary>
		/// <param name="hash">Hash of the </param>
		/// <param name="entry"></param>
		/// <returns></returns>
		public bool TryGetEntry(IoHash hash, [NotNullWhen(true)] out ObjectPackEntry? entry)
		{
			ObjectPackEntry? localEntry;
			if (_nextPackHashToEntry.TryGetValue(hash, out localEntry))
			{
				entry = localEntry;
				return true;
			}

			foreach (ObjectPackIndex pack in PackIndexes)
			{
				if (pack.TryGetEntry(hash, out localEntry))
				{
					entry = localEntry;
					return true;
				}
			}

			entry = null;
			return false;
		}

		/// <summary>
		/// Flush any pending blobs to disk
		/// </summary>
		public async Task FlushAsync()
		{
			// Find the live set of objects
			HashSet<IoHash> liveSet = new HashSet<IoHash>();
			foreach(IoHash rootHash in RootSet)
			{
				FindLiveSet(rootHash, liveSet);
			}

			// Find the total cost of all the current blobs, then loop through the blobs trying to find a more optimal arrangement
			double totalCost = PackIndexes.Sum(x => GetCostHeuristic(x)) + GetCostHeuristic(_nextPackSize, TimeSpan.Zero);
			for (; ; )
			{
				// Exclude any objects that are in the pending blobs, since we will always upload these
				HashSet<IoHash> newLiveSet = new HashSet<IoHash>(liveSet);
				newLiveSet.ExceptWith(_nextPackHashToEntry.Values.Select(x => x.Hash));

				// Get the size and cost of the next blob
				double nextBlobCost = GetCostHeuristic(_nextPackSize, TimeSpan.Zero);

				// Pass through all the blobs to find the best one to merge in
				double mergeCost = totalCost;
				ObjectPackIndex? mergePack = null;
				for (int idx = PackIndexes.Count - 1; idx >= 0; idx--)
				{
					ObjectPackIndex packIndex = PackIndexes[idx];

					// Try to merge any old blobs with the next blob
					if (packIndex.Time < _time)
					{
						// Calculate the cost of the last blob if we merge this one with it. We remove blobs as we iterate
						// through the list, since subsequent blobs will not usefully contribute the same items.
						double newTotalCost = totalCost - GetCostHeuristic(packIndex) - nextBlobCost;

						int newNextPackSize = _nextPackSize;
						foreach (ObjectPackEntry entry in packIndex.Blobs)
						{
							if (liveSet.Contains(entry.Hash))
							{
								if (newNextPackSize + entry.Length > MaxPackSize)
								{
									newTotalCost += GetCostHeuristic(newNextPackSize, TimeSpan.Zero);
									newNextPackSize = 0;
								}
								newNextPackSize += entry.Length;
							}
						}

						newTotalCost += GetCostHeuristic(newNextPackSize, TimeSpan.Zero);

						// Compute the potential cost if we replace the partial blob with the useful parts of this blob
						if (newTotalCost < mergeCost)
						{
							mergePack = packIndex;
							mergeCost = newTotalCost;
						}
					}

					// Remove any items in this blob from the remaining live set. No other blobs need to include them.
					newLiveSet.ExceptWith(packIndex.Blobs.Select(x => x.Hash));
				}

				// Bail out if we didn't find anything to merge
				if (mergePack == null)
				{
					break;
				}

				// Get the data for this blob
				ReadOnlyMemory<byte> mergeData = await _storageClient.ReadBlobToMemoryAsync(_namespaceId, mergePack.DataHash);

				// Add anything that's still part of the live set into the new blobs
				int offset = 0;
				foreach (ObjectPackEntry blob in mergePack.Blobs)
				{
					if (liveSet.Contains(blob.Hash))
					{
						ReadOnlyMemory<byte> data = mergeData.Slice(offset, blob.Length);
						Add(blob.Hash, data.Span, blob.Refs);
					}
					offset += blob.Length;
				}

				// Discard the old blob
				PackIndexes.Remove(mergePack);
				totalCost = mergeCost;
			}

			// Write the current blob
			FlushCurrentPack();

			// Wait for all the writes to finish
			await Task.WhenAll(_writeTasks);
			_writeTasks.Clear();
		}

		/// <summary>
		/// Finds the live set for a particular tree, and updates tree entries with the size of used items within them
		/// </summary>
		/// <param name="hash"></param>
		/// <param name="liveSet"></param>
		void FindLiveSet(IoHash hash, HashSet<IoHash> liveSet)
		{
			if (liveSet.Add(hash))
			{
				ObjectPackEntry? entry;
				if (!TryGetEntry(hash, out entry))
				{
					throw new Exception($"Missing blob {hash} from working set");
				}
				foreach (IoHash refHash in entry.Refs)
				{
					FindLiveSet(refHash, liveSet);
				}
			}
		}

		/// <summary>
		/// Creates enough space to store the given block of data
		/// </summary>
		/// <param name="size"></param>
		void CreateSpace(int size)
		{
			// Get the last blob and make sure there's enough space in it
			if (_nextPackSize + size > MaxPackSize)
			{
				FlushCurrentPack();
			}

			// Resize the next blob buffer if necessary
			if (size > _nextPackData.Length)
			{
				Array.Resize(ref _nextPackData, size);
			}
		}

		/// <summary>
		/// Finalize the current blob and start writing it to storage
		/// </summary>
		void FlushCurrentPack()
		{
			if (_nextPackSize > 0)
			{
				// Write the buffer to storage
				Array.Resize(ref _nextPackData, _nextPackSize);
				ReadOnlyMemory<byte> data = _nextPackData;
				IoHash dataHash = IoHash.Compute(data.Span);
				_writeTasks.Add(Task.Run(() => _storageClient.WriteBlobFromMemoryAsync(_namespaceId, dataHash, data)));

				// Create the new index
				ObjectPackIndex index = new ObjectPackIndex(_time, _nextPackEntries.ToArray(), dataHash, _nextPackSize);
				PackIndexes.Add(index);

				// Clear the next pack buffer
				Reset();
			}
		}

		/// <inheritdoc cref="GetCostHeuristic(Int32, TimeSpan)"/>
		/// <param name="index">Index to calculate the heuristic for</param>
		public double GetCostHeuristic(ObjectPackIndex index) => GetCostHeuristic(index.Blobs.Length, _time - index.Time);

		/// <summary>
		/// Heuristic which estimates the cost of a particular blob. This is used to compare scenarios of merging blobs to reduce download
		/// size against keeping older blobs which a lot of agents already have.
		/// </summary>
		/// <param name="size">Size of the blob</param>
		/// <param name="age">Age of the blob</param>
		/// <returns>Heuristic for the cost of a blob</returns>
		public static double GetCostHeuristic(int size, TimeSpan age)
		{
			// Time overhead to starting a download
			const double DownloadInit = 0.1;

			// Download speed for agents, in bytes/sec
			const double DownloadRate = 1024 * 1024;

			// Probability of an agent having to download everything. Prevents bias against keeping a large number of files.
			const double CleanSyncProbability = 0.2;

			// Average length of time between agents having to update
			TimeSpan averageCoherence = TimeSpan.FromHours(4.0);

			// Scale the age into a -1.0 -> 1.0 range around AverageCoherence
			double scaledAge = (averageCoherence - age).TotalSeconds / averageCoherence.TotalSeconds;

			// Get the probability of agents having to sync this blob based on its age. This is modeled as a logistic function (1 / (1 + e^-x))
			// with value of 0.5 at AverageCoherence, and MaxInterval at zero.

			// Find the scale factor for the 95% interval
			//    1 / (1 + e^-x) = MaxInterval
			//    e^-x = (1 / MaxInterval) - 1
			//    x = -ln((1 / MaxInterval) - 1)
			const double MaxInterval = 0.95;
			double sigmoidScale = -Math.Log((1.0 / MaxInterval) - 1.0);

			// Find the probability of having to sync this 
			double param = scaledAge * sigmoidScale;
			double probability = 1.0 / (1.0 + Math.Exp(-param));

			// Scale the probability against having to do a full sync
			probability = CleanSyncProbability + (probability * (1.0 - CleanSyncProbability));

			// Compute the final cost estimate; the amount of time we expect agents to spend downloading the file
			return probability * (DownloadInit + (size / DownloadRate));
		}
	}
}
