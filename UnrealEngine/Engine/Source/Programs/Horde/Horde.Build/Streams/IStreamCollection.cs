// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Projects;
using Horde.Build.Utilities;

namespace Horde.Build.Streams
{
	using JobId = ObjectId<IJob>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Collection of stream documents
	/// </summary>
	public interface IStreamCollection
	{
		/// <summary>
		/// Creates or replaces a stream configuration
		/// </summary>
		/// <param name="id">Unique id for the new stream</param>
		/// <param name="stream">The current stream value. If not-null, this will attempt to replace the existing instance.</param>
		/// <param name="revision">The config file revision</param>
		/// <param name="projectId">The project id</param>
		/// <returns></returns>
		Task<IStream?> TryCreateOrReplaceAsync(StreamId id, IStream? stream, string revision, ProjectId projectId);

		/// <summary>
		/// Gets a stream by ID
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>The stream document</returns>
		Task<IStream?> GetAsync(StreamId streamId);

		/// <summary>
		/// Gets a stream's permissions by ID
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>The stream document</returns>
		Task<IStreamPermissions?> GetPermissionsAsync(StreamId streamId);

		/// <summary>
		/// Enumerates all streams
		/// </summary>
		/// <returns></returns>
		Task<List<IStream>> FindAllAsync();

		/// <summary>
		/// Gets all the available streams for a project
		/// </summary>
		/// <param name="projectIds">Unique id of the projects to query</param>
		/// <returns>List of stream documents</returns>
		Task<List<IStream>> FindForProjectsAsync(ProjectId[] projectIds);

		/// <summary>
		/// Updates user-facing properties for an existing stream
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="newPausedUntil">The new datetime for pausing builds</param>
		/// <param name="newPauseComment">The reason for pausing</param>
		/// <returns>The updated stream if successful, null otherwise</returns>
		Task<IStream?> TryUpdatePauseStateAsync(IStream stream, DateTime? newPausedUntil, string? newPauseComment);

		/// <summary>
		/// Attempts to update the last trigger time for a schedule
		/// </summary>
		/// <param name="stream">The stream to update</param>
		/// <param name="templateRefId">The template ref id</param>
		/// <param name="lastTriggerTimeUtc">New last trigger time for the schedule</param>
		/// <param name="lastTriggerChange">New last trigger changelist for the schedule</param>
		/// <param name="newActiveJobs">New list of active jobs</param>
		/// <returns>The updated stream if successful, null otherwise</returns>
		Task<IStream?> TryUpdateScheduleTriggerAsync(IStream stream, TemplateRefId templateRefId, DateTime? lastTriggerTimeUtc, int? lastTriggerChange, List<JobId> newActiveJobs);

		/// <summary>
		/// Attempts to update a stream template ref
		/// </summary>
		/// <param name="streamInterface">The stream containing the template ref</param>
		/// <param name="templateRefId">The template ref to update</param>
		/// <param name="stepStates">The stream states to update, pass an empty list to clear all step states, otherwise will be a partial update based on included step updates</param>
		/// <returns></returns>
		Task<IStream?> TryUpdateTemplateRefAsync(IStream streamInterface, TemplateRefId templateRefId, List<UpdateStepStateRequest>? stepStates = null);

		/// <summary>
		/// Delete a stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(StreamId streamId);
	}

	static class StreamCollectionExtensions
	{
		/// <summary>
		/// Creates or replaces a stream configuration
		/// </summary>
		/// <param name="streamCollection">The stream collection</param>
		/// <param name="id">Unique id for the new stream</param>
		/// <param name="stream">The current stream value. If not-null, this will attempt to replace the existing instance.</param>
		/// <param name="revision">The config file revision</param>
		/// <param name="projectId">The project id</param>
		/// <returns></returns>
		public static async Task<IStream> CreateOrReplaceAsync(this IStreamCollection streamCollection, StreamId id, IStream? stream, string revision, ProjectId projectId)
		{
			for (; ; )
			{
				stream = await streamCollection.TryCreateOrReplaceAsync(id, stream, revision, projectId);
				if (stream != null)
				{
					return stream;
				}
				stream = await streamCollection.GetAsync(id);
			}
		}
	}
}
