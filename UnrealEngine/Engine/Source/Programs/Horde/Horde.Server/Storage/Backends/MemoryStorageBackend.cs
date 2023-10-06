// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Server.Storage.Backends
{
	/// <summary>
	/// In-memory implementation of ILogFileStorage
	/// </summary>
	public sealed class MemoryStorageBackend : IStorageBackend
	{
		/// <summary>
		/// Data storage
		/// </summary>
		readonly ConcurrentDictionary<string, byte[]> _pathToData = new ConcurrentDictionary<string, byte[]>();

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<Stream?> TryReadAsync(string path, CancellationToken cancellationToken)
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
		public async Task<Stream?> TryReadAsync(string path, int offset, int length, CancellationToken cancellationToken)
		{
			Stream? stream = await TryReadAsync(path, cancellationToken);
			if (stream != null)
			{
				stream.Seek(offset, SeekOrigin.Begin);
			}
			return stream;
		}

		/// <inheritdoc/>
		public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
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

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			foreach (string path in _pathToData.Keys)
			{
				yield return path;
				cancellationToken.ThrowIfCancellationRequested();
				await Task.Yield();
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default) => default;
	}
}
