// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Build.Storage.Backends
{
	/// <summary>
	/// In-memory implementation of ILogFileStorage
	/// </summary>
	public class TransientStorageBackend : IStorageBackend
	{
		/// <summary>
		/// Data storage
		/// </summary>
		readonly ConcurrentDictionary<string, byte[]> _pathToData = new ConcurrentDictionary<string, byte[]>();

		/// <inheritdoc/>
		public Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken)
		{
			byte[]? data;
			if (_pathToData.TryGetValue(path, out data))
			{
				return Task.FromResult<Stream?>(new MemoryStream(data, false));
			}
			else
			{
				return Task.FromResult<Stream?>(null);
			}
		}

		/// <inheritdoc/>
		public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer);
				_pathToData[path] = buffer.ToArray();
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			return Task.FromResult(_pathToData.ContainsKey(path));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			_pathToData.TryRemove(path, out _);
			return Task.CompletedTask;
		}
	}
}
