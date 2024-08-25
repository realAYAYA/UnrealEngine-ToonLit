// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;
using Azure;
using Microsoft.Extensions.Options;
using Azure.Storage.Blobs;
using Azure.Storage.Blobs.Models;
using EpicGames.Horde.Storage;
using Jupiter.Common.Implementation;
using System.Threading;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;
using Azure.Storage.Sas;
using Jupiter.Common;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
	public class AzureBlobStore : IBlobStore
	{
		private readonly IOptionsMonitor<AzureSettings> _settings;
		private readonly ISecretResolver _secretResolver;
		private readonly INamespacePolicyResolver _namespacePolicyResolver;
		private readonly IServiceProvider _provider;
		private readonly ConcurrentDictionary<NamespaceId, AzureStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, AzureStorageBackend>();

		public AzureBlobStore(IOptionsMonitor<AzureSettings> settings, ISecretResolver secretResolver, INamespacePolicyResolver namespacePolicyResolver, IServiceProvider provider)
		{
			_settings = settings;
			_secretResolver = secretResolver;
			_namespacePolicyResolver = namespacePolicyResolver;
			_provider = provider;
		}

		private AzureStorageBackend GetBackend(NamespaceId ns)
		{
			return _backends.GetOrAdd(ns, x => ActivatorUtilities.CreateInstance<AzureStorageBackend>(_provider, GetConnectionString(ns), ns, GetContainerName(ns)));
		}

		private string GetContainerName(NamespaceId ns)
		{
			NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

			string containerName = !string.IsNullOrEmpty(policy.StoragePool) ? $"jupiter-{policy.StoragePool}" : "jupiter";
			if (_settings.CurrentValue.StoragePoolContainerOverride.TryGetValue(policy.StoragePool, out string? overriddenContainerName))
			{
				containerName = overriddenContainerName;
			}

			return SanitizeContainerName(containerName);
		}

		private string GetConnectionString(NamespaceId ns)
		{
			NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
			string connectionString = _settings.CurrentValue.ConnectionString;
			if (_settings.CurrentValue.StoragePoolConnectionStrings.TryGetValue(policy.StoragePool, out string? connectionStringForPool))
			{
				connectionString = connectionStringForPool;
			}

			string resolvedConnectionString = _secretResolver.Resolve(connectionString);
			return resolvedConnectionString;
		}

		private static string GetPath(BlobId blobIdentifier) => blobIdentifier.ToString();

		public async Task<Uri?> GetObjectByRedirectAsync(NamespaceId ns, BlobId identifier)
		{
			Uri? redirectUri = await GetBackend(ns).GetReadRedirectAsync(GetPath(identifier));

			return redirectUri;
		}

		public async Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId)
		{
			BlobMetadata? metadata = await GetBackend(ns).GetMetadataAsync(GetPath(blobId), CancellationToken.None);

			if (metadata == null)
			{
				throw new BlobNotFoundException(ns, blobId);
			}

			return metadata;
		}

		public async Task<Uri?> PutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier)
		{
			Uri? redirectUri = await GetBackend(ns).GetWriteRedirectAsync(GetPath(identifier));

			return redirectUri;
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, ReadOnlyMemory<byte> content, BlobId blobIdentifier)
		{
			// TODO: this is not ideal as we copy the buffer, but there is no upload from memory available so we would need this copy anyway
			await using MemoryStream stream = new MemoryStream(content.ToArray());
			return await PutObjectAsync(ns, stream, blobIdentifier);
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, Stream content, BlobId blobIdentifier)
		{
			await GetBackend(ns).WriteAsync(GetPath(blobIdentifier), content, CancellationToken.None);
			return blobIdentifier;
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] content, BlobId blobIdentifier)
		{
			await using MemoryStream stream = new MemoryStream(content);
			return await PutObjectAsync(ns, stream, blobIdentifier);
		}

		public async Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blobIdentifier, LastAccessTrackingFlags flags, bool supportsRedirectUri = false)
		{
			NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
			if (supportsRedirectUri && policies.AllowRedirectUris)
			{
				Uri? redirectUri = await GetBackend(ns).GetReadRedirectAsync(GetPath(blobIdentifier));
				if (redirectUri != null)
				{
					return new BlobContents(redirectUri);
				}
			}
			
			BlobContents? contents = await GetBackend(ns).TryReadAsync(GetPath(blobIdentifier), flags, CancellationToken.None);
			if (contents == null)
			{
				throw new BlobNotFoundException(ns, blobIdentifier);
			}
			return contents;
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BlobId blobIdentifier, bool forceCheck)
		{
			return await GetBackend(ns).ExistsAsync(GetPath(blobIdentifier), CancellationToken.None);
		}

		public async Task DeleteObjectAsync(NamespaceId ns, BlobId blobIdentifier)
		{
			await GetBackend(ns).DeleteAsync(GetPath(blobIdentifier), CancellationToken.None);
		}

		public async Task DeleteNamespaceAsync(NamespaceId ns)
		{
			string fixedNamespace = GetContainerName(ns);
			BlobContainerClient container = new BlobContainerClient(GetConnectionString(ns), fixedNamespace);
			if (await container.ExistsAsync())
			{
				// we can only delete it if the container exists
				await container.DeleteAsync();
			}
		}

		public async IAsyncEnumerable<(BlobId, DateTime)> ListObjectsAsync(NamespaceId ns)
		{
			await foreach ((string path, DateTime time) in GetBackend(ns).ListAsync(CancellationToken.None))
			{
				yield return (new BlobId(path), time);
			}
		}

		private static string SanitizeContainerName(string containerName)
		{
			return containerName.Replace(".", "-", StringComparison.OrdinalIgnoreCase).Replace("_", "-", StringComparison.OrdinalIgnoreCase).ToLower();
		}
	}

	public class AzureStorageBackend : IStorageBackend
	{
		private readonly NamespaceId _namespaceId;
		private readonly ILogger<AzureStorageBackend> _logger;
		private readonly Tracer _tracer;
		private readonly BlobContainerClient _blobContainer;

		private const string LastTouchedKey = "Io_LastTouched";
		private const string NamespaceKey = "Io_Namespace";

		public AzureStorageBackend(string connectionString, NamespaceId namespaceId, string containerName, ILogger<AzureStorageBackend> logger, Tracer tracer)
		{
			_namespaceId = namespaceId;
			_logger = logger;
			_tracer = tracer;
			_blobContainer = new BlobContainerClient(connectionString, containerName);
		}

		public async Task WriteAsync(string path, Stream content, CancellationToken cancellationToken)
		{
			Dictionary<string, string> metadata = new Dictionary<string, string> { { NamespaceKey, _namespaceId.ToString() } };
			await _blobContainer.CreateIfNotExistsAsync(metadata: metadata, cancellationToken: cancellationToken);

			_logger.LogDebug("Fetching blob reference with name {ObjectName}", path);
			try
			{

				await _blobContainer.GetBlobClient(path).UploadAsync(content, cancellationToken);
			}
			catch (RequestFailedException e)
			{
				if (e.Status == 409)
				{
					// the object already existed, that is fine, no need to do anything
					return;
				}

				throw;
			}
			finally
			{
				// we touch the blob so that the last access time is always refreshed even if we didnt actually mutate it to make sure the gc knows this is a active blob
				// see delete operation in Leda blob store cleanup
				await TouchBlobAsync(_blobContainer.GetBlobClient(path));
				_logger.LogDebug("Upload of blob {ObjectName} completed", path);
			}
		}

		private static async Task TouchBlobAsync(BlobClient blob)
		{
			Dictionary<string, string> metadata = new Dictionary<string, string>
			{
				{
					LastTouchedKey, DateTime.Now.ToString(CultureInfo.InvariantCulture)
				}
			};
			// we update the metadata, we don''t really care about the field being specified as we just want to touch the blob to update its last modified date property which we can not set
			await blob.SetMetadataAsync(metadata);
		}

		public async Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags, CancellationToken cancellationToken)
		{

			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			try
			{
				BlobClient blob = _blobContainer.GetBlobClient(path);
				Response<BlobDownloadInfo> blobInfo = await blob.DownloadAsync(cancellationToken);
				return new BlobContents(blobInfo.Value.Content, blobInfo.Value.ContentLength);
			}
			catch (RequestFailedException e)
			{
				if (e.Status == 404)
				{
					return null;
				}

				throw;
			}
		}

		public async Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				return false;
			}

			BlobClient blob = _blobContainer.GetBlobClient(path);
			return await blob.ExistsAsync(cancellationToken);
		}

		public async Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			await _blobContainer.DeleteBlobAsync(path, cancellationToken: cancellationToken);
		}

		public async IAsyncEnumerable<(string, DateTime)> ListAsync([EnumeratorCancellation] CancellationToken cancellationToken)
		{
			bool exists = await _blobContainer.ExistsAsync(cancellationToken);
			if (!exists)
			{
				yield break;
			}

			await foreach (BlobItem? item in _blobContainer.GetBlobsAsync(BlobTraits.Metadata, cancellationToken: cancellationToken))
			{
				yield return (item.Name, item.Properties?.LastModified?.DateTime ?? DateTime.Now);
			}
		}

		public async ValueTask<Uri?> GetReadRedirectAsync(string path)
		{
			if (!await _blobContainer.ExistsAsync())
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			return GetPresignedUrl(path, BlobSasPermissions.Read);
		}

		public async ValueTask<Uri?> GetWriteRedirectAsync(string path)
		{
			if (!await _blobContainer.ExistsAsync())
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			return GetPresignedUrl(path, BlobSasPermissions.Write);
		}

		/// <summary>
		/// Helper method to generate a presigned URL for a request
		/// </summary>
		Uri? GetPresignedUrl(string path, BlobSasPermissions permissions)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan("azure.BuildPresignedUrl")
					.SetAttribute("Path", path)
				;

			try
			{
				BlobClient blob = _blobContainer.GetBlobClient(path);
				return blob.GenerateSasUri(permissions, DateTimeOffset.Now.AddHours(1.0));
			}
			catch (RequestFailedException e)
			{
				if (e.Status == 404)
				{
					return null;
				}

				throw;
			}
		}

		public async Task<BlobMetadata?> GetMetadataAsync(string path, CancellationToken cancellationToken)
		{
			if (!await _blobContainer.ExistsAsync(cancellationToken))
			{
				throw new InvalidOperationException($"Container {_blobContainer.Name} did not exist");
			}

			BlobClient blob = _blobContainer.GetBlobClient(path);
			Response<BlobProperties>? response = await blob.GetPropertiesAsync(cancellationToken: cancellationToken);
			if (response == null)
			{
				return null;
			}

			return new BlobMetadata(response.Value.ContentLength, response.Value.CreatedOn.DateTime);
		}
	}
}
