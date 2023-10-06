// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using EpicGames.Core;

namespace UnrealBuildTool.Storage
{
	/// <summary>
	/// Interface for reads from content-addressible storage
	/// </summary>
	interface IStorageReader : IDisposable
	{
		/// <summary>
		/// Accessor for the read stream
		/// </summary>
		Stream? Stream { get; }

		/// <summary>
		/// Whether the reader contains any data
		/// </summary>
		bool IsValid { get; }
	}

	/// <summary>
	/// Interface for writes to content-addressible storage
	/// </summary>
	interface IStorageWriter : IDisposable
	{
		/// <summary>
		/// Accessor for the write stream
		/// </summary>
		Stream? Stream { get; }

		/// <summary>
		/// Commits the written data to the cache
		/// </summary>
		void Commit();
	}

	/// <summary>
	/// Interface for an artifact cache
	/// </summary>
	interface IStorageProvider
	{
		/// <summary>
		/// Attempts to open a file from the output cache
		/// </summary>
		/// <param name="Digest">Digest of the item to retrieve</param>
		/// <returns>True if the item exists in the cache, false otherwise</returns>
		IStorageReader CreateReader(ContentHash Digest);

		/// <summary>
		/// Opens a stream for writing into the cache. The digest 
		/// </summary>
		/// <returns></returns>
		IStorageWriter CreateWriter(ContentHash Digest);
	}
}
