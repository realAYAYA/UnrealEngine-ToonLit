// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Mime;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Streams;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Acls;
using Horde.Server.Agents.Leases;
using Horde.Server.Jobs;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon.Rpc.Tasks;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Public interface for artifacts
	/// </summary>
	[Authorize]
	[ApiController]
	public class ArtifactsController : HordeControllerBase
	{
		readonly IArtifactCollection _artifactCollection;
		readonly StorageService _storageService;
		readonly ILeaseCollection _leaseCollection;
		readonly IJobCollection _jobCollection;
		readonly AclService _aclService;
		readonly GlobalConfig _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactsController(IArtifactCollection artifactCollection, StorageService storageService, ILeaseCollection leaseCollection, IJobCollection jobCollection, AclService aclService, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<ArtifactsController> logger)
		{
			_artifactCollection = artifactCollection;
			_storageService = storageService;
			_leaseCollection = leaseCollection;
			_jobCollection = jobCollection;
			_aclService = aclService;
			_globalConfig = globalConfig.Value;
			_logger = logger;
		}

		/// <summary>
		/// Creates a new artifact. Actual data for the artifact can be uploaded using a storage client pointed to the blobs endpoint.
		/// </summary>
		/// <param name="request">Information about the desired artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The created artifact</returns>
		[HttpPost]
		[Route("/api/v2/artifacts")]
		public async Task<ActionResult<CreateArtifactResponse>> CreateArtifactAsync([FromBody] CreateArtifactRequest request, CancellationToken cancellationToken = default)
		{
			StreamConfig? streamConfig;
			if (request.StreamId != null && request.Change != null && _globalConfig.TryGetStream(request.StreamId.Value, out streamConfig))
			{
				if (streamConfig.Authorize(ArtifactAclAction.WriteArtifact, User))
				{
					return await CreateArtifactInternalAsync(request.Name, request.Type, request.Description, request.StreamId.Value, request.Change.Value, request.Keys, request.Metadata, AclScopeName.Root, cancellationToken);
				}
			}

			LeaseId? leaseId = User.GetLeaseClaim();
			if (leaseId != null)
			{
				ILease? lease = await _leaseCollection.GetAsync(leaseId.Value, cancellationToken);
				if (lease == null)
				{
					_logger.LogInformation("Claim has invalid lease id {LeaseId}", leaseId.Value);
					return Forbid(ArtifactAclAction.WriteArtifact);
				}

				Any payload = Any.Parser.ParseFrom(lease.Payload.ToArray());
				if (!payload.TryUnpack(out ExecuteJobTask jobTask))
				{
					_logger.LogInformation("Lease {LeaseId} is not for a job", leaseId.Value);
					return Forbid(ArtifactAclAction.WriteArtifact);
				}

				IJob? job = await _jobCollection.GetAsync(JobId.Parse(jobTask.JobId), cancellationToken);
				if (job == null)
				{
					_logger.LogInformation("Missing job {JobId} for lease {LeaseId}", JobId.Parse(jobTask.JobId), leaseId.Value);
					return Forbid(ArtifactAclAction.WriteArtifact);
				}

				IJobStepBatch? batch = job.Batches.FirstOrDefault(x => x.LeaseId == leaseId);
				if (batch == null)
				{
					_logger.LogInformation("Unable to find batch in job {JobId} for lease {LeaseId}", job.Id, leaseId.Value);
					return Forbid(ArtifactAclAction.WriteArtifact);
				}

				List<string> keys = new List<string>(request.Keys);
				keys.Add(job.GetArtifactKey());

				IJobStep? step = batch.Steps.FirstOrDefault(x => x.State == HordeCommon.JobStepState.Running);
				if (step != null)
				{
					keys.Add(job.GetArtifactKey(step));
				}

				StreamId streamId = request.StreamId ?? job.StreamId;

				AclScopeName scopeName = AclScopeName.Root;
				if (_globalConfig.TryGetTemplate(streamId, job.TemplateId, out TemplateRefConfig? templateRefConfig))
				{
					scopeName = templateRefConfig.Acl.ScopeName;
				}

				return await CreateArtifactInternalAsync(request.Name, request.Type, request.Description, streamId, request.Change ?? job.Change, keys, request.Metadata, scopeName, cancellationToken);
			}

			return Forbid(ArtifactAclAction.WriteArtifact);
		}

		async Task<ActionResult<CreateArtifactResponse>> CreateArtifactInternalAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, int change, List<string> keys, List<string> metadata, AclScopeName scopeName, CancellationToken cancellationToken)
		{
			IArtifact artifact = await _artifactCollection.AddAsync(name, type, description, streamId, change, keys, metadata, scopeName, cancellationToken);
			RefName? prevRefName = await GetPrevRefNameForArtifactAsync(artifact, cancellationToken);

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(HordeClaimTypes.ReadNamespace, $"{artifact.NamespaceId}:{ArtifactCollection.GetArtifactPath(streamId, name, type)}"));
			claims.Add(new AclClaimConfig(HordeClaimTypes.WriteNamespace, $"{artifact.NamespaceId}:{artifact.RefName}"));

			string token = await _aclService.IssueBearerTokenAsync(claims, TimeSpan.FromHours(8.0), cancellationToken);
			return new CreateArtifactResponse(artifact.Id, artifact.NamespaceId, artifact.RefName, prevRefName, token);
		}

		async Task<RefName?> GetPrevRefNameForArtifactAsync(IArtifact artifact, CancellationToken cancellationToken)
		{
			IStorageBackend storageBackend = _storageService.CreateBackend(artifact.NamespaceId);
			await foreach (IArtifact prevArtifact in _artifactCollection.FindAsync(artifact.StreamId, maxChange: artifact.Change - 1, name: artifact.Name, type: artifact.Type, cancellationToken: cancellationToken))
			{
				if (prevArtifact.NamespaceId != artifact.NamespaceId)
				{
					break;
				}

				BlobRefValue? refValue = await storageBackend.TryReadRefAsync(prevArtifact.RefName, cancellationToken: cancellationToken);
				if (refValue != null)
				{
					return prevArtifact.RefName;
				}
			}
			return null;
		}

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="filter">Filter for returned properties</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}")]
		[ProducesResponseType(typeof(GetArtifactResponse), 200)]
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

			return PropertyFilter.Apply(new GetArtifactResponse(artifact.Id, artifact.Name, artifact.Type, artifact.Description, artifact.StreamId, artifact.Change, artifact.Keys, artifact.Metadata), filter);
		}

		/// <summary>
		/// Retrieves bundles for a particular artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="locator">The blob locator</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/blobs/{*locator}")]
		public async Task<ActionResult> ReadArtifactBlobAsync(ArtifactId id, BlobLocator locator, CancellationToken cancellationToken = default)
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
			if (!locator.WithinFolder(artifact.RefName.Text))
			{
				return BadRequest("Invalid blob id for artifact");
			}

			IStorageBackend storageBackend = _storageService.CreateBackend(artifact.NamespaceId);
			return await StorageController.ReadBlobInternalAsync(storageBackend, locator, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Retrieves the root blob for an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/refs/default")]
		public async Task<ActionResult<ReadRefResponse>> ReadArtifactRefAsync(ArtifactId id, CancellationToken cancellationToken = default)
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

			return await StorageController.ReadRefInternalAsync(_storageService, artifact.NamespaceId, artifact.RefName, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="path">Path to fetch</param>
		/// <param name="search">Optional search parameter</param>
		/// <param name="filter">Filter for returned properties</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/browse")]
		[ProducesResponseType(typeof(GetArtifactDirectoryResponse), 200)]
		public async Task<ActionResult<object>> BrowseArtifactAsync(ArtifactId id, [FromQuery] string? path = null, [FromQuery] string? search = null, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
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

			using IStorageClient storageClient = _storageService.CreateClient(artifact.NamespaceId);

			DirectoryNode directoryNode;
			try
			{
				directoryNode = await storageClient.ReadRefTargetAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken: cancellationToken);
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
					directoryNode = await nextDirectoryEntry.Handle.ReadBlobAsync(cancellationToken: cancellationToken);
				}
			}

			GetArtifactDirectoryResponse response = new GetArtifactDirectoryResponse();
			if (String.IsNullOrEmpty(search))
			{
				await ExpandDirectoriesAsync(directoryNode, 0, response, cancellationToken);
			}
			else
			{
				await SearchDirectoriesAsync(directoryNode, search, response, cancellationToken);
			}
			return PropertyFilter.Apply(response, filter);
		}

		static async Task ExpandDirectoriesAsync(DirectoryNode directoryNode, int depth, GetArtifactDirectoryResponse response, CancellationToken cancellationToken)
		{
			foreach (DirectoryEntry subDirectoryEntry in directoryNode.Directories)
			{
				DirectoryNode subDirectoryNode = await subDirectoryEntry.Handle.ReadBlobAsync(cancellationToken: cancellationToken);

				GetArtifactDirectoryEntryResponse subDirectoryEntryResponse = new GetArtifactDirectoryEntryResponse(subDirectoryEntry.Name.ToString(), subDirectoryEntry.Length, subDirectoryEntry.Handle.Hash);

				if (IncludeInlineResponse(depth, subDirectoryNode.Directories.Count, subDirectoryNode.Files.Count))
				{
					await ExpandDirectoriesAsync(subDirectoryNode, depth + 1, subDirectoryEntryResponse, cancellationToken);
				}

				response.Directories ??= new List<GetArtifactDirectoryEntryResponse>();
				response.Directories.Add(subDirectoryEntryResponse);
			}

			foreach (FileEntry fileEntry in directoryNode.Files)
			{
				response.Files ??= new List<GetArtifactFileEntryResponse>();
				response.Files.Add(new GetArtifactFileEntryResponse(fileEntry.Name.ToString(), fileEntry.Length, fileEntry.StreamHash));
			}
		}

		static async Task SearchDirectoriesAsync(DirectoryNode directoryNode, string search, GetArtifactDirectoryResponse response, CancellationToken cancellationToken)
		{
			await SearchDirectoriesFullAsync(directoryNode, "", search, response, cancellationToken);
			FilterInlineResponses(0, response);
		}

		static async Task SearchDirectoriesFullAsync(DirectoryNode directoryNode, string path, string search, GetArtifactDirectoryResponse response, CancellationToken cancellationToken)
		{
			foreach (DirectoryEntry subDirectoryEntry in directoryNode.Directories)
			{
				DirectoryNode subDirectoryNode = await subDirectoryEntry.Handle.ReadBlobAsync(cancellationToken: cancellationToken);

				GetArtifactDirectoryEntryResponse subDirectoryEntryResponse = new GetArtifactDirectoryEntryResponse(subDirectoryEntry.Name.ToString(), subDirectoryEntry.Length, subDirectoryEntry.Handle.Hash);
				await SearchDirectoriesFullAsync(subDirectoryNode, $"{path}/{subDirectoryEntry.Name}", search, subDirectoryEntryResponse, cancellationToken);

				if ((subDirectoryEntryResponse.Files?.Count ?? 0) > 0 || (subDirectoryEntryResponse.Directories?.Count ?? 0) > 0)
				{
					response.Directories ??= new List<GetArtifactDirectoryEntryResponse>();
					response.Directories.Add(subDirectoryEntryResponse);
				}
			}

			foreach (FileEntry fileEntry in directoryNode.Files)
			{
				string filePath = $"{path}/{fileEntry.Name}";
				if (filePath.Contains(search, StringComparison.OrdinalIgnoreCase))
				{
					response.Files ??= new List<GetArtifactFileEntryResponse>();
					response.Files.Add(new GetArtifactFileEntryResponse(fileEntry.Name.ToString(), fileEntry.Length, fileEntry.StreamHash));
				}
			}
		}

		static void FilterInlineResponses(int depth, GetArtifactDirectoryResponse response)
		{
			if (response.Directories != null)
			{
				foreach (GetArtifactDirectoryResponse subDirResponse in response.Directories)
				{
					if (IncludeInlineResponse(depth, subDirResponse.Directories?.Count ?? 0, subDirResponse.Files?.Count ?? 0))
					{
						FilterInlineResponses(depth + 1, subDirResponse);
					}
					else
					{
						subDirResponse.Directories = null;
						subDirResponse.Files = null;
					}
				}
			}
		}

		static bool IncludeInlineResponse(int depth, int numDirectories, int numFiles)
		{
			bool result = false;
			if (depth == 0)
			{
				result = (numDirectories + numFiles) < 16;
			}
			else if (depth == 1)
			{
				result = (numDirectories + numFiles) < 8;
			}
			else if (depth < 10)
			{
				result = (numDirectories == 1 && numFiles == 0);
			}
			return result;
		}

		/// <summary>
		/// Browse to an individual file from an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="path">Path to fetch</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/browse/{*path}")]
		public Task<ActionResult<object>> BrowseFileAsync(ArtifactId id, string path, CancellationToken cancellationToken = default)
		{
			return GetFileAsync(id, path, inline: true, cancellationToken);
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

			using IStorageClient storageClient = _storageService.CreateClient(artifact.NamespaceId);
			DirectoryNode directory = await storageClient.ReadRefTargetAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken: cancellationToken);

			FileEntry? fileEntry = await directory.GetFileEntryByPathAsync(path, cancellationToken: cancellationToken);
			if (fileEntry == null)
			{
				return NotFound($"Unable to find file {path}");
			}

			string? contentType;
			if (!new FileExtensionContentTypeProvider().TryGetContentType(path, out contentType))
			{
				if (path.EndsWith(".log", StringComparison.OrdinalIgnoreCase))
				{
					contentType = MediaTypeNames.Text.Plain;
				}
				else
				{
					contentType = MediaTypeNames.Application.Octet;
				}
			}

			Stream stream = fileEntry.OpenAsStream();
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
		/// Downloads the artifact data
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="format">Format for the download type</param>
		/// <param name="filter">Paths to include. The post version of this request allows for more parameters than can fit in a request string.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts/{id}/download")]
		public async Task<ActionResult<object>> DownloadAsync(ArtifactId id, [FromQuery] DownloadArtifactFormat? format, [FromQuery(Name = "filter")] string[]? filter, CancellationToken cancellationToken = default)
		{
			return await DownloadInternalAsync(id, format, filter, cancellationToken);
		}

		/// <summary>
		/// Downloads an individual file from an artifact
		/// </summary>
		/// <param name="id">Identifier of the artifact to retrieve</param>
		/// <param name="format">Format for the download type</param>
		/// <param name="request">Filter for the zip file</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpPost]
		[Route("/api/v2/artifacts/{id}/download")]
		public async Task<ActionResult<object>> DownloadWithFilterAsync(ArtifactId id, [FromQuery] DownloadArtifactFormat? format, CreateZipRequest request, CancellationToken cancellationToken = default)
		{
			return await DownloadInternalAsync(id, format, request.Filter, cancellationToken);
		}

		async Task<ActionResult> DownloadInternalAsync(ArtifactId id, DownloadArtifactFormat? format, IReadOnlyList<string>? fileFilter, CancellationToken cancellationToken)
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

			switch (format ?? DownloadArtifactFormat.Zip)
			{
				case DownloadArtifactFormat.Zip:
					return await GetZipInternalAsync(artifact, fileFilter, cancellationToken);
				case DownloadArtifactFormat.Ugs:
					return GetDescriptorInternal(artifact, fileFilter);
				default:
					return BadRequest("Unhandled download format");
			}
		}

		async Task<ActionResult> GetZipInternalAsync(IArtifact artifact, IEnumerable<string>? fileFilter, CancellationToken cancellationToken)
		{
			FileFilter? filter = null;
			if (fileFilter != null && fileFilter.Any())
			{
				filter = new FileFilter(fileFilter);
			}

#pragma warning disable CA2000
			IStorageClient storageClient = _storageService.CreateClient(artifact.NamespaceId);
			try
			{
				IBlobRef<DirectoryNode> directory = await storageClient.ReadRefAsync<DirectoryNode>(artifact.RefName, DateTime.UtcNow.AddHours(1.0), cancellationToken: cancellationToken);

				Stream stream = directory.AsZipStream(filter).WrapOwnership(storageClient);
				return new FileStreamResult(stream, "application/zip") { FileDownloadName = $"{artifact.RefName}.zip" };
			}
			catch
			{
				storageClient.Dispose();
				throw;
			}
#pragma warning restore CA2000
		}

		ActionResult GetDescriptorInternal(IArtifact artifact, IReadOnlyList<string>? fileFilter)
		{
			Uri baseUri = new Uri(_globalConfig.ServerSettings.ServerUrl, $"api/v2/artifacts/{artifact.Id}");

			ArtifactDescriptor descriptor = new ArtifactDescriptor(baseUri, new RefName("default"), fileFilter);

			byte[] data = descriptor.Serialize();
			return new FileStreamResult(new MemoryStream(data), "application/x-horde-artifact") { FileDownloadName = $"{artifact.Name}.uartifact" };
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
		public async Task<ActionResult> GetZipAsync(ArtifactId id, [FromQuery(Name = "filter")] string[]? filter, CancellationToken cancellationToken = default)
		{
			return await DownloadInternalAsync(id, DownloadArtifactFormat.Zip, filter, cancellationToken);
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
			return await DownloadInternalAsync(id, DownloadArtifactFormat.Zip, request.Filter, cancellationToken);
		}

		/// <summary>
		/// Finds artifacts matching certain criteria
		/// </summary>
		/// <param name="streamId">Stream to search</param>
		/// <param name="minChange">Minimum changelist number for artifacts to return</param>
		/// <param name="maxChange">Maximum changelist number for artifacts to return</param>
		/// <param name="name">Artifact name</param>
		/// <param name="type">Type of the artifact</param>
		/// <param name="keys">Keys to find</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <param name="filter">Filter for returned values</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v2/artifacts")]
		[ProducesResponseType(typeof(FindArtifactsResponse), 200)]
		public async Task<ActionResult<object>> FindArtifactsAsync([FromQuery] StreamId? streamId = null, [FromQuery] int? minChange = null, [FromQuery] int? maxChange = null, [FromQuery(Name = "name")] ArtifactName? name = null, [FromQuery(Name = "type")] ArtifactType? type = null, [FromQuery(Name = "key")] IEnumerable<string>? keys = null, [FromQuery] int maxResults = 100, [FromQuery] PropertyFilter? filter = null)
		{
			if (streamId == null && keys == null)
			{
				return BadRequest("Missing streamId or key parameter");
			}

			FindArtifactsResponse response = new FindArtifactsResponse();
			await foreach (IArtifact artifact in _artifactCollection.FindAsync(streamId, minChange, maxChange, name, type, keys, maxResults, HttpContext.RequestAborted))
			{
				if (_globalConfig.Authorize(artifact.AclScope, ArtifactAclAction.ReadArtifact, User))
				{
					response.Artifacts.Add(new GetArtifactResponse(artifact.Id, artifact.Name, artifact.Type, artifact.Description, artifact.StreamId, artifact.Change, artifact.Keys, artifact.Metadata));
				}
			}

			return PropertyFilter.Apply(response, filter);
		}
	}
}
