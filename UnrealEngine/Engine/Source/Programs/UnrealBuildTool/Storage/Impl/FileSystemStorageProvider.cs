// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool.Storage.Impl
{
	/// <summary>
	/// Storage provider which uses the filesystem
	/// </summary>
	class FileSystemStorageProvider : IStorageProvider
	{
		/// <summary>
		/// Cached copy of the current process id
		/// </summary>
		static int ProcessId = Process.GetCurrentProcess().Id;

		/// <summary>
		/// Implements <see cref="IStorageReader"/> for writes to the backing storage
		/// </summary>
		class StorageReader : IStorageReader
		{
			public Stream? Stream
			{
				get;
				private set;
			}

			public bool IsValid => Stream != null;

			public StorageReader(FileReference Location)
			{
				if (FileReference.Exists(Location))
				{
					Stream = FileReference.Open(Location, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete);
				}
			}

			public void Dispose()
			{
				if (Stream != null)
				{
					Stream.Dispose();
					Stream = null;
				}
			}
		}

		/// <summary>
		/// Represents a write transaction to the cache. Commit() should be called once the transaction is complete.
		/// </summary>
		class StorageWriter : IStorageWriter
		{
			FileReference? TempLocation;
			FileReference FinalLocation;

			public Stream? Stream
			{
				get;
				private set;
			}

			public StorageWriter(FileReference Location)
			{
				FinalLocation = Location;
				DirectoryReference.CreateDirectory(FinalLocation.Directory);

				TempLocation = new FileReference(String.Format("{0}.{1}", Location.FullName, ProcessId));
				Stream = FileReference.Open(TempLocation, FileMode.Create, FileAccess.Write, FileShare.Read | FileShare.Delete);
			}

			public void Commit()
			{
				if (TempLocation == null)
				{
					throw new InvalidOperationException("Item has already been committed");
				}

				Stream?.Close();

				try
				{
					FileReference.Move(TempLocation, FinalLocation);
				}
				catch
				{
					FileReference.Delete(TempLocation);
					TempLocation = null;
					throw;
				}

				TempLocation = null;
			}

			public void Dispose()
			{
				if (Stream != null)
				{
					Stream.Dispose();
					Stream = null;
				}
				if (TempLocation != null)
				{
					FileReference.Delete(TempLocation);
					TempLocation = null;
				}
			}
		}

		/// <summary>
		/// Attempts to open a file from the output cache
		/// </summary>
		/// <param name="Digest">Digest of the item to retrieve</param>
		/// <returns>Reader interface for the file</returns>
		public IStorageReader CreateReader(ContentHash Digest)
		{
			return new StorageReader(GetFileForDigest(Digest));
		}

		/// <summary>
		/// Opens a stream for writing into the cache. The digest 
		/// </summary>
		/// <returns></returns>
		public IStorageWriter CreateWriter(ContentHash Digest)
		{
			return new StorageWriter(GetFileForDigest(Digest));
		}

		/// <summary>
		/// Gets the filename on disk to use for a particular digest
		/// </summary>
		/// <param name="Digest">The digest to find a filename for</param>
		/// <returns>Filename to use for the given digest</returns>
		static FileReference GetFileForDigest(ContentHash Digest)
		{
			string DigestText = Digest.ToString();
			return FileReference.Combine(Unreal.EngineDirectory, "Saved", "UnrealBuildTool", "Cache", String.Format("{0}/{1}/{2}/{3}.bin", DigestText[0], DigestText[1], DigestText[2], DigestText));
		}
	}
}
