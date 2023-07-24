// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Amazon.EC2.Model;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Primitives;

namespace Horde.Build.Storage
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
		public FindNodeResponse(NodeHandle target)
		{
			Hash = target.Hash;
			Blob = target.Locator.Blob;
			ExportIdx = target.Locator.ExportIdx;
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
		/// Constructor
		/// </summary>
		public ReadRefResponse(NodeHandle target)
		{
			Hash = target.Hash;
			Blob = target.Locator.Blob;
			ExportIdx = target.Locator.ExportIdx;
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
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageController(StorageService storageService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_storageService = storageService;
			_globalConfig = globalConfig;
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
			if (!namespaceConfig.Authorize(AclAction.WriteBlobs, User))
			{
				return Forbid(AclAction.WriteBlobs, namespaceId);
			}

			IStorageClientImpl client = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			if (file == null)
			{
				(BlobLocator Locator, Uri UploadUrl)? result = await client.GetWriteRedirectAsync(prefix ?? String.Empty, cancellationToken);
				if (result == null)
				{
					return new WriteBlobResponse { SupportsRedirects = false };
				}
				else
				{
					return new WriteBlobResponse { Blob = result.Value.Locator, UploadUrl = result.Value.UploadUrl };
				}
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
		/// Writes a blob to storage. Exposed as a public utility method to allow other routes with their own authentication methods to wrap their own authentication/redirection.
		/// </summary>
		/// <param name="storageService">The storage service</param>
		/// <param name="namespaceId">Namespace to write the blob to</param>
		/// <param name="file">File to be written</param>
		/// <param name="prefix">Prefix for uploaded blobs</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Information about the written blob, or redirect information</returns>
		public static async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(StorageService storageService, NamespaceId namespaceId, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
		{
			IStorageClientImpl client = await storageService.GetClientAsync(namespaceId, cancellationToken);
			if (file == null)
			{
				(BlobLocator Locator, Uri UploadUrl)? result = await client.GetWriteRedirectAsync(prefix ?? String.Empty, cancellationToken);
				if (result == null)
				{
					return new WriteBlobResponse { SupportsRedirects = false };
				}
				else
				{
					return new WriteBlobResponse { Blob = result.Value.Locator, UploadUrl = result.Value.UploadUrl };
				}
			}
			else
			{
				using (Stream stream = file.OpenReadStream())
				{
					BlobLocator locator = await client.WriteBlobAsync(stream, prefix: (prefix == null) ? Utf8String.Empty : new Utf8String(prefix), cancellationToken: cancellationToken);
					return new WriteBlobResponse { Blob = locator, SupportsRedirects = client.SupportsRedirects ? (bool?)true : null };
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
			if (!namespaceConfig.Authorize(AclAction.ReadBlobs, User))
			{
				return Forbid(AclAction.ReadBlobs, namespaceId);
			}

			IStorageClientImpl client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

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
		[Route("/api/v1/storage/{namespaceId}/nodes")]
		public async Task<ActionResult<FindNodesResponse>> FindNodesAsync(NamespaceId namespaceId, string alias, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(AclAction.ReadBlobs, User))
			{
				return Forbid(AclAction.ReadBlobs, namespaceId);
			}

			IStorageClientImpl client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

			FindNodesResponse response = new FindNodesResponse();
			await foreach (NodeHandle handle in client.FindNodesAsync(alias, cancellationToken))
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
			if (!namespaceConfig.Authorize(AclAction.WriteRefs, User))
			{
				return Forbid(AclAction.WriteRefs, namespaceId);
			}

			IStorageClient client = await _storageService.GetClientAsync(namespaceId, cancellationToken);
			NodeHandle target = new NodeHandle(request.Hash, request.Blob, request.ExportIdx);
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
			if (!namespaceConfig.Authorize(AclAction.ReadRefs, User))
			{
				return Forbid(AclAction.ReadRefs, namespaceId);
			}

			IStorageClient client = await _storageService.GetClientAsync(namespaceId, cancellationToken);

			NodeHandle? target = await client.TryReadRefTargetAsync(refName, cancellationToken: cancellationToken);
			if (target == null)
			{
				return NotFound();
			}

			return new ReadRefResponse(target);
		}
	}
}
