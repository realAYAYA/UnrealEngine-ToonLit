// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Mime;
using System.Reflection;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Jupiter.Implementation.Bundles;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Jupiter.Controllers
{
    /// <summary>
    /// Response from uploading a bundle
    /// </summary>
    public class WriteBlobResponse
    {
        /// <summary>
        /// Locator for the uploaded bundle
        /// </summary>
        public BlobLocator Blob { get; set; }

        /// <summary>
        /// URL to upload the blob to.
        /// </summary>
        public Uri? UploadUrl { get; set; }

        /// <summary>
        /// Flag for whether the client could use a redirect instead (ie. not post content to the server, and get an upload url back).
        /// </summary>
        public bool? SupportsRedirects { get; set; }
    }

    /// <summary>
    /// Response object for finding a node
    /// </summary>
    public class FindNodeResponse
    {
        /// <summary>
        /// Hash of the target node
        /// </summary>
        public IoHash Hash { get; set; }

        /// <summary>
        /// Locator for the target blob
        /// </summary>
        public BlobLocator Blob { get; set; }

        /// <summary>
        /// Export index for the ref
        /// </summary>
        public int ExportIdx { get; set; }

        /// <summary>
        /// Constructor
        /// </summary>
        public FindNodeResponse(BlobHandle target)
        {
            Hash = target.Hash;
            Blob = target.GetLocator().Blob;
            ExportIdx = target.GetLocator().ExportIdx;
        }
    }
    /// <summary>
    /// Response object for searching for nodes with a given alias
    /// </summary>
    public class FindNodesResponse
    {
        /// <summary>
        /// Hash of the target node
        /// </summary>
#pragma warning disable CA2227 // Collection properties should be read only
        public List<FindNodeResponse> Nodes { get; set; } = new List<FindNodeResponse>();
#pragma warning restore CA2227 // Collection properties should be read only
    }

    /// <summary>
    /// Request object for writing a ref
    /// </summary>
    public class WriteRefRequest
    {
        /// <summary>
        /// Hash of the target node
        /// </summary>
        public IoHash Hash { get; set; }

        /// <summary>
        /// Locator for the target blob
        /// </summary>
        public BlobLocator Blob { get; set; }

        /// <summary>
        /// Export index for the ref
        /// </summary>
        public int ExportIdx { get; set; }

        /// <summary>
        /// Options for the ref
        /// </summary>
        public RefOptions? Options { get; set; }
    }

    /// <summary>
    /// Response object for reading a ref
    /// </summary>
    public class ReadRefResponse
    {
        /// <summary>
        /// Hash of the target node
        /// </summary>
        public IoHash Hash { get; set; }

        /// <summary>
        /// Locator for the target blob
        /// </summary>
        public BlobLocator Blob { get; set; }

        /// <summary>
        /// Export index for the ref
        /// </summary>
        public int ExportIdx { get; set; }

        /// <summary>
        /// Link to information about the target node
        /// </summary>
        public string Link { get; set; }

        /// <summary>
        /// Constructor
        /// </summary>
        public ReadRefResponse(BlobHandle target, string link)
        {
            Hash = target.Hash;
            Blob = target.GetLocator().Blob;
            ExportIdx = target.GetLocator().ExportIdx;
            Link = link;
        }

        [JsonConstructor]
        public ReadRefResponse(IoHash hash, BlobLocator blob, int exportIdx, string link)
        {
            Hash = hash;
            Blob = blob;
            ExportIdx = exportIdx;
            Link = link;
        }
    }

    /// <summary>
    /// Controller for the /api/v1/storage endpoint
    /// </summary>
    [Authorize]
    [ApiController]
    [Route("/api/v1/storage")]
    public class StorageController : ControllerBase
    {
        readonly IStorageService _storageService;
        private readonly RequestHelper _requestHelper;
        readonly IMemoryCache _memoryCache;
        readonly ILogger<StorageController> _logger;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="storageService"></param>
        /// <param name="requestHelper"></param>
        /// <param name="memoryCache"></param>
        /// <param name="logger"></param>
        public StorageController(IStorageService storageService, RequestHelper requestHelper, IMemoryCache memoryCache, ILogger<StorageController> logger)
        {
            _storageService = storageService;
            _requestHelper = requestHelper;
            _memoryCache = memoryCache;
            _logger = logger;
        }

        /// <summary>
        /// Uploads data to the storage service. 
        /// </summary>
        /// <param name="namespaceId">Namespace to fetch from</param>
        /// <param name="file">Data to be uploaded. May be null, in which case the server may return a separate url.</param>
        /// <param name="prefix">Prefix for the uploaded file</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        [HttpPost]
        [Route("{namespaceId}/blobs")]
        public async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(NamespaceId namespaceId, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }

            IStorageClientJupiter client = await _storageService.GetClientAsync(namespaceId, cancellationToken);
            // disable redirected responses for upload so that we can parse the blobs for references
            // TODO: Add way to parse objects after upload similar to BlobTick in Horde
            /*if (file == null)
            {
                (BlobLocator Locator, Uri UploadUrl)? response = await client.GetWriteRedirectAsync(prefix ?? string.Empty, cancellationToken);
                if (response == null)
                {
                    return new WriteBlobResponse { SupportsRedirects = false };
                }
                else
                {
                    return new WriteBlobResponse { Blob = response.Value.Locator, UploadUrl = response.Value.UploadUrl };
                }
            }*/
            if (file == null)
            {
                return new WriteBlobResponse { SupportsRedirects = false };
            }
            else
            {
                using (Stream stream = file.OpenReadStream())
                {
                    BlobLocator locator = await client.WriteBlobAsync(stream, prefix: (prefix == null) ? Utf8String.Empty : new Utf8String(prefix), cancellationToken: cancellationToken);
                    return new WriteBlobResponse { Blob = locator, SupportsRedirects = client.SupportsRedirects? (bool?)true : null };
                }
            }
        }

        /// <summary>
        /// Retrieves data from the storage service. 
        /// </summary>
        /// <param name="namespaceId">Namespace to fetch from</param>
        /// <param name="locator">Bundle to retrieve</param>
        /// <param name="offset">Offset of the data.</param>
        /// <param name="length">Length of the data to return.</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        [HttpGet]
        [Route("{namespaceId}/blobs/{*locator}")]
        public async Task<ActionResult> ReadBlobAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery] int? offset = null, [FromQuery] int? length = null, CancellationToken cancellationToken = default)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            IStorageClientJupiter client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

            Uri? redirectUrl = await client.GetReadRedirectAsync(locator, cancellationToken);
            if (redirectUrl != null)
            {
                return Redirect(redirectUrl.ToString());
            }

#pragma warning disable CA2000 // Dispose objects before losing scope
            // TODO: would be better to use the range header here, but seems to require a lot of plumbing to convert unseekable AWS streams into a format that works with range processing.
            Stream stream;
            if (offset == null && length == null)
            {
                stream = await client.ReadBlobAsync(locator, cancellationToken);
            }
            else if (offset != null && length != null)
            {
                stream = await client.ReadBlobRangeAsync(locator, offset.Value, length.Value, cancellationToken);

                return File(stream, MediaTypeNames.Application.Octet);
            }
            else
            {
                return BadRequest("Offset and length must both be specified as query parameters for ranged reads");
            }
            return File(stream, "application/octet-stream");
#pragma warning restore CA2000 // Dispose objects before losing scope
        }

        /// <summary>
        /// Retrieves data from the storage service. 
        /// </summary>
        /// <param name="namespaceId">Namespace to fetch from</param>
        /// <param name="alias">Alias of the node to find</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        [HttpGet]
        [Route("{namespaceId}/nodes")]
        public async Task<ActionResult<FindNodesResponse>> FindNodesAsync(NamespaceId namespaceId, string alias, CancellationToken cancellationToken = default)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            IStorageClient client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

            FindNodesResponse response = new FindNodesResponse();
            await foreach (BlobHandle handle in client.FindNodesAsync(alias, cancellationToken))
            {
                response.Nodes.Add(new FindNodeResponse(handle));
            }

            if (response.Nodes.Count == 0)
            {
                return NotFound();
            }

            return response;
        }

        /// <summary>
        /// Writes a ref to the storage service.
        /// </summary>
        /// <param name="namespaceId">Namespace to write to</param>
        /// <param name="refName">Name of the ref</param>
        /// <param name="request">Request for the ref to write</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        [HttpPut]
        [Route("{namespaceId}/refs/{*refName}")]
        public async Task<ActionResult> WriteRefAsync(NamespaceId namespaceId, RefName refName, [FromBody] WriteRefRequest request, CancellationToken cancellationToken)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }
            IStorageClientJupiter client = await _storageService.GetClientAsync(namespaceId, cancellationToken);
            HashedNodeLocator target = new HashedNodeLocator(request.Hash, request.Blob, request.ExportIdx);
            await client.WriteRefTargetAsync(refName, target, request.Options, cancellationToken);

            return Ok();
        }

        /// <summary>
        /// Uploads data to the storage service. 
        /// </summary>
        /// <param name="namespaceId"></param>
        /// <param name="refName"></param>
        /// <param name="cancellationToken"></param>
        [HttpGet]
        [Route("{namespaceId}/refs/{*refName}")]
        public async Task<ActionResult<ReadRefResponse>> ReadRefAsync(NamespaceId namespaceId, RefName refName, CancellationToken cancellationToken)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            IStorageClient client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

            BlobHandle? target = await client.TryReadRefTargetAsync(refName, cancellationToken: cancellationToken);
            if (target == null)
            {
                return NotFound();
            }

            string link = Url.Action("GetNode", new { namespaceId = namespaceId, locator = target.GetLocator().Blob, export = target.GetLocator().ExportIdx })!;
            return new ReadRefResponse(target, WebUtility.UrlDecode(link));
        }

        /// <summary>
        /// Gets information about a particular bundle in storage
        /// </summary>
        /// <param name="namespaceId">Namespace containing the blob</param>
        /// <param name="locator">Blob locator</param>
        /// <param name="includeImports">Whether to include imports for the bundle</param>
        /// <param name="includeExports">Whether to include exports for the bundle</param>
        /// <param name="includePackets">Whether to include packets for the bundle</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        /// <returns></returns>
        [HttpGet]
        [Route("{namespaceId}/bundles/{*locator}")]
        public async Task<ActionResult<object>> GetBundleAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery(Name = "imports")] bool includeImports = false, [FromQuery(Name = "exports")] bool includeExports = true, [FromQuery(Name = "packets")] bool includePackets = false, CancellationToken cancellationToken = default)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            IStorageClient storageClient = await _storageService.GetClientAsync(namespaceId, cancellationToken);

            BundleReader reader = new BundleReader(storageClient, _memoryCache, _logger);

            BundleHeader header = await reader.ReadBundleHeaderAsync(locator, cancellationToken);

            List<object>? responseImports = null;
            if (includeImports)
            {
                responseImports = new List<object>();
                foreach (BlobLocator import in header.Imports)
                {
                    string link = Url.Action("GetBundle", new { namespaceId = namespaceId, locator = import })!;
                    responseImports.Add(link);
                }
            }

            List<object>? responseExports = null;
            if (includeExports)
            {
                responseExports = new List<object>();
                for (int exportIdx = 0; exportIdx < header.Exports.Count; exportIdx++)
                {
                    BundleExport export = header.Exports[exportIdx];

                    string details = Url.Action("GetNode", new { namespaceId = namespaceId, locator = locator, export = exportIdx})!;
                    BlobType type = header.Types[export.TypeIdx];
                    string typeName = GetNodeType(type.Guid)?.Name ?? type.Guid.ToString();

                    responseExports.Add(new { export.Hash, export.Length, details, type = typeName });
                }
            }

            List<object>? responsePackets = null;
            if (includePackets)
            {
                responsePackets = new List<object>();
                for (int packetIdx = 0, exportIdx = 0; packetIdx < header.Packets.Count; packetIdx++)
                {
                    BundlePacket packet = header.Packets[packetIdx];

                    List<string> packetExports = new List<string>();

                    int length = 0;
                    for (; exportIdx < header.Exports.Count && length + header.Exports[exportIdx].Length <= packet.DecodedLength; exportIdx++)
                    {
                        BundleExport export = header.Exports[exportIdx];
                        packetExports.Add(Url.Action("GetNode", new { namespaceId = namespaceId, locator = locator, export = exportIdx})!);
                        length += export.Length;
                    }

                    responsePackets.Add(new { packetIdx, packet.EncodedLength, packet.DecodedLength, exports = packetExports });
                }
            }

            return new { imports = responseImports, exports = responseExports, packets = responsePackets };
        }

        /// <summary>
        /// Gets information about a particular bundle in storage
        /// </summary>
        /// <param name="namespaceId">Namespace containing the blob</param>
        /// <param name="locator">Blob locator</param>
        /// <param name="exportIdx">Index of the export</param>
        /// <param name="cancellationToken">Cancellation token for the operation</param>
        /// <returns></returns>
        [HttpGet]
        [Route("{namespaceId}/nodes/{*locator}")]
        public async Task<ActionResult<object>> GetNodeAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery(Name = "export")] int exportIdx, CancellationToken cancellationToken = default)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, namespaceId, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            IStorageClient storageClient = await _storageService.GetClientAsync(namespaceId, cancellationToken);
            BundleReader reader = new BundleReader(storageClient, _memoryCache, _logger);

            BundleHeader header = await reader.ReadBundleHeaderAsync(locator, cancellationToken);
            BundleExport export = header.Exports[exportIdx];

            object content;

            BlobData nodeData = await reader.ReadNodeDataAsync(new NodeLocator(locator, exportIdx), cancellationToken);

            Node node = Node.Deserialize(nodeData);
            switch (node)
            {
                case DirectoryNode directoryNode:
                    {
                        List<object> directories = new List<object>();
                        foreach ((Utf8String name, DirectoryEntry entry) in directoryNode.NameToDirectory)
                        {
                            directories.Add(new { name = name.ToString(), length = entry.Length, hash = entry.Handle.Hash, link = Url.Action("GetNode", new { namespaceId = namespaceId, locator = entry.Handle!.GetLocator().Blob, export = entry.Handle!.GetLocator().ExportIdx})! });
                        }

                        List<object> files = new List<object>();
                        foreach ((Utf8String name, FileEntry entry) in directoryNode.NameToFile)
                        {
                            files.Add(new { name = name.ToString(), length = entry.Length, flags = entry.Flags, hash = entry.Hash, link = Url.Action("GetNode", new { namespaceId = namespaceId, locator = entry.Handle!.GetLocator().Blob, export = entry.Handle!.GetLocator().ExportIdx})!});
                        }

                        content = new { directoryNode.Length, directories, files };
                    }
                    break;
                default:
                    content = new { references = nodeData.Refs.Select(x => Url.Action("GetNode", new { namespaceId = namespaceId, locator = x.GetLocator().Blob, export = x.GetLocator().ExportIdx})!) };
                    break;
            }

            return new { bundle = Url.Action("GetBundle", new { namespaceId = namespaceId, locator = locator})!, export.Hash, export.Length, guid = header.Types[export.TypeIdx].Guid, type = node.GetType().Name, content = content };
        }

        static Type? GetNodeType(Guid typeGuid)
        {
            Node.TryGetConcreteType(typeGuid, out Type? type);
            return type;
        }
    }
}
