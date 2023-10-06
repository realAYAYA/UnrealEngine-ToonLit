// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http.Headers;
using System.Reflection;
using System.Security.Claims;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Amazon.EC2.Model;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Redis;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Primitives;

namespace Horde.Server.Storage
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
		public List<FindNodeResponse> Nodes { get; set; } = new List<FindNodeResponse>();
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
	}

	/// <summary>
	/// Controller for the /api/v1/storage endpoint
	/// </summary>
	[Authorize]
	[ApiController]
	[Route("[controller]")]
	public class StorageController : HordeControllerBase
	{
		readonly StorageService _storageService;
		readonly IMemoryCache _memoryCache;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageController(StorageService storageService, IMemoryCache memoryCache, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<StorageController> logger)
		{
			_storageService = storageService;
			_memoryCache = memoryCache;
			_globalConfig = globalConfig;
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
		[Route("/api/v1/storage/{namespaceId}/blobs")]
		public async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(NamespaceId namespaceId, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.WriteBlobs, User) && !HasPathClaim(User, HordeClaimTypes.WriteNamespace, namespaceId, prefix ?? String.Empty))
			{
				return Forbid(StorageAclAction.WriteBlobs, namespaceId);
			}

			IStorageClientImpl storageClient = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			return await WriteBlobAsync(storageClient, file, prefix, cancellationToken);
		}

		/// <summary>
		/// Writes a blob to storage. Exposed as a public utility method to allow other routes with their own authentication methods to wrap their own authentication/redirection.
		/// </summary>
		/// <param name="storageClient">The client to write to service</param>
		/// <param name="file">File to be written</param>
		/// <param name="prefix">Prefix for uploaded blobs</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Information about the written blob, or redirect information</returns>
		public static async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(IStorageClient storageClient, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
		{
			IStorageClientImpl? storageClientImpl = storageClient as IStorageClientImpl;
			if (file == null)
			{
				if (storageClientImpl == null)
				{
					return new WriteBlobResponse { SupportsRedirects = false };
				}

				(BlobLocator Locator, Uri UploadUrl)? result = await storageClientImpl.GetWriteRedirectAsync(prefix ?? String.Empty, cancellationToken);
				if (result == null)
				{
					return new WriteBlobResponse { SupportsRedirects = false };
				}

				return new WriteBlobResponse { Blob = result.Value.Locator, UploadUrl = result.Value.UploadUrl };
			}
			else
			{
				using (Stream stream = file.OpenReadStream())
				{
					BlobLocator locator = await storageClient.WriteBlobAsync(stream, prefix: (prefix == null) ? Utf8String.Empty : new Utf8String(prefix), cancellationToken: cancellationToken);
					return new WriteBlobResponse { Blob = locator, SupportsRedirects = storageClientImpl?.SupportsRedirects };
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
		[Route("/api/v1/storage/{namespaceId}/blobs/{*locator}")]
		public async Task<ActionResult> ReadBlobAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery] int? offset = null, [FromQuery] int? length = null, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadBlobs, User) && !HasPathClaim(User, HordeClaimTypes.ReadNamespace, namespaceId, locator.Inner.ToString()))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			return await ReadBlobInternalAsync(_storageService, namespaceId, locator, offset, length, cancellationToken);
		}

		/// <summary>
		/// Reads a blob from storage, without performing namespace access checks.
		/// </summary>
		internal static async Task<ActionResult> ReadBlobInternalAsync(StorageService storageService, NamespaceId namespaceId, BlobLocator locator, int? offset, int? length, CancellationToken cancellationToken)
		{
			IStorageClientImpl client = await storageService.GetClientAsync(namespaceId, cancellationToken);
			return await ReadBlobInternalAsync(client, locator, offset, length, cancellationToken);
		}

		/// <summary>
		/// Reads a blob from storage, without performing namespace access checks.
		/// </summary>
		internal static async Task<ActionResult> ReadBlobInternalAsync(IStorageClient storageClient, BlobLocator locator, int? offset, int? length, CancellationToken cancellationToken)
		{
			if (storageClient is IStorageClientImpl storageClientImpl)
			{
				Uri? redirectUrl = await storageClientImpl.GetReadRedirectAsync(locator, cancellationToken);
				if (redirectUrl != null)
				{
					return new RedirectResult(redirectUrl.ToString());
				}
			}

#pragma warning disable CA2000 // Dispose objects before losing scope
			// TODO: would be better to use the range header here, but seems to require a lot of plumbing to convert unseekable AWS streams into a format that works with range processing.
			Stream stream;
			if (offset == null && length == null)
			{
				stream = await storageClient.ReadBlobAsync(locator, cancellationToken);
			}
			else if (offset != null && length != null)
			{
				stream = await storageClient.ReadBlobRangeAsync(locator, offset.Value, length.Value, cancellationToken);
			}
			else
			{
				return new BadRequestObjectResult("Offset and length must both be specified as query parameters for ranged reads");
			}
			return new FileStreamResult(stream, "application/octet-stream");
#pragma warning restore CA2000 // Dispose objects before losing scope
		}

		/// <summary>
		/// Retrieves data from the storage service. 
		/// </summary>
		/// <param name="namespaceId">Namespace to fetch from</param>
		/// <param name="alias">Alias of the node to find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpGet]
		[Route("/api/v1/storage/{namespaceId}/nodes")]
		public async Task<ActionResult<FindNodesResponse>> FindNodesAsync(NamespaceId namespaceId, string alias, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadBlobs, User))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			IStorageClientImpl client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

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
		[Route("/api/v1/storage/{namespaceId}/refs/{*refName}")]
		public async Task<ActionResult> WriteRefAsync(NamespaceId namespaceId, RefName refName, [FromBody] WriteRefRequest request, CancellationToken cancellationToken)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.WriteRefs, User) && !HasPathClaim(User, HordeClaimTypes.WriteNamespace, namespaceId, refName.ToString()))
			{
				return Forbid(StorageAclAction.WriteRefs, namespaceId);
			}

			IStorageClientImpl client = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			NodeLocator target = new NodeLocator(request.Hash, request.Blob, request.ExportIdx);
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
		[Route("/api/v1/storage/{namespaceId}/refs/{*refName}")]
		public async Task<ActionResult<ReadRefResponse>> ReadRefAsync(NamespaceId namespaceId, RefName refName, CancellationToken cancellationToken)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadRefs, User) && !HasPathClaim(User, HordeClaimTypes.ReadNamespace, namespaceId, refName.ToString()))
			{
				return Forbid(StorageAclAction.ReadRefs, namespaceId);
			}

			return await ReadRefInternalAsync(_storageService, namespaceId, refName, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from storage, without performing namespace access checks.
		/// </summary>
		internal static async Task<ActionResult<ReadRefResponse>> ReadRefInternalAsync(StorageService storageService, NamespaceId namespaceId, RefName refName, IHeaderDictionary headers, CancellationToken cancellationToken)
		{
			IStorageClient client = await storageService.GetClientAsync(namespaceId, cancellationToken);

			RefCacheTime cacheTime = new RefCacheTime();
			foreach (string entry in headers.CacheControl)
			{
				if (CacheControlHeaderValue.TryParse(entry, out CacheControlHeaderValue? value) && value?.MaxAge != null)
				{
					cacheTime = new RefCacheTime(value.MaxAge.Value);
				}
			}

			BlobHandle? target = await client.TryReadRefTargetAsync(refName, cacheTime, cancellationToken: cancellationToken);
			if (target == null)
			{
				return new NotFoundResult();
			}

			NodeLocator locator = target.GetLocator();
			string link = $"/api/v1/storage/{namespaceId}/nodes/{locator.Blob}?export={locator.ExportIdx}";
			return new ReadRefResponse(target, link);
		}

		/// <summary>
		/// Checks whether the user has an explicit claim to read or write to a path within a namespace
		/// </summary>
		/// <param name="user">User to query</param>
		/// <param name="claimType">The claim name</param>
		/// <param name="namespaceId">Namespace id to check for</param>
		/// <param name="entity">Path to the entity to query</param>
		/// <returns>True if the user is authorized for access to the given path</returns>
		static bool HasPathClaim(ClaimsPrincipal user, string claimType, NamespaceId namespaceId, string entity)
		{
			foreach (Claim claim in user.Claims)
			{
				if (claim.Type.Equals(claimType, StringComparison.Ordinal))
				{
					int colonIdx = claim.Value.IndexOf(':', StringComparison.Ordinal);
					if (colonIdx == -1)
					{
						if (namespaceId.Text.Equals(claim.Value))
						{
							return true;
						}
					}
					else
					{
						if (namespaceId.Text.Equals(claim.Value.AsMemory(0, colonIdx)) && HasPathPrefix(entity, claim.Value.AsSpan(colonIdx + 1)))
						{
							return true;
						}
					}
				}
			}
			return false;
		}

		static bool HasPathPrefix(string name, ReadOnlySpan<char> prefix)
		{
			return name.Length > prefix.Length && name[prefix.Length] == '/' && name.AsSpan(0, prefix.Length).SequenceEqual(prefix);
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
		[Route("/api/v1/storage/{namespaceId}/bundles/{*locator}")]
		public async Task<ActionResult<object>> GetBundleAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery(Name = "imports")] bool includeImports = false, [FromQuery(Name = "exports")] bool includeExports = true, [FromQuery(Name = "packets")] bool includePackets = false, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadBlobs, User))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			IStorageClient storageClient = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			BundleReader reader = new BundleReader(storageClient, _memoryCache, _logger);

			BundleHeader header = await reader.ReadBundleHeaderAsync(locator, cancellationToken);

			string linkBase = $"/api/v1/storage/{namespaceId}";

			List<object>? responseImports = null;
			if (includeImports)
			{
				responseImports = new List<object>();
				foreach (BlobLocator import in header.Imports)
				{
					responseImports.Add($"{linkBase}/bundles/{import}");
				}
			}

			List<object>? responseExports = null;
			if (includeExports)
			{
				responseExports = new List<object>();
				for (int exportIdx = 0; exportIdx < header.Exports.Count; exportIdx++)
				{
					BundleExport export = header.Exports[exportIdx];

					string details = $"{linkBase}/nodes/{locator}?export={exportIdx}";
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
						packetExports.Add($"{linkBase}/{locator}?export={exportIdx}");
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
		[Route("/api/v1/storage/{namespaceId}/nodes/{*locator}")]
		public async Task<ActionResult<object>> GetNodeAsync(NamespaceId namespaceId, BlobLocator locator, [FromQuery(Name = "export")] int exportIdx, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadBlobs, User))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			IStorageClient storageClient = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			BundleReader reader = new BundleReader(storageClient, _memoryCache, _logger);

			BundleHeader header = await reader.ReadBundleHeaderAsync(locator, cancellationToken);
			BundleExport export = header.Exports[exportIdx];

			string linkBase = $"/api/v1/storage/{namespaceId}";

			object content;

			BlobData nodeData = await reader.ReadNodeDataAsync(new NodeLocator(export.Hash, locator, exportIdx), cancellationToken);

			Node node = Node.Deserialize(nodeData);
			switch (node)
			{
				case DirectoryNode directoryNode:
					{
						List<object> directories = new List<object>();
						foreach ((Utf8String name, DirectoryEntry entry) in directoryNode.NameToDirectory)
						{
							directories.Add(new { name = name.ToString(), length = entry.Length, hash = entry.Handle.Hash, link = GetNodeLink(linkBase, entry.Handle) });
						}

						List<object> files = new List<object>();
						foreach ((Utf8String name, FileEntry entry) in directoryNode.NameToFile)
						{
							files.Add(new { name = name.ToString(), length = entry.Length, flags = entry.Flags, hash = entry.Hash, link = GetNodeLink(linkBase, entry.Handle) });
						}

						content = new { directoryNode.Length, directories, files };
					}
					break;
				default:
					content = new { references = nodeData.Refs.Select(x => GetNodeLink(linkBase, x)) };
					break;
			}

			return new { bundle = $"{linkBase}/bundles/{locator}", export.Hash, export.Length, guid = header.Types[export.TypeIdx].Guid, type = node.GetType().Name, content = content };
		}

		static string GetNodeLink(string linkBase, BlobHandle handle) => GetNodeLink(linkBase, handle.GetLocator());
		
		static string GetNodeLink(string linkBase, NodeLocator locator) => $"{linkBase}/nodes/{locator.Blob}?export={locator.ExportIdx}";

		static Type? GetNodeType(Guid typeGuid)
		{
			Node.TryGetConcreteType(typeGuid, out Type? type);
			return type;
		}
	}
}
