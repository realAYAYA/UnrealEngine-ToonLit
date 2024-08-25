// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using Horde.Server.Acls;
using Horde.Server.Artifacts;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Bson;

namespace Horde.Server.Jobs.Artifacts
{
	/// <summary>
	/// Controller for the /api/artifacts endpoint
	/// </summary>
	[ApiController]
	[Obsolete("Use /api/v2/artifacts instead")]
	[Route("[controller]")]
	public class ArtifactsControllerV1 : ControllerBase
	{
		private readonly GlobalsService _globalsService;
		private readonly IArtifactCollectionV1 _artifactCollection;
		private readonly AclService _aclService;
		private readonly JobService _jobService;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactsControllerV1(GlobalsService globalsService, IArtifactCollectionV1 artifactCollection, AclService aclService, JobService jobService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_globalsService = globalsService;
			_artifactCollection = artifactCollection;
			_aclService = aclService;
			_jobService = jobService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Creates an artifact
		/// </summary>
		/// <param name="jobId">BatchId</param>
		/// <param name="stepId">StepId</param>
		/// <param name="file">The file contents</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Http result code</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/artifacts")]
		public async Task<ActionResult<CreateJobArtifactResponseV1>> CreateArtifactAsync([FromQuery] JobId jobId, [FromQuery] JobStepId? stepId, IFormFile file, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(ArtifactAclAction.UploadArtifact, User))
			{
				return Forbid();
			}

			IJobStep? step = null;
			if (stepId != null)
			{
				foreach (IJobStepBatch batch in job.Batches)
				{
					if (batch.TryGetStep(stepId.Value, out step))
					{
						break;
					}
				}
				if (step == null)
				{
					// if the step doesn't exist in any of the batches, not found
					return NotFound();
				}
			}

			IArtifactV1 newArtifact = await _artifactCollection.CreateArtifactAsync(job.Id, step?.Id, file.FileName, file.ContentType ?? "horde-mime/unknown", file.OpenReadStream(), cancellationToken);
			return new CreateJobArtifactResponseV1(newArtifact.Id.ToString());
		}

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifactId">JobId</param>
		/// <param name="file">The file contents</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}")]
		public async Task<ActionResult<CreateJobArtifactResponseV1>> UpdateArtifactAsync(string artifactId, IFormFile file, CancellationToken cancellationToken)
		{
			IArtifactV1? artifact = await _artifactCollection.GetArtifactAsync(ObjectId.Parse(artifactId), cancellationToken);
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, ArtifactAclAction.UploadArtifact, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			await _artifactCollection.UpdateArtifactAsync(artifact, file.ContentType ?? "horde-mime/unknown", file.OpenReadStream(), cancellationToken);
			return Ok();
		}

		/// <summary>
		/// Query artifacts for a job step
		/// </summary>
		/// <param name="jobId">Optional JobId to filter by</param>
		/// <param name="stepId">Optional StepId to filter by</param>
		/// <param name="code">Whether to generate a direct download code</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts")]
		[ProducesResponseType(typeof(List<GetJobArtifactResponseV1>), 200)]
		public async Task<ActionResult<List<object>>> GetArtifactsAsync([FromQuery] JobId jobId, [FromQuery] JobStepId? stepId = null, [FromQuery] bool code = false, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			if (!await _jobService.AuthorizeAsync(jobId, ArtifactAclAction.DownloadArtifact, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			string? downloadCode = code ? (string?)await GetDirectDownloadCodeForJobAsync(jobId, cancellationToken) : null;

			IReadOnlyList<IArtifactV1> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepId, null, cancellationToken);
			return artifacts.ConvertAll(x => new GetJobArtifactResponseV1(x, downloadCode).ApplyFilter(filter));
		}

		/// <summary>
		/// Gets the claim required to download artifacts for a particular job
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <returns>The required claim</returns>
		static Claim GetDirectDownloadClaim(JobId jobId)
		{
			return new Claim(HordeClaimTypes.JobArtifacts, jobId.ToString());
		}

		/// <summary>
		/// Get a download code for the artifacts of a job
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The download code</returns>
		async ValueTask<string> GetDirectDownloadCodeForJobAsync(JobId jobId, CancellationToken cancellationToken)
		{
			Claim downloadClaim = GetDirectDownloadClaim(jobId);
			return await _aclService.IssueBearerTokenAsync(new[] { downloadClaim }, TimeSpan.FromHours(4.0), cancellationToken);
		}

		/// <summary>
		/// Retrieve metadata about a specific artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <param name="code">Whether to generate a direct download code</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}")]
		[ProducesResponseType(typeof(GetJobArtifactResponseV1), 200)]
		public async Task<ActionResult<object>> GetArtifactAsync(string artifactId, bool code = false, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IArtifactV1? artifact = await _artifactCollection.GetArtifactAsync(ObjectId.Parse(artifactId), cancellationToken);
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, ArtifactAclAction.DownloadArtifact, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			string? downloadCode = code ? (string?)await GetDirectDownloadCodeForJobAsync(artifact.JobId, cancellationToken) : null;
			return new GetJobArtifactResponseV1(artifact, downloadCode).ApplyFilter(filter);
		}

		/// <summary>
		/// Retrieve raw data for an artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}/data")]
		public async Task<ActionResult> GetArtifactDataAsync(string artifactId, CancellationToken cancellationToken)
		{
			// Catch case clients are sending an undefined artifact id
			if (artifactId == "undefined")
			{
				return NotFound();
			}

			IArtifactV1? artifact = await _artifactCollection.GetArtifactAsync(ObjectId.Parse(artifactId), cancellationToken);
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, ArtifactAclAction.DownloadArtifact, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			// Fun, filestream result automatically closes the stream!
			return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact, cancellationToken), artifact.MimeType);
		}

		/// <summary>
		/// Retrieve raw data for an artifact by filename
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="filename">Filename of artifact from step</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/steps/{stepId}/artifacts/{filename}/data")]
		public async Task<ActionResult<object>> GetArtifactDataByFilenameAsync(JobId jobId, JobStepId stepId, string filename, CancellationToken cancellationToken)
		{
			if (!await _jobService.AuthorizeAsync(jobId, ArtifactAclAction.DownloadArtifact, User, _globalConfig.Value, cancellationToken))
			{
				return Forbid();
			}

			IReadOnlyList<IArtifactV1> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepId, filename, cancellationToken);
			if (artifacts.Count == 0)
			{
				return NotFound();
			}

			IArtifactV1 artifact = artifacts[0];
			return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact, cancellationToken), artifact.MimeType);
		}

		/// <summary>
		/// Retrieve raw data for an artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <param name="code">The authorization code for this resource</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/artifacts/{artifactId}/download")]
		public async Task<ActionResult> DownloadArtifactAsync(string artifactId, [FromQuery] string code, CancellationToken cancellationToken)
		{
			IGlobals globals = await _globalsService.GetAsync(cancellationToken);

			TokenValidationParameters parameters = new TokenValidationParameters();
			parameters.ValidateAudience = false;
			parameters.RequireExpirationTime = true;
			parameters.ValidateLifetime = true;
			parameters.ValidIssuer = globals.JwtIssuer;
			parameters.ValidateIssuer = true;
			parameters.ValidateIssuerSigningKey = true;
			parameters.IssuerSigningKey = globals.JwtSigningKey;

			JwtSecurityTokenHandler handler = new JwtSecurityTokenHandler();
			TokenValidationResult tokenResult = await handler.ValidateTokenAsync(code, parameters);

			IArtifactV1? artifact = await _artifactCollection.GetArtifactAsync(ObjectId.Parse(artifactId), cancellationToken);
			if (artifact == null)
			{
				return NotFound();
			}

			Claim directDownloadClaim = GetDirectDownloadClaim(artifact.JobId);
			if (!tokenResult.ClaimsIdentity.HasClaim(directDownloadClaim.Type, directDownloadClaim.Value))
			{
				return Forbid();
			}

			return new InlineFileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact, cancellationToken), artifact.MimeType, Path.GetFileName(artifact.Name));
		}

		/// <summary>
		/// Returns a zip archive of many artifacts
		/// </summary>
		/// <param name="artifactZipRequest">Artifact request params</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Zip of many artifacts</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/artifacts/zip")]
		public async Task<ActionResult> ZipArtifactsAsync(GetJobArtifactZipRequestV1 artifactZipRequest, CancellationToken cancellationToken)
		{
			if (artifactZipRequest.JobId == null)
			{
				return BadRequest("Must specify a JobId");
			}

			IJob? job = await _jobService.GetJobAsync(artifactZipRequest.JobId.Value, cancellationToken);
			if (job == null)
			{
				return NotFound();
			}
			if (!_globalConfig.Value.Authorize(job, ArtifactAclAction.DownloadArtifact, User))
			{
				return Forbid();
			}

			IReadOnlyList<IArtifactV1> artifacts = await _artifactCollection.GetArtifactsAsync(job.Id, artifactZipRequest.StepId, null, cancellationToken);

			Dictionary<ObjectId, IArtifactV1> idToArtifact = artifacts.ToDictionary(x => x.Id, x => x);

			IReadOnlyList<IArtifactV1> zipArtifacts;
			if (artifactZipRequest.ArtifactIds == null)
			{
				zipArtifacts = artifacts;
			}
			else
			{
				List<IArtifactV1> filteredZipArtifacts = new List<IArtifactV1>();
				foreach (string artifactId in artifactZipRequest.ArtifactIds)
				{
					IArtifactV1? artifact;
					if (idToArtifact.TryGetValue(ObjectId.Parse(artifactId), out artifact))
					{
						filteredZipArtifacts.Add(artifact);
					}
					else
					{
						return NotFound();
					}
				}
				zipArtifacts = filteredZipArtifacts;
			}

			IGraph graph = await _jobService.GetGraphAsync(job, cancellationToken);

			return new CustomFileCallbackResult("Artifacts.zip", "application/octet-stream", false, async (outputStream, context) =>
			{
				// Make an unseekable MemoryStream for the ZipArchive. We have to do this because the ZipEntry stream falls back to a synchronous write to it's own stream wrappers.
				using (CustomBufferStream zipOutputStream = new CustomBufferStream())
				{
					// Keep the stream open after dispose so we can write the EOF bits.
					using (ZipArchive zipArchive = new ZipArchive(zipOutputStream, ZipArchiveMode.Create, true))
					{
						foreach (IArtifactV1 artifact in zipArtifacts)
						{
							await using (System.IO.Stream artifactStream = await _artifactCollection.OpenArtifactReadStreamAsync(artifact, cancellationToken))
							{
								// tack on the step name into the directory if it exists
								string stepName = String.Empty;
								if (artifact.StepId.HasValue)
								{
									foreach (IJobStepBatch batch in job.Batches)
									{
										IJobStep? step;
										if (batch.TryGetStep(artifact.StepId.Value, out step))
										{
											stepName = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx].Name;
											break;
										}
									}
								}

								ZipArchiveEntry zipEntry = zipArchive.CreateEntry(artifact.Name);
								using (System.IO.Stream entryStream = zipEntry.Open())
								{
									byte[] buffer = new byte[4096];
									int totalBytesRead = 0;
									while (totalBytesRead < artifact.Length)
									{
										int bytesRead = await artifactStream.ReadAsync(buffer, 0, buffer.Length);

										// Write bytes to the entry stream.  Also advances the MemStream pos.
										await entryStream.WriteAsync(buffer, 0, bytesRead);
										// Dump what we have to the output stream
										await outputStream.WriteAsync(zipOutputStream.GetBuffer(), 0, (int)zipOutputStream.Position);

										// Reset everything.
										zipOutputStream.Position = 0;
										zipOutputStream.SetLength(0);
										totalBytesRead += bytesRead;
									}
								}
							}
						}
					}
					// Write out the EOF stuff
					zipOutputStream.Position = 0;
					await zipOutputStream.CopyToAsync(outputStream);
				}
			});
		}
	}
}
