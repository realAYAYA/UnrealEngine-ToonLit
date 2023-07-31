// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading;
using System.Threading.Tasks;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Storage.Backends
{
	/// <summary>
	/// Options for the relay storage backend
	/// </summary>
	public interface IRelayStorageOptions : IFileSystemStorageOptions
	{
		/// <summary>
		/// Remote Horde server to use for storage using the relay storage backend
		/// </summary>
		public string? RelayServer { get; }

		/// <summary>
		/// Authentication token for using a relay server
		/// </summary>
		public string? RelayToken { get; }
	}

	/// <summary>
	/// Implementation of ILogFileStorage which forwards requests to another server
	/// </summary>
	public sealed class RelayStorageBackend : IStorageBackend, IDisposable
	{
		/// <summary>
		/// The client to connect with
		/// </summary>
		readonly HttpClient _client;

		/// <summary>
		/// The base server URL
		/// </summary>
		readonly Uri _serverUrl;

		/// <summary>
		/// Local storage provider
		/// </summary>
		readonly FileSystemStorageBackend _localStorage;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="options">Settings for the server instance</param>
		public RelayStorageBackend(IRelayStorageOptions options)
		{
			if (options.RelayServer == null)
			{
				throw new InvalidDataException($"Missing {nameof(IRelayStorageOptions.RelayServer)} in server configuration");
			}
			if (options.RelayToken == null)
			{
				throw new InvalidDataException($"Missing {nameof(IRelayStorageOptions.RelayToken)} in server configuration");
			}

			_serverUrl = new Uri(options.RelayServer);

			_client = new HttpClient();
			_client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", options.RelayToken);

			_localStorage = new FileSystemStorageBackend(options);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_client.Dispose();
		}

		/// <inheritdoc/>
		public async Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("RelayStorageBackend.ReadAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			Stream? localResult = await _localStorage.ReadAsync(path, cancellationToken);
			if (localResult != null)
			{
				return localResult;
			}

			Uri url = new Uri(_serverUrl, $"api/v1/debug/storage?path={path}");
			using (HttpResponseMessage response = await _client.GetAsync(url, cancellationToken))
			{
				if (response.IsSuccessStatusCode)
				{
					byte[] responseData = await response.Content.ReadAsByteArrayAsync(cancellationToken);
					await _localStorage.WriteBytesAsync(path, responseData, cancellationToken);
					return new MemoryStream(responseData);
				}
			}

			return null;
		}

		/// <inheritdoc/>
		public Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("RelayStorageBackend.WriteAsync").StartActive();
			scope.Span.SetTag("Path", path);

			return _localStorage.WriteAsync(path, stream, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}
	}
}
