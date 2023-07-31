// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;

namespace Horde.Build.Logs.Storage
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Empty implementation of log storage
	/// </summary>
	public sealed class NullLogStorage : ILogStorage
	{
		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			return Task.FromResult<LogIndexData?>(null);
		}

		/// <inheritdoc/>
		public Task WriteIndexAsync(LogId logId, long length, LogIndexData index)
		{
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			return Task.FromResult<LogChunkData?>(null);
		}

		/// <inheritdoc/>
		public Task WriteChunkAsync(LogId logId, long offset, LogChunkData chunkData)
		{
			return Task.CompletedTask;
		}
	}
}
