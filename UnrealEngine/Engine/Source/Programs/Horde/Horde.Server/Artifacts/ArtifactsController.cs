// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http.Headers;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Options;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Describes an artifact
	/// </summary>
	public class GetArtifactResponse
	{
		readonly IArtifact _artifact;

		/// <inheritdoc cref="IArtifact.Id"/>
		public string Id => _artifact.Id.ToString();

		/// <inheritdoc cref="IArtifact.Type"/>
		public ArtifactType Type => _artifact.Type;

		/// <inheritdoc cref="IArtifact.Keys"/>
		public IReadOnlyList<string> Keys => _artifact.Keys;

		internal GetArtifactResponse(IArtifact artifact) => _artifact = artifact;
	}

	/// <summary>
	/// Result of an artifact search
	/// </summary>
	public class FindArtifactsResponse
	{
		/// <summary>
		/// List of artifacts matching the search criteria
		/// </summary>
		public List<GetArtifactResponse> Artifacts { get; } = new List<GetArtifactResponse>();
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactFileEntryResponse
	{
		readonly FileEntry _entry;

		/// <inheritdoc cref="FileEntry.Name"/>
		public string Name => _entry.Name.ToString();

		/// <inheritdoc cref="FileEntry.Length"/>
		public long Length => _entry.Length;

		/// <inheritdoc cref="FileEntry.Hash"/>
		public IoHash Hash => _entry.Hash;

		internal GetArtifactFileEntryResponse(FileEntry entry) => _entry = entry;
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactDirectoryEntryResponse : GetArtifactDirectoryResponse
	{
		readonly DirectoryEntry _entry;

		/// <inheritdoc cref="FileEntry.Name"/>
		public string Name => _entry.Name.ToString();

		/// <inheritdoc cref="FileEntry.Length"/>
		public long Length => _entry.Length;

		/// <inheritdoc cref="FileEntry.Hash"/>
		public IoHash Hash => _entry.Handle.Hash;

		internal GetArtifactDirectoryEntryResponse(DirectoryEntry entry) => _entry = entry;
	}

	/// <summary>
	/// Describes a directory within an artifact
	/// </summary>
	public class GetArtifactDirectoryResponse
	{
		/// <summary>
		/// Names of sub-directories
		/// </summary>
		public List<GetArtifactDirectoryEntryResponse>? Directories { get; internal set; }
			
		/// <summary>
		/// Files within the directory
		/// </summary>
		public List<GetArtifactFileEntryResponse>? Files { get; internal set; }
	}

	/// <summary>
	/// Request to create a zip file with artifact data
	/// </summary>
	public class CreateZipRequest
	{
		/// <summary>
		/// Filter lines for the zip. Uses standard <see cref="FileFilter"/> syntax.
		/// </summary>
		public List<string> Filter { get; set; } = new List<string>();
	}

	/// <summary>
	/// Public interface for artifacts
	/// </summary>
	[Authorize]
	[ApiController]
	public class ArtifactsController : HordeControllerBase
	{
		readonly IArtifactCollection _artifactCollection;
		readonly StorageService _storageService;
		readonly GlobalConfig _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactsController(IArtifactCollection artifactCollection, StorageService storageService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_artifactCollection = artifactCollection;
			_storageService = storageService;
			_globalConfig = globalConfig.Value;
		}

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="filter">Filter for returned properties</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}")]
		[ProducesResponseType(typeof(FindArtifactsResponse), 200)]
		public async Task<ActionResult<object>> GetArtifactAsync(ArtifactId id, [FromQuery] PropertyFilter? filter = null)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, HttpContext.RequestAborted);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}

			return PropertyFilter.Apply(new GetArtifactResponse(artifact), filter);
		}

		/// <summary>
		/// Retrieves blobs for a particular artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="locator">The blob locator</param>
		/// <param name="length">Length of data to return</param>
		/// <param name="offset">Offset of the data to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/blobs/{*locator}")]
		public async Task<ActionResult<object>> ReadArtifactBlobAsync(ArtifactId id, BlobLocator locator, [FromQuery] int? offset = null, [FromQuery] int? length = null, CancellationToken cancellationToken = default)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, cancellationToken);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}
			if (!locator.BlobId.WithinFolder(artifact.RefName.Text))
			{
				return BadRequest("Invalid blob id for artifact");
			}

			return StorageController.ReadBlobInternalAsync(_storageService, artifact.NamespaceId, locator, offset, length, cancellationToken);
		}

		/// <summary>
		/// Retrieves the root blob for an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/ref")]
		public async Task<ActionResult<object>> ReadArtifactRefAsync(ArtifactId id, CancellationToken cancellationToken = default)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, cancellationToken);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}

			return StorageController.ReadRefInternalAsync(_storageService, artifact.NamespaceId, artifact.RefName, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="path">Path to fetch</param>
		/// <param name="filter">Filter for returned properties</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/browse")]
		[ProducesResponseType(typeof(GetArtifactDirectoryResponse), 200)]
		public async Task<ActionResult<object>> BrowseArtifactAsync(ArtifactId id, [FromQuery] string? path = null, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, cancellationToken);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}

			IStorageClient storageClient = await _storageService.GetClientAsync(artifact.NamespaceId, cancellationToken);

			DirectoryNode directoryNode;
			try
			{
				directoryNode = await storageClient.ReadNodeAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken);
			}
			catch (RefNameNotFoundException)
			{
				return NotFound(artifact.NamespaceId, artifact.RefName);
			}

			if (path != null)
			{
				foreach (string fragment in path.Split('/'))
				{
					DirectoryEntry? nextDirectoryEntry;
					if (!directoryNode.TryGetDirectoryEntry(fragment, out nextDirectoryEntry))
					{
						return NotFound();
					}
					directoryNode = await nextDirectoryEntry.ExpandAsync(cancellationToken);
				}
			}

			GetArtifactDirectoryResponse response = new GetArtifactDirectoryResponse();
			await ExpandDirectoriesAsync(directoryNode, 0, response, cancellationToken);
			return PropertyFilter.Apply(response, filter);
		}

		async Task ExpandDirectoriesAsync(DirectoryNode directoryNode, int depth, GetArtifactDirectoryResponse response, CancellationToken cancellationToken)
		{
			if (directoryNode.Directories.Count > 0)
			{
				response.Directories = new List<GetArtifactDirectoryEntryResponse>();
				foreach (DirectoryEntry subDirectoryEntry in directoryNode.Directories)
				{
					DirectoryNode subDirectoryNode = await subDirectoryEntry.ExpandAsync(cancellationToken);

					GetArtifactDirectoryEntryResponse subDirectoryEntryResponse = new GetArtifactDirectoryEntryResponse(subDirectoryEntry);
					if (depth == 0)
					{
						if (subDirectoryNode.Directories.Count + subDirectoryNode.Files.Count < 16)
						{
							await ExpandDirectoriesAsync(subDirectoryNode, depth + 1, subDirectoryEntryResponse, cancellationToken);
						}
					}
					else if (depth == 1)
					{
						if (subDirectoryNode.Directories.Count + subDirectoryNode.Files.Count < 8)
						{
							await ExpandDirectoriesAsync(subDirectoryNode, depth + 1, subDirectoryEntryResponse, cancellationToken);
						}
					}
					else if (depth < 10)
					{
						if (subDirectoryNode.Directories.Count == 1 && subDirectoryNode.Files.Count == 0)
						{
							await ExpandDirectoriesAsync(subDirectoryNode, depth + 1, subDirectoryEntryResponse, cancellationToken);
						}
					}
					response.Directories.Add(subDirectoryEntryResponse);
				}
			}

			if (directoryNode.Files.Count > 0)
			{
				response.Files = directoryNode.Files.Select(x => new GetArtifactFileEntryResponse(x)).ToList();
			}
		}

		/// <summary>
		/// Downloads an individual file from an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="path">Path to fetch</param>
		/// <param name="inline">Whether to request the file be downloaded vs displayed inline</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/file")]
		public async Task<ActionResult<object>> GetFileAsync(ArtifactId id, [FromQuery] string path, [FromQuery] bool inline = false, CancellationToken cancellationToken = default)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, cancellationToken);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}

			IStorageClient storageClient = await _storageService.GetClientAsync(artifact.NamespaceId, cancellationToken);
			DirectoryNode directory = await storageClient.ReadNodeAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken);

			FileEntry? fileEntry = await directory.GetFileEntryByPathAsync(path, cancellationToken);
			if (fileEntry == null)
			{
				return NotFound($"Unable to find file {path}");
			}

			string? contentType;
			if (!new FileExtensionContentTypeProvider().TryGetContentType(path, out contentType))
			{
				contentType = "application/octet-stream";
			}

			Stream stream = fileEntry.AsStream();
			if (inline)
			{
				return new InlineFileStreamResult(stream, contentType, Path.GetFileName(path));
			}
			else
			{
				return new FileStreamResult(stream, contentType) { FileDownloadName = Path.GetFileName(path) };
			}
		}

		/// <summary>
		/// Downloads an individual file from an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="filter">Paths to include in the zip file. The post version of this request allows for more parameters than can fit in a request string.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/zip")]
		public async Task<ActionResult<object>> GetZipAsync(ArtifactId id, [FromQuery(Name = "filter")] string[]? filter, CancellationToken cancellationToken = default)
		{
			return await GetZipInternalAsync(id, filter, cancellationToken);
		}

		/// <summary>
		/// Downloads an individual file from an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="request">Filter for the zip file</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpPost]
		[Route("/api/v2/artifacts/{id}/zip")]
		public async Task<ActionResult<object>> CreateZipFromFilterAsync(ArtifactId id, CreateZipRequest request, CancellationToken cancellationToken = default)
		{
			return await GetZipInternalAsync(id, request.Filter, cancellationToken);
		}

		async Task<ActionResult> GetZipInternalAsync(ArtifactId id, IEnumerable<string>? fileFilter, CancellationToken cancellationToken)
		{
			IArtifact? artifact = await _artifactCollection.GetAsync(id, cancellationToken);
			if (artifact == null)
			{
				return NotFound(id);
			}
			if (!_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
			{
				return Forbid(ArtifactAclAction.ReadArtifact, artifact.AclScope);
			}

			FileFilter? filter = null;
			if (fileFilter != null && fileFilter.Any())
			{
				filter = new FileFilter(fileFilter);
			}

			IStorageClient storageClient = await _storageService.GetClientAsync(artifact.NamespaceId, cancellationToken);
			DirectoryNode directory = await storageClient.ReadNodeAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken);

			Stream stream = directory.AsZipStream(filter);
			return new FileStreamResult(stream, "application/zip") { FileDownloadName = $"{artifact.RefName}.zip" };
		}

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="ids">Artifact ids to return</param>
		/// <param name="keys">Keys to find</param>
		/// <param name="filter">Filter for returned values</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts")]
		[ProducesResponseType(typeof(FindArtifactsResponse), 200)]
		public async Task<ActionResult<object>> FindArtifactsAsync([FromQuery(Name = "id")] IEnumerable<ArtifactId>? ids = null, [FromQuery(Name = "key")] IEnumerable<string>? keys = null, [FromQuery] PropertyFilter? filter = null)
		{
			if ((ids == null || !ids.Any()) && (keys == null || !keys.Any()))
			{
				return BadRequest("At least one search term must be specified");
			}

			FindArtifactsResponse response = new FindArtifactsResponse();
			await foreach (IArtifact artifact in _artifactCollection.FindAsync(ids, keys, HttpContext.RequestAborted))
			{
				if (_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
				{
					response.Artifacts.Add(new GetArtifactResponse(artifact));
				}
			}

			return PropertyFilter.Apply(response, filter);
		}
	}
}
