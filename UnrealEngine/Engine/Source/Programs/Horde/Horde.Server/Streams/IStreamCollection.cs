// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Server.Jobs;
using Horde.Server.Utilities;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Collection of stream documents
	/// </summary>
	public interface IStreamCollection
	{
		/// <summary>
		/// Gets a stream by ID
		/// </summary>
		/// <param name="streamConfig">The stream config object</param>
		/// <returns>The stream document</returns>
		Task<IStream> GetAsync(StreamConfig streamConfig);

		/// <summary>
		/// Gets a stream by ID
		/// </summary>
		/// <param name="streamConfigs">The stream config object</param>
		/// <returns>The stream document</returns>
		Task<List<IStream>> GetAsync(IReadOnlyList<StreamConfig> streamConfigs);

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
		Task<IStream?> TryUpdateScheduleTriggerAsync(IStream stream, TemplateId templateRefId, DateTime? lastTriggerTimeUtc, int? lastTriggerChange, List<JobId> newActiveJobs);

		/// <summary>
		/// Attempts to update a stream template ref
		/// </summary>
		/// <param name="streamInterface">The stream containing the template ref</param>
		/// <param name="templateRefId">The template ref to update</param>
		/// <param name="stepStates">The stream states to update, pass an empty list to clear all step states, otherwise will be a partial update based on included step updates</param>
		/// <returns></returns>
		Task<IStream?> TryUpdateTemplateRefAsync(IStream streamInterface, TemplateId templateRefId, List<UpdateStepStateRequest>? stepStates = null);
	}

	/// <summary>
	/// Extension methods for <see cref="StreamCollection"/>
	/// </summary>
	public static class StreamCollectionExtensions
	{
		/// <summary>
		/// Updates an existing stream
		/// </summary>
		/// <param name="streamCollection">The stream collection</param>
		/// <param name="stream">The stream to update</param>
		/// <param name="newPausedUntil">The new datetime for pausing builds</param>
		/// <param name="newPauseComment">The reason for pausing</param>
		/// <returns>Async task object</returns>
		public static async Task<IStream?> UpdatePauseStateAsync(this IStreamCollection streamCollection, IStream? stream, DateTime? newPausedUntil = null, string? newPauseComment = null)
		{
			for (; stream != null; stream = await streamCollection.GetAsync(stream.Config))
			{
				IStream? newStream = await streamCollection.TryUpdatePauseStateAsync(stream, newPausedUntil, newPauseComment);
				if (newStream != null)
				{
					return newStream;
				}
			}
			return null;
		}

		/// <summary>
		/// Attempts to update the last trigger time for a schedule
		/// </summary>
		/// <param name="streamCollection">The stream collection</param>
		/// <param name="stream">The stream to update</param>
		/// <param name="templateRefId">The template ref id</param>
		/// <param name="lastTriggerTimeUtc"></param>
		/// <param name="lastTriggerChange"></param>
		/// <param name="addJobs">Jobs to add</param>
		/// <param name="removeJobs">Jobs to remove</param>
		/// <returns>True if the stream was updated</returns>
		public static async Task<IStream?> UpdateScheduleTriggerAsync(this IStreamCollection streamCollection, IStream stream, TemplateId templateRefId, DateTime? lastTriggerTimeUtc = null, int? lastTriggerChange = null, List<JobId>? addJobs = null, List<JobId>? removeJobs = null)
		{
			IStream? newStream = stream;
			while (newStream != null)
			{
				ITemplateRef? templateRef;
				if (!newStream.Templates.TryGetValue(templateRefId, out templateRef))
				{
					break;
				}
				if (templateRef.Schedule == null)
				{
					break;
				}

				IEnumerable<JobId> newActiveJobs = templateRef.Schedule.ActiveJobs;
				if (removeJobs != null)
				{
					newActiveJobs = newActiveJobs.Except(removeJobs);
				}
				if (addJobs != null)
				{
					newActiveJobs = newActiveJobs.Union(addJobs);
				}

				newStream = await streamCollection.TryUpdateScheduleTriggerAsync(newStream, templateRefId, lastTriggerTimeUtc, lastTriggerChange, newActiveJobs.ToList());

				if (newStream != null)
				{
					return newStream;
				}

				newStream = await streamCollection.GetAsync(stream.Config);
			}
			return null;
		}
	}
}
