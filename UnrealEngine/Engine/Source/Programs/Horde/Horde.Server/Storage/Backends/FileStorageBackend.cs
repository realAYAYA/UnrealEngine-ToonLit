// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Utilities;
using OpenTelemetry.Trace;

namespace Horde.Server.Storage.Backends
{
	/// <summary>
	/// Options for the filesystem backend
	/// </summary>
	public interface IFileSystemStorageOptions
	{
		/// <summary>
		/// Base directory for storing files
		/// </summary>
		public string? BaseDir { get; }
	}

	/// <summary>
	/// Storage backend using the filesystem
	/// </summary>
	public sealed class FileSystemStorageBackend : IStorageBackend
	{
		/// <summary>
		/// Base directory for log files
		/// </summary>
		private readonly DirectoryReference _baseDir;

		/// <summary>
		/// Unique identifier for this instance
		/// </summary>
		private readonly string _instanceId;

		/// <summary>
		/// Unique id for each write
		/// </summary>
		private int _uniqueId;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="options">Current Horde Settings</param>
		public FileSystemStorageBackend(IFileSystemStorageOptions options)
		{
			_baseDir = DirectoryReference.Combine(Program.DataDir, options.BaseDir ?? "Storage");
			_instanceId = Guid.NewGuid().ToString("N");
			DirectoryReference.CreateDirectory(_baseDir);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<Stream?> TryReadAsync(string path, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(FileSystemStorageBackend)}.{nameof(TryReadAsync)}");
			span.SetAttribute("path", path);
			
			FileReference location = FileReference.Combine(_baseDir, path);
			if (!FileReference.Exists(location))
			{
				return Task.FromResult<Stream?>(null);
			}

			try
			{
				return Task.FromResult<Stream?>(FileReference.Open(location, FileMode.Open, FileAccess.Read, FileShare.Read));
			}
			catch (DirectoryNotFoundException)
			{
				return Task.FromResult<Stream?>(null);
			}
			catch (FileNotFoundException)
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
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(FileSystemStorageBackend)}.{nameof(WriteAsync)}");
			span.SetAttribute("path", path);
			
			FileReference finalLocation = FileReference.Combine(_baseDir, path);
			if (!FileReference.Exists(finalLocation))
			{
				// Write to a temp file first
				int newUniqueId = Interlocked.Increment(ref _uniqueId);

				DirectoryReference.CreateDirectory(finalLocation.Directory);
				FileReference tempLocation = new FileReference($"{finalLocation}.{_instanceId}.{newUniqueId:x8}");

				using (Stream outputStream = FileReference.Open(tempLocation, FileMode.Create, FileAccess.Write, FileShare.Read))
				{
					await stream.CopyToAsync(outputStream, cancellationToken);
				}

				// Move the temp file into place
				try
				{
					FileReference.Move(tempLocation, finalLocation, true);
				}
				catch (IOException) // Already exists
				{
					if (FileReference.Exists(finalLocation))
					{
						FileReference.Delete(tempLocation);
					}
					else
					{
						throw;
					}
				}
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(FileSystemStorageBackend)}.{nameof(ExistsAsync)}");
			span.SetAttribute("path", path);
			
			FileReference location = FileReference.Combine(_baseDir, path);
			return Task.FromResult(FileReference.Exists(location));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(FileSystemStorageBackend)}.{nameof(DeleteAsync)}");
			span.SetAttribute("path", path);
			
			FileReference location = FileReference.Combine(_baseDir, path);
			FileReference.Delete(location);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			Stack<IEnumerator<DirectoryInfo>> queue = new Stack<IEnumerator<DirectoryInfo>>();
			try
			{
				queue.Push(new List<DirectoryInfo> { _baseDir.ToDirectoryInfo() }.GetEnumerator());
				while (queue.Count > 0)
				{
					IEnumerator<DirectoryInfo> top = queue.Peek();
					if (!top.MoveNext())
					{
						top.Dispose();
						queue.Pop();
						continue;
					}

					DirectoryInfo current = top.Current;
					foreach (FileInfo fileInfo in current.EnumerateFiles("*"))
					{
						string path = fileInfo.FullName.Substring(_baseDir.FullName.Length + 1).Replace(Path.DirectorySeparatorChar, '/');
						yield return path;
					}

					queue.Push(current.EnumerateDirectories().GetEnumerator());

					cancellationToken.ThrowIfCancellationRequested();
					await Task.Yield();
				}
			}
			finally
			{
				while (queue.TryPop(out IEnumerator<DirectoryInfo>? enumerator))
				{
					enumerator.Dispose();
				}
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default) => default;
	}
}
