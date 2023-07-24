// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;
using Azure;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Azure.Storage.Blobs;
using Azure.Storage.Blobs.Models;
using EpicGames.Horde.Storage;
using Jupiter.Common.Implementation;
using System.Threading;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;

namespace Jupiter.Implementation
{
    public class AzureBlobStore : IBlobStore
    {
        private readonly ILogger _logger;
        private readonly string _connectionString;
        private readonly ConcurrentDictionary<NamespaceId, IStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, IStorageBackend>();

        public AzureBlobStore(IOptionsMonitor<AzureSettings> settings, IServiceProvider provider, ILogger<AzureBlobStore> logger)
        {
            _connectionString = GetConnectionString(settings.CurrentValue, provider);
            _logger = logger;
        }

        private IStorageBackend GetBackend(NamespaceId ns)
        {
            return _backends.GetOrAdd(ns, x => new AzureStorageBackend(_connectionString, ns, SanitizeNamespace(ns), _logger));
        }

        private static string GetPath(BlobIdentifier blobIdentifier) => blobIdentifier.ToString();

        /// <summary>
        /// Gets the connection string for Azure storage.
        /// If a key vault secret is used, the value is cached in <see cref="AzureSettings.ConnectionString"/>
        /// for next time.
        /// </summary>
        /// <param name="settings"></param>
        /// <param name="provider"></param>
        /// <returns></returns>
        public static string GetConnectionString(AzureSettings settings, IServiceProvider provider)
        {
            // Cache the connection string in the settings for next time.
            ISecretResolver secretResolver = provider.GetService<ISecretResolver>()!;
            string connectionString = secretResolver.Resolve(settings.ConnectionString)!;
            settings.ConnectionString = connectionString;

            return connectionString;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier blobIdentifier)
        {
            // TODO: this is not ideal as we copy the buffer, but there is no upload from memory available so we would need this copy anyway
            await using MemoryStream stream = new MemoryStream(content.ToArray());
            return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier blobIdentifier)
        {
            await GetBackend(ns).WriteAsync(GetPath(blobIdentifier), content, CancellationToken.None);
            return blobIdentifier;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] content, BlobIdentifier blobIdentifier)
        {
            await using MemoryStream stream = new MemoryStream(content);
            return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blobIdentifier,
            LastAccessTrackingFlags flags)
        {
            BlobContents? contents = await GetBackend(ns).TryReadAsync(GetPath(blobIdentifier), flags, CancellationToken.None);
            if (contents == null)
                {
                    throw new BlobNotFoundException(ns, blobIdentifier);
                }
            return contents;
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blobIdentifier, bool forceCheck)
        {
            return await GetBackend(ns).ExistsAsync(GetPath(blobIdentifier));
        }

        public async Task DeleteObject(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            await GetBackend(ns).DeleteAsync(GetPath(blobIdentifier));
        }

        public async Task DeleteNamespace(NamespaceId ns)
        {
            string fixedNamespace = SanitizeNamespace(ns);
            BlobContainerClient container = new BlobContainerClient(_connectionString, fixedNamespace);
            if (await container.ExistsAsync())
            {
                // we can only delete it if the container exists
                await container.DeleteAsync();
            }
        }

        public async IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
        {
            await foreach ((string path, DateTime time) in GetBackend(ns).ListAsync())
            {
                yield return (new BlobIdentifier(path), time);
            }
        }

        private static string SanitizeNamespace(NamespaceId ns)
        {
            return ns.ToString().Replace(".", "-", StringComparison.OrdinalIgnoreCase).Replace("_", "-", StringComparison.OrdinalIgnoreCase).ToLower();
        }
    }

    public class AzureStorageBackend : IStorageBackend
    {
        private readonly string _connectionString;
        private readonly NamespaceId _namespaceId;
        private readonly string _containerName;
        private readonly ILogger _logger;

        private const string LastTouchedKey = "Io_LastTouched";
        private const string NamespaceKey = "Io_Namespace";

        public AzureStorageBackend(string connectionString, NamespaceId namespaceId, string containerName, ILogger logger)
        {
            _connectionString = connectionString;
            _namespaceId = namespaceId;
            _containerName = containerName;
            _logger = logger;
        }

        /// <summary>
        /// Gets the connection string for Azure storage.
        /// If a key vault secret is used, the value is cached in <see cref="AzureSettings.ConnectionString"/>
        /// for next time.
        /// </summary>
        /// <param name="settings"></param>
        /// <param name="provider"></param>
        /// <returns></returns>
        public static string GetConnectionString(AzureSettings settings, IServiceProvider provider)
        {
            // Cache the connection string in the settings for next time.
            ISecretResolver secretResolver = provider.GetService<ISecretResolver>()!;
            string connectionString = secretResolver.Resolve(settings.ConnectionString)!;
            settings.ConnectionString = connectionString;

            return connectionString;
        }

        public async Task WriteAsync(string path, Stream content, CancellationToken cancellationToken)
        {
            _logger.LogDebug("Checking if Azure container with name {Name} exists", _containerName);
            BlobContainerClient container = new BlobContainerClient(_connectionString, _containerName);
            Dictionary<string, string> metadata = new Dictionary<string, string> { { NamespaceKey, _namespaceId.ToString() } };
            await container.CreateIfNotExistsAsync(metadata: metadata, cancellationToken: cancellationToken);

            _logger.LogDebug("Fetching blob reference with name {ObjectName}", path);
            try
            {

                await container.GetBlobClient(path).UploadAsync(content, cancellationToken);
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
                await TouchBlob(container.GetBlobClient(path));
                _logger.LogDebug("Upload of blob {ObjectName} completed", path);
            }
        }

        private static async Task TouchBlob(BlobClient blob)
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
            BlobContainerClient container = new BlobContainerClient(_connectionString, _containerName);
            if (!await container.ExistsAsync(cancellationToken))
            {
                throw new InvalidOperationException($"Container {_containerName} did not exist");
            }

            try
            {
                BlobClient blob = container.GetBlobClient(path);
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
            BlobContainerClient container = new BlobContainerClient(_connectionString, _containerName);
            if (!await container.ExistsAsync(cancellationToken))
            {
                return false;
            }

            BlobClient blob = container.GetBlobClient(path);
            return await blob.ExistsAsync(cancellationToken);
        }

        public async Task DeleteAsync(string path, CancellationToken cancellationToken)
        {
            BlobContainerClient container = new BlobContainerClient(_connectionString, _containerName);
            if (!await container.ExistsAsync(cancellationToken))
            {
                throw new InvalidOperationException($"Container {_containerName} did not exist");
            }

            await container.DeleteBlobAsync(path, cancellationToken: cancellationToken);
        }

        public async IAsyncEnumerable<(string, DateTime)> ListAsync([EnumeratorCancellation] CancellationToken cancellationToken)
        {
            BlobContainerClient container = new BlobContainerClient(_connectionString, _containerName);
            bool exists = await container.ExistsAsync(cancellationToken);
            if (!exists)
            {
                yield break;
            }

            await foreach (BlobItem? item in container.GetBlobsAsync(BlobTraits.Metadata, cancellationToken: cancellationToken))
            {
                yield return (item.Name, item.Properties?.LastModified?.DateTime ?? DateTime.Now);
            }
        }
    }
}
