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
using Jupiter.Implementation;
using Serilog;
using EpicGames.Horde.Storage;
using Jupiter.Common.Implementation;

namespace Horde.Storage.Implementation
{
    public class AzureBlobStore : IBlobStore
    {
        private readonly ILogger _logger = Log.ForContext<AzureBlobStore>();
        private readonly string _connectionString;

        private const string LastTouchedKey = "Io_LastTouched";
        private const string NamespaceKey = "Io_Namespace";

        public AzureBlobStore(IOptionsMonitor<AzureSettings> settings, IServiceProvider provider)
        {
            _connectionString = GetConnectionString(settings.CurrentValue, provider);
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

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier blobIdentifier)
        {
            // TODO: this is not ideal as we copy the buffer, but there is no upload from memory available so we would need this copy anyway
            await using MemoryStream stream = new MemoryStream(content.ToArray());
            return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier blobIdentifier)
        {
            string @namespace = SanitizeNamespace(ns);
            _logger.Debug("Checking if Azure container with name {Name} exists", @namespace);
            BlobContainerClient container = new BlobContainerClient(_connectionString, @namespace);
            Dictionary<string, string> metadata = new Dictionary<string, string> { { NamespaceKey, ns.ToString() } };
            await container.CreateIfNotExistsAsync(metadata:metadata);

            _logger.Debug("Fetching blob reference with name {ObjectName}", blobIdentifier);
            try
            {

                await container.GetBlobClient(blobIdentifier.ToString()).UploadAsync(content);
            }
            catch (RequestFailedException e)
            {
                if (e.Status == 409)
                {
                    // the object already existed, that is fine, no need to do anything
                    return blobIdentifier;
                }

                throw;
            }
            finally
            {
                // we touch the blob so that the last access time is always refreshed even if we didnt actually mutate it to make sure the gc knows this is a active blob
                // see delete operation in Leda blob store cleanup
                await TouchBlob(container.GetBlobClient(blobIdentifier.ToString()));
                _logger.Debug("Upload of blob {ObjectName} completed", blobIdentifier);
            }

            return blobIdentifier;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] content, BlobIdentifier blobIdentifier)
        {
            await using MemoryStream stream = new MemoryStream(content);
            return await PutObject(ns, stream, blobIdentifier);
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

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blobIdentifier,
            LastAccessTrackingFlags flags)
        {
            string fixedNamespace = SanitizeNamespace(ns);
            BlobContainerClient container = new BlobContainerClient(_connectionString, fixedNamespace);
            if (!await container.ExistsAsync())
            {
                throw new InvalidOperationException($"Container {fixedNamespace} did not exist");
            }

            try
            {
                BlobClient blob = container.GetBlobClient(blobIdentifier.ToString());
                Response<BlobDownloadInfo> blobInfo = await blob.DownloadAsync();
                return new BlobContents(blobInfo.Value.Content, blobInfo.Value.ContentLength);
            }
            catch (RequestFailedException e)
            {
                if (e.Status == 404)
                {
                    throw new BlobNotFoundException(ns, blobIdentifier);
                }

                throw;
            }
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blobIdentifier, bool forceCheck)
        {
            BlobContainerClient container = new BlobContainerClient(_connectionString, SanitizeNamespace(ns));
            if (!await container.ExistsAsync())
            {
                return false;
            }

            BlobClient blob = container.GetBlobClient(blobIdentifier.ToString());
            return await blob.ExistsAsync();
        }

        public async Task DeleteObject(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            string fixedNamespace = SanitizeNamespace(ns);
            BlobContainerClient container = new BlobContainerClient(_connectionString, fixedNamespace);
            if (!await container.ExistsAsync())
            {
                throw new InvalidOperationException($"Container {fixedNamespace} did not exist");
            }

            await container.DeleteBlobAsync(blobIdentifier.ToString());
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
            string fixedNamespace = SanitizeNamespace(ns);
            BlobContainerClient container = new BlobContainerClient(_connectionString, fixedNamespace);
            bool exists = await container.ExistsAsync();
            if (!exists)
            {
                yield break;
            }

            await foreach (BlobItem? item in container.GetBlobsAsync(BlobTraits.Metadata))
            {
                yield return (new BlobIdentifier(item.Name), item.Properties?.LastModified?.DateTime ?? DateTime.Now);
            }
        }

        private static string SanitizeNamespace(NamespaceId ns)
        {
            return ns.ToString().Replace(".", "-", StringComparison.OrdinalIgnoreCase).ToLower();
        }
    }
}
