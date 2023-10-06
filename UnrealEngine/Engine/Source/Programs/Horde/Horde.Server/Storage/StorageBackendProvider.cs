// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Storage.Backends;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Default implementation of <see cref="IStorageBackendProvider"/>
	/// </summary>
	class StorageBackendProvider : IStorageBackendProvider
	{
		class RefCountedBackend : IDisposable
		{
			public IoHash Hash { get; }
			public IStorageBackend Backend { get; }

			public int _refCount = 1;

			public RefCountedBackend(IoHash hash, IStorageBackend backend)
			{
				Hash = hash;
				Backend = backend;
			}

			public void Dispose()
			{
				Backend.Dispose();
			}
		}

		class BackendWrapper : IStorageBackend
		{
			readonly StorageBackendProvider _owner;
			RefCountedBackend _refCountedBackend;
			IStorageBackend _backend;

			public BackendWrapper(StorageBackendProvider owner, RefCountedBackend refCountedBackend)
			{
				_owner = owner;
				_refCountedBackend = refCountedBackend;
				_backend = _refCountedBackend.Backend;
			}

			public void Dispose()
			{
				if (_refCountedBackend != null)
				{
					_owner.ReleaseBackend(_refCountedBackend);
					_refCountedBackend = null!;

					_backend = null!;
				}
			}

			#region IStorageBackend Implementation

			/// <inheritdoc/>
			public bool SupportsRedirects => _backend.SupportsRedirects;

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _backend.ExistsAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _backend.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _backend.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public Task<Stream?> TryReadAsync(string path, CancellationToken cancellationToken = default) => _backend.TryReadAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<Stream?> TryReadAsync(string path, int offset, int length, CancellationToken cancellationToken = default) => _backend.TryReadAsync(path, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken = default) => _backend.WriteAsync(path, stream, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _backend.TryGetReadRedirectAsync(path, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default) => _backend.TryGetWriteRedirectAsync(path, cancellationToken);

			#endregion
		}

		readonly IServiceProvider _serviceProvider;
		readonly object _lockObject = new object();
		readonly Dictionary<IoHash, RefCountedBackend> _backends = new Dictionary<IoHash, RefCountedBackend>();

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageBackendProvider(IServiceProvider serviceProvider)
		{
			_serviceProvider = serviceProvider;
		}

		/// <inheritdoc/>
		public IStorageBackend CreateBackend(BackendConfig config)
		{
			// Compute the new hash of the configuration data
			IoHash hash;
			using (MemoryStream stream = new MemoryStream())
			{
				JsonSerializerOptions options = new JsonSerializerOptions();
				Startup.ConfigureJsonSerializer(options);

				JsonSerializer.Serialize(stream, config, options: options);

				hash = IoHash.Compute(stream.ToArray());
			}

			// See if we've got an existing backend we can use
			RefCountedBackend? refCountedBackend;
			lock (_lockObject)
			{
				if (_backends.TryGetValue(hash, out refCountedBackend))
				{
					refCountedBackend._refCount++;
				}
				else
				{
					IStorageBackend newBackend = CreateStorageBackend(config);
					refCountedBackend = new RefCountedBackend(hash, newBackend);
					_backends.Add(hash, refCountedBackend);
				}
			}

			return new BackendWrapper(this, refCountedBackend);
		}

		void ReleaseBackend(RefCountedBackend backend)
		{
			lock (_lockObject)
			{
				if (--backend._refCount == 0)
				{
					_backends.Remove(backend.Hash);
					backend.Backend.Dispose();
				}
			}
		}

		/// <summary>
		/// Creates a storage backend with the given configuration
		/// </summary>
		/// <param name="config">Configuration for the backend</param>
		/// <returns>New storage backend instance</returns>
		IStorageBackend CreateStorageBackend(BackendConfig config)
		{
			switch (config.Type ?? StorageBackendType.FileSystem)
			{
				case StorageBackendType.FileSystem:
					return new FileSystemStorageBackend(config);
				case StorageBackendType.Aws:
					return new AwsStorageBackend(_serviceProvider.GetRequiredService<IConfiguration>(), config, _serviceProvider.GetRequiredService<ILogger<AwsStorageBackend>>());
				case StorageBackendType.Memory:
					return new MemoryStorageBackend();
				default:
					throw new NotImplementedException();
			}
		}
	}
}
