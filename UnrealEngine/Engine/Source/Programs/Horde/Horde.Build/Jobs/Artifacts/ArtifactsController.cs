// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Mime;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Bson;

namespace Horde.Build.Jobs.Artifacts
{
	using JobId = ObjectId<IJob>;

	/// <summary>
	/// Controller for the /api/artifacts endpoint
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public class ArtifactsController : ControllerBase
	{
		/// <summary>
		/// Instance of the database service
		/// </summary>
		private readonly MongoService _mongoService;

		/// <summary>
		/// Instance of the artifact collection
		/// </summary>
		private readonly IArtifactCollection _artifactCollection;

		/// <summary>
		/// Instance of the ACL service
		/// </summary>
		private readonly AclService _aclService;

		/// <summary>
		/// Instance of the Job service
		/// </summary>
		private readonly JobService _jobService;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactsController(MongoService mongoService, IArtifactCollection artifactCollection, AclService aclService, JobService jobService)
		{
			_mongoService = mongoService;
			_artifactCollection = artifactCollection;
			_aclService = aclService;
			_jobService = jobService;
		}

		/// <summary>
		/// Creates an artifact
		/// </summary>
		/// <param name="jobId">BatchId</param>
		/// <param name="stepId">StepId</param>
		/// <param name="file">The file contents</param>
		/// <returns>Http result code</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/artifacts")]
		public async Task<ActionResult<CreateArtifactResponse>> CreateArtifact([FromQuery] JobId jobId, [FromQuery]string? stepId, IFormFile file)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if(job == null)
			{
				return NotFound();
			}

			if (!await _jobService.AuthorizeAsync(job, AclAction.UploadArtifact, User, null))
			{
				return Forbid();
			}

			IJobStep? step = null;
			if(stepId != null)
			{
				foreach(IJobStepBatch batch in job.Batches)
				{
					if(batch.TryGetStep(stepId.ToSubResourceId(), out step))
					{
						break;
					}
				}
				if(step == null)
				{
					// if the step doesn't exist in any of the batches, not found
					return NotFound();
				}
			}

			IArtifact newArtifact = await _artifactCollection.CreateArtifactAsync(job.Id, step?.Id, file.FileName, file.ContentType ?? "horde-mime/unknown", file.OpenReadStream());
			return new CreateArtifactResponse(newArtifact.Id.ToString());
		}

		/// <summary>
		/// Updates an artifact
		/// </summary>
		/// <param name="artifactId">JobId</param>
		/// <param name="file">The file contents</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}")]
		public async Task<ActionResult<CreateArtifactResponse>> UpdateArtifact(string artifactId, IFormFile file)
		{
			IArtifact? artifact = await _artifactCollection.GetArtifactAsync(artifactId.ToObjectId());
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, AclAction.UploadArtifact, User, null))
			{
				return Forbid();
			}

			await _artifactCollection.UpdateArtifactAsync(artifact, file.ContentType ?? "horde-mime/unknown", file.OpenReadStream());
			return Ok();
		}

		/// <summary>
		/// Query artifacts for a job step
		/// </summary>
		/// <param name="jobId">Optional JobId to filter by</param>
		/// <param name="stepId">Optional StepId to filter by</param>
		/// <param name="code">Whether to generate a direct download code</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts")]
		[ProducesResponseType(typeof(List<GetArtifactResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetArtifacts([FromQuery] JobId jobId, [FromQuery] string? stepId = null, [FromQuery] bool code = false, [FromQuery] PropertyFilter? filter = null)
		{
			if (!await _jobService.AuthorizeAsync(jobId, AclAction.DownloadArtifact, User, null))
			{
				return Forbid();
			}

			string? downloadCode = code ? (string?)GetDirectDownloadCodeForJob(jobId) : null;

			List<IArtifact> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepId?.ToSubResourceId(), null);
			return artifacts.ConvertAll(x => new GetArtifactResponse(x, downloadCode).ApplyFilter(filter));
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
		/// <returns>The download code</returns>
		string GetDirectDownloadCodeForJob(JobId jobId)
		{
			Claim downloadClaim = GetDirectDownloadClaim(jobId);
			return _aclService.IssueBearerToken(new[] { downloadClaim }, TimeSpan.FromHours(4.0));
		}

		/// <summary>
		/// Retrieve metadata about a specific artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <param name="code">Whether to generate a direct download code</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}")]
		[ProducesResponseType(typeof(GetArtifactResponse), 200)]
		public async Task<ActionResult<object>> GetArtifact(string artifactId, bool code = false, [FromQuery] PropertyFilter? filter = null)
		{
			IArtifact? artifact = await _artifactCollection.GetArtifactAsync(artifactId.ToObjectId());
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, AclAction.DownloadArtifact, User, null))
			{
				return Forbid();
			}

			string? downloadCode = code? (string?)GetDirectDownloadCodeForJob(artifact.JobId) : null;
			return new GetArtifactResponse(artifact, downloadCode).ApplyFilter(filter);
		}

		/// <summary>
		/// Retrieve raw data for an artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/artifacts/{artifactId}/data")]
		public async Task<ActionResult> GetArtifactData(string artifactId)
		{
			IArtifact? artifact = await _artifactCollection.GetArtifactAsync(artifactId.ToObjectId());
			if (artifact == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(artifact.JobId, AclAction.DownloadArtifact, User, null))
			{
				return Forbid();
			}

			// Fun, filestream result automatically closes the stream!
			return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact), artifact.MimeType);
		}
		
		/// <summary>
		/// Retrieve raw data for an artifact by filename
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="filename">Filename of artifact from step</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/steps/{stepId}/artifacts/{filename}/data")]
		public async Task<ActionResult<object>> GetArtifactDataByFilename(JobId jobId, string stepId, string filename)
		{
			SubResourceId stepIdValue = stepId.ToSubResourceId();

			if (!await _jobService.AuthorizeAsync(jobId, AclAction.DownloadArtifact, User, null))
			{
				return Forbid();
			}
			
			List<IArtifact> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepIdValue, filename);
			if (artifacts.Count == 0)
			{
				return NotFound();
			}

			IArtifact artifact = artifacts[0];
			return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact), artifact.MimeType);
		}

		/// <summary>
		/// Class to return a file stream without the "content-disposition: attachment" header
		/// </summary>
		class InlineFileStreamResult : FileStreamResult
		{
			/// <summary>
			/// The suggested download filename
			/// </summary>
			readonly string _fileName;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="stream"></param>
			/// <param name="mimeType"></param>
			/// <param name="fileName"></param>
			public InlineFileStreamResult(System.IO.Stream stream, string mimeType, string fileName)
				: base(stream, mimeType)
			{
				_fileName = fileName;
			}

			/// <inheritdoc/>
			public override Task ExecuteResultAsync(ActionContext context)
			{
				ContentDisposition contentDisposition = new ContentDisposition();
				contentDisposition.Inline = true;
				contentDisposition.FileName = _fileName;
				context.HttpContext.Response.Headers.Add("Content-Disposition", contentDisposition.ToString());
				
				return base.ExecuteResultAsync(context);
			}
		}

		/// <summary>
		/// Retrieve raw data for an artifact
		/// </summary>
		/// <param name="artifactId">Id of the artifact to get information about</param>
		/// <param name="code">The authorization code for this resource</param>
		/// <returns>Raw artifact data</returns>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/artifacts/{artifactId}/download")]
		public async Task<ActionResult> DownloadArtifact(string artifactId, [FromQuery] string code)
		{
			TokenValidationParameters parameters = new TokenValidationParameters();
			parameters.ValidateAudience = false;
			parameters.RequireExpirationTime = true;
			parameters.ValidateLifetime = true;
			parameters.ValidIssuer = _mongoService.JwtIssuer;
			parameters.ValidateIssuer = true;
			parameters.ValidateIssuerSigningKey = true;
			parameters.IssuerSigningKey = _mongoService.JwtSigningKey;

			JwtSecurityTokenHandler handler = new JwtSecurityTokenHandler();
			ClaimsPrincipal principal = handler.ValidateToken(code, parameters, out _);

			IArtifact? artifact = await _artifactCollection.GetArtifactAsync(artifactId.ToObjectId());
			if (artifact == null)
			{
				return NotFound();
			}

			Claim directDownloadClaim = GetDirectDownloadClaim(artifact.JobId);
			if (!principal.HasClaim(directDownloadClaim.Type, directDownloadClaim.Value))
			{
				return Forbid();
			}

			return new InlineFileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact), artifact.MimeType, Path.GetFileName(artifact.Name));
		}

		/// <summary>
		/// Returns a zip archive of many artifacts
		/// </summary>
		/// <param name="artifactZipRequest">Artifact request params</param>
		/// <returns>Zip of many artifacts</returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/artifacts/zip")]
		public async Task<ActionResult> ZipArtifacts(GetArtifactZipRequest artifactZipRequest)
		{
			if (artifactZipRequest.JobId == null)
			{
				return BadRequest("Must specify a JobId");
			}

			IJob? job = await _jobService.GetJobAsync(new JobId(artifactZipRequest.JobId!));
			if (job == null)
			{
				return NotFound();
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.DownloadArtifact, User, null))
			{
				return Forbid();
			}

			List<IArtifact> artifacts = await _artifactCollection.GetArtifactsAsync(job.Id, artifactZipRequest.StepId?.ToSubResourceId(), null);

			Dictionary<ObjectId, IArtifact> idToArtifact = artifacts.ToDictionary(x => x.Id, x => x);

			List<IArtifact> zipArtifacts;
			if (artifactZipRequest.ArtifactIds == null)
			{
				zipArtifacts = artifacts;
			}
			else
			{
				zipArtifacts = new List<IArtifact>();
				foreach (string artifactId in artifactZipRequest.ArtifactIds)
				{
					IArtifact? artifact;
					if (idToArtifact.TryGetValue(artifactId.ToObjectId(), out artifact))
					{
						zipArtifacts.Add(artifact);
					}
					else
					{
						return NotFound();
					}
				}
			}

			IGraph graph = await _jobService.GetGraphAsync(job);

			return new CustomFileCallbackResult("Artifacts.zip", "application/octet-stream", false, async (outputStream, context) =>
			{
				// Make an unseekable MemoryStream for the ZipArchive. We have to do this because the ZipEntry stream falls back to a synchronous write to it's own stream wrappers.
				using (CustomBufferStream zipOutputStream = new CustomBufferStream())
				{
					// Keep the stream open after dispose so we can write the EOF bits.
					using (ZipArchive zipArchive = new ZipArchive(zipOutputStream, ZipArchiveMode.Create, true))
					{
						foreach (IArtifact artifact in zipArtifacts)
						{
							await using (System.IO.Stream artifactStream = await _artifactCollection.OpenArtifactReadStreamAsync(artifact))
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
