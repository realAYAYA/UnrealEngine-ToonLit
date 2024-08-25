// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Exception thrown when stream validation fails
	/// </summary>
	public class InvalidStreamException : Exception
	{
		/// <inheritdoc/>
		public InvalidStreamException()
		{
		}

		/// <inheritdoc/>
		public InvalidStreamException(string message) : base(message)
		{
		}

		/// <inheritdoc/>
		public InvalidStreamException(string message, Exception innerEx) : base(message, innerEx)
		{
		}
	}

	/// <summary>
	/// Information about a stream
	/// </summary>
	public interface IStream
	{
		/// <summary>
		/// Name of the stream.
		/// </summary>
		StreamId Id { get; }

		/// <summary>
		/// Configuration settings for the stream
		/// </summary>
		StreamConfig Config { get; }

		/// <summary>
		/// List of templates available for this stream
		/// </summary>
		IReadOnlyDictionary<TemplateId, ITemplateRef> Templates { get; }

		/// <summary>
		/// Stream is paused for builds until specified time
		/// </summary>
		DateTime? PausedUntil { get; }

		/// <summary>
		/// Comment/reason for why the stream was paused
		/// </summary>
		string? PauseComment { get; }
	}

	/// <summary>
	/// Job template in a stream
	/// </summary>
	public interface ITemplateRef
	{
		/// <summary>
		/// The template id
		/// </summary>
		TemplateId Id { get; }

		/// <summary>
		/// Configuration of this template ref
		/// </summary>
		TemplateRefConfig Config { get; }

		/// <summary>
		/// List of schedules for this template
		/// </summary>
		ITemplateSchedule? Schedule { get; }

		/// <summary>
		/// List of template step states
		/// </summary>
		IReadOnlyList<ITemplateStep> StepStates { get; }
	}

	/// <summary>
	/// Schedule for a template
	/// </summary>
	public interface ITemplateSchedule
	{
		/// <summary>
		/// Config for this schedule
		/// </summary>
		ScheduleConfig Config { get; }

		/// <summary>
		/// Last changelist number that this was triggered for
		/// </summary>
		int LastTriggerChange { get; }

		/// <summary>
		/// Gets the last trigger time, in UTC
		/// </summary>
		DateTime LastTriggerTimeUtc { get; }

		/// <summary>
		/// List of jobs that are currently active
		/// </summary>
		IReadOnlyList<JobId> ActiveJobs { get; }
	}

	/// <summary>
	/// Information about a paused template step
	/// </summary>
	public interface ITemplateStep
	{
		/// <summary>
		/// Name of the step
		/// </summary>
		string Name { get; }

		/// <summary>
		/// User who paused the step
		/// </summary>
		UserId PausedByUserId { get; }

		/// <summary>
		/// The UTC time when the step was paused
		/// </summary>
		DateTime PauseTimeUtc { get; }
	}

	/// <summary>
	/// Extension methods for streams
	/// </summary>
	static class StreamExtensions
	{
		/// <summary>
		/// Gets the next trigger time for a schedule
		/// </summary>
		/// <param name="schedule"></param>
		/// <param name="timeZone"></param>
		/// <returns></returns>
		public static DateTime? GetNextTriggerTimeUtc(this ITemplateSchedule schedule, TimeZoneInfo timeZone)
		{
			return schedule.Config.GetNextTriggerTimeUtc(schedule.LastTriggerTimeUtc, timeZone);
		}

		/// <summary>
		/// Converts to a public response object
		/// </summary>
		/// <param name="stream">The stream object</param>
		/// <param name="apiTemplateRefs">The template refs for this stream. Passed separately because they have their own ACL.</param>
		/// <returns>New response instance</returns>
		public static GetStreamResponse ToApiResponse(this IStream stream, List<GetTemplateRefResponse> apiTemplateRefs)
		{
			return new GetStreamResponse(stream, apiTemplateRefs);
		}

		/// <summary>
		/// Converts to an RPC response object
		/// </summary>
		/// <param name="streamConfig">The stream config object</param>
		/// <returns>New response instance</returns>
		public static HordeCommon.Rpc.GetStreamResponse ToRpcResponse(this StreamConfig streamConfig)
		{
			HordeCommon.Rpc.GetStreamResponse response = new HordeCommon.Rpc.GetStreamResponse();
			response.Name = streamConfig.Name;
			response.AgentTypes.Add(streamConfig.AgentTypes.ToDictionary(x => x.Key, x => x.Value.ToRpcResponse()));
			return response;
		}

		/// <summary>
		/// Check if stream is paused for new builds
		/// </summary>
		/// <param name="stream">The stream object</param>
		/// <param name="currentTime">Current time (allow tests to pass in a fake clock)</param>
		/// <returns>If stream is paused</returns>
		public static bool IsPaused(this IStream stream, DateTime currentTime)
		{
			return stream.PausedUntil != null && stream.PausedUntil > currentTime;
		}
	}
}
