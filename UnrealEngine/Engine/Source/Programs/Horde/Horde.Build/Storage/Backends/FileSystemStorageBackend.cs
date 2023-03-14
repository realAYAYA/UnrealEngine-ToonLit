// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Storage.Backends
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
	public class FileSystemStorageBackend : IStorageBackend
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
		public Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("FileSystemStorageBackend.ReadAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
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
		public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("FileSystemStorageBackend.WriteAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
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
			using IScope scope = GlobalTracer.Instance.BuildSpan("FileSystemStorageBackend.ExistsAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			FileReference location = FileReference.Combine(_baseDir, path);
			return Task.FromResult(FileReference.Exists(location));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("FileSystemStorageBackend.DeleteAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			FileReference location = FileReference.Combine(_baseDir, path);
			FileReference.Delete(location);
			return Task.CompletedTask;
		}
	}
}
