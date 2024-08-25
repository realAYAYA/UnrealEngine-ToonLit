// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Jobs;

namespace Horde.Server.Jobs.Artifacts
{
	/// <summary>
	/// Response from creating an artifact
	/// </summary>
	public class CreateJobArtifactResponseV1
	{
		/// <summary>
		/// Unique id for this artifact
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">The artifact file id</param>
		public CreateJobArtifactResponseV1(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Request to get a zip of artifacts
	/// </summary>
	public class GetJobArtifactZipRequestV1
	{
		/// <summary>
		/// Job id to get all artifacts for
		/// </summary>
		public JobId? JobId { get; set; }

		/// <summary>
		/// Step id to filter by
		/// </summary>
		public JobStepId? StepId { get; set; }

		/// <summary>
		/// Further filter by a list of artifact ids
		/// </summary>
		public List<string>? ArtifactIds { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobId">Job id to get all artifacts for</param>
		/// <param name="stepId">step to filter by</param>
		/// <param name="artifactIds">The artifact ids.  Returns all artifacts for a job </param>
		public GetJobArtifactZipRequestV1(JobId? jobId, JobStepId? stepId, List<string>? artifactIds)
		{
			JobId = jobId;
			StepId = stepId;
			ArtifactIds = artifactIds;
		}
	}

	/// <summary>
	/// Response describing an artifact
	/// </summary>
	public class GetJobArtifactResponseV1
	{
		/// <summary>
		/// Unique id of the artifact
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the artifact
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Unique id of the job for this artifact
		/// </summary>
		public string JobId { get; set; }

		/// <summary>
		/// Optional unique id of the step for this artifact
		/// </summary>
		public string? StepId { get; set; }

		/// <summary>
		/// Type of artifact
		/// </summary>
		public string MimeType { get; set; }

		/// <summary>
		/// Length of artifact
		/// </summary>
		public long Length { get; set; }

		/// <summary>
		/// Short-lived code that can be used to download this artifact through direct download links in the browser
		/// </summary>
		public string? Code { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="artifact">The artifact to construct from</param>
		/// <param name="code">The direct download code</param>
		public GetJobArtifactResponseV1(IArtifactV1 artifact, string? code)
		{
			Id = artifact.Id.ToString();
			Name = artifact.Name;
			JobId = artifact.JobId.ToString();
			StepId = artifact.StepId.ToString();
			MimeType = artifact.MimeType;
			Length = artifact.Length;
			Code = code;
		}
	}
}
