// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Horde.Build.Perforce;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Jobs.Schedules
{
	using JobId = ObjectId<IJob>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Specifies a pattern of times that this schedule should run. Each schedule may have multiple patterns.
	/// </summary>
	public class SchedulePattern
	{
		/// <summary>
		/// Which days of the week the schedule should run
		/// </summary>
		public List<DayOfWeek>? DaysOfWeek { get; set; }

		/// <summary>
		/// Time during the day for the first schedule to trigger. Measured in minutes from midnight.
		/// </summary>
		public int MinTime { get; set; }

		/// <summary>
		/// Time during the day for the last schedule to trigger. Measured in minutes from midnight.
		/// </summary>
		public int? MaxTime { get; set; }

		/// <summary>
		/// Interval between each schedule triggering
		/// </summary>
		public int? Interval { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		private SchedulePattern()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="daysOfWeek">Which days of the week the schedule should run</param>
		/// <param name="minTime">Time during the day for the first schedule to trigger. Measured in minutes from midnight.</param>
		/// <param name="maxTime">Time during the day for the last schedule to trigger. Measured in minutes from midnight.</param>
		/// <param name="interval">Interval between each schedule triggering</param>
		public SchedulePattern(List<DayOfWeek>? daysOfWeek, int minTime, int? maxTime, int? interval)
		{
			DaysOfWeek = daysOfWeek;
			MinTime = minTime;
			MaxTime = maxTime;
			Interval = interval;
		}

		/// <summary>
		/// Calculates the trigger index based on the given time in minutes
		/// </summary>
		/// <param name="lastTimeUtc">Time for the last trigger</param>
		/// <param name="timeZone">The timezone for running the schedule</param>
		/// <returns>Index of the trigger</returns>
		public DateTime GetNextTriggerTimeUtc(DateTime lastTimeUtc, TimeZoneInfo timeZone)
		{
			// Convert last time into the correct timezone for running the scheule
			DateTimeOffset lastTime = TimeZoneInfo.ConvertTime((DateTimeOffset)lastTimeUtc, timeZone);

			// Get the base time (ie. the start of this day) for anchoring the schedule
			DateTimeOffset baseTime = new DateTimeOffset(lastTime.Year, lastTime.Month, lastTime.Day, 0, 0, 0, lastTime.Offset);
			for (; ; )
			{
				if (DaysOfWeek == null || DaysOfWeek.Contains(baseTime.DayOfWeek))
				{
					// Get the last time in minutes from the start of this day
					int lastTimeMinutes = (int)(lastTime - baseTime).TotalMinutes;

					// Get the time of the first trigger of this day. If the last time is less than this, this is the next trigger.
					if (lastTimeMinutes < MinTime)
					{
						return baseTime.AddMinutes(MinTime).UtcDateTime;
					}

					// Otherwise, get the time for the last trigger in the day.
					if (Interval.HasValue && Interval.Value > 0)
					{
						int actualMaxTime = MaxTime ?? ((24 * 60) - 1);
						if (lastTimeMinutes < actualMaxTime)
						{
							int lastIndex = (lastTimeMinutes - MinTime) / Interval.Value;
							int nextIndex = lastIndex + 1;

							int nextTimeMinutes = MinTime + (nextIndex * Interval.Value);
							if (nextTimeMinutes <= actualMaxTime)
							{
								return baseTime.AddMinutes(nextTimeMinutes).UtcDateTime;
							}
						}
					}
				}
				baseTime = baseTime.AddDays(1.0);
			}
		}
	}

	/// <summary>
	/// Gate allowing a schedule to trigger.
	/// </summary>
	public class ScheduleGate
	{
		/// <summary>
		/// The template containing the dependency
		/// </summary>
		public TemplateRefId TemplateRefId { get; set; }

		/// <summary>
		/// Target to wait for
		/// </summary>
		public string Target { get; set; }

		/// <summary>
		/// Private constructor for serializtaion
		/// </summary>
		[BsonConstructor]
		private ScheduleGate()
		{
			Target = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="templateRefId">The template containing the dependency</param>
		/// <param name="target">Target to wait for</param>
		public ScheduleGate(TemplateRefId templateRefId, string target)
		{
			TemplateRefId = templateRefId;
			Target = target;
		}
	}

	/// <summary>
	/// Represents a schedule
	/// </summary>
	public class Schedule
	{
		/// <summary>
		/// Whether this schedule is currently enabled
		/// </summary>
		public bool Enabled { get; set; }

		/// <summary>
		/// Maximum number of builds triggered by this schedule active at once. Set to zero for unlimited.
		/// </summary>
		public int MaxActive { get; set; }

		/// <summary>
		/// Maximum number of changes the schedule can fall behind head revision. If greater than zero, builds will be triggered for every submitted changelist until the backlog is this size.
		/// </summary>
		public int MaxChanges { get; set; }

		/// <summary>
		/// Whether the build requires a change to be submitted
		/// </summary>
		[BsonIgnoreIfDefault, BsonDefaultValue(true)]
		public bool RequireSubmittedChange { get; set; } = true;

		/// <summary>
		/// Reference to another job/target which much succeed for this schedule to trigger
		/// </summary>
		public ScheduleGate? Gate { get; set; }

		/// <summary>
		/// Whether this build requires a code change to trigger
		/// </summary>
		public List<ChangeContentFlags>? Filter { get; set; }

		/// <summary>
		/// File paths which should trigger this schedule
		/// </summary>
		public List<string>? Files { get; set; }

		/// <summary>
		/// Parameters for the template
		/// </summary>
		public Dictionary<string, string> TemplateParameters { get; set; }

		/// <summary>
		/// List of patterns for this schedule
		/// </summary>
		public List<SchedulePattern> Patterns { get; set; }

		/// <summary>
		/// Last changelist number that this was triggered for
		/// </summary>
		public int LastTriggerChange { get; set; }

		/// <summary>
		/// Last time that the schedule was triggered
		/// </summary>
		[BsonIgnoreIfNull]
		public DateTimeOffset? LastTriggerTime { get; set; }

		/// <summary>
		/// Gets the last trigger time, in UTC
		/// </summary>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Naming", "CA1721:Property names should not match get methods", Justification = "<Pending>")]
		public DateTime LastTriggerTimeUtc { get; set; }

		/// <summary>
		/// List of jobs that are currently active
		/// </summary>
		public List<JobId> ActiveJobs { get; set; } = new List<JobId>();

		/// <summary>
		/// Private constructor for serializtaion
		/// </summary>
		[BsonConstructor]
		private Schedule()
		{
			TemplateParameters = new Dictionary<string, string>();
			Patterns = new List<SchedulePattern>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="currentTimeUtc">The current time. This will be used to seed the last trigger time.</param>
		/// <param name="enabled">Whether the schedule is currently enabled</param>
		/// <param name="maxActive">Maximum number of builds that may be active at once</param>
		/// <param name="maxChanges">Maximum number of changes the schedule can fall behind head revision</param>
		/// <param name="requireSubmittedChange">Whether a change has to be submitted for the schedule to trigger</param>
		/// <param name="gate">Reference to another job/target which much succeed for this schedule to trigger</param>
		/// <param name="filter">Filter for changes to consider</param>
		/// <param name="files">Files that should trigger the schedule</param>
		/// <param name="templateParameters">Parameters for the template to run</param>
		/// <param name="patterns">List of patterns for the schedule</param>
		public Schedule(DateTime currentTimeUtc, bool enabled = true, int maxActive = 0, int maxChanges = 0, bool requireSubmittedChange = true, ScheduleGate? gate = null, List<ChangeContentFlags>? filter = null, List<string>? files = null, Dictionary<string, string>? templateParameters = null, List<SchedulePattern>? patterns = null)
		{
			Enabled = enabled;
			MaxActive = maxActive;
			MaxChanges = maxChanges;
			RequireSubmittedChange = requireSubmittedChange;
			Gate = gate;
			Filter = filter;
			Files = files;
			TemplateParameters = templateParameters ?? new Dictionary<string, string>();
			Patterns = patterns ?? new List<SchedulePattern>();
			LastTriggerTimeUtc = currentTimeUtc;
		}

		/// <summary>
		/// Copies the state fields from another schedule object
		/// </summary>
		/// <param name="other">The schedule object to copy from</param>
		public void CopyState(Schedule other)
		{
			LastTriggerChange = other.LastTriggerChange;
			LastTriggerTime = other.LastTriggerTime;
			ActiveJobs.AddRange(other.ActiveJobs);
		}

		/// <summary>
		/// Gets the flags to filter changes by
		/// </summary>
		/// <returns>Set of filter flags</returns>
		public ChangeContentFlags? GetFilterFlags()
		{
			if(Filter == null)
			{
				return null;
			}

			ChangeContentFlags flags = 0;
			foreach (ChangeContentFlags flag in Filter)
			{
				flags |= flag;
			}
			return flags;
		}

		/// <summary>
		/// Gets the last time that the schedule triggered
		/// </summary>
		/// <returns>Last trigger time</returns>
		public DateTime GetLastTriggerTimeUtc()
		{
			if (LastTriggerTime != null)
			{
				LastTriggerTimeUtc = LastTriggerTime.Value.UtcDateTime;
				LastTriggerTime = null;
			}
			return LastTriggerTimeUtc;
		}

		/// <summary>
		/// Get the next time that the schedule will trigger
		/// </summary>
		/// <param name="timeZone">Timezone to evaluate the trigger</param>
		/// <returns>Next time at which the schedule will trigger</returns>
		public DateTime? GetNextTriggerTimeUtc(TimeZoneInfo timeZone)
		{
			return GetNextTriggerTimeUtc(GetLastTriggerTimeUtc(), timeZone);
		}

		/// <summary>
		/// Get the next time that the schedule will trigger
		/// </summary>
		/// <param name="lastTimeUtc">Last time at which the schedule triggered</param>
		/// <param name="timeZone">Timezone to evaluate the trigger</param>
		/// <returns>Next time at which the schedule will trigger</returns>
		public DateTime? GetNextTriggerTimeUtc(DateTime lastTimeUtc, TimeZoneInfo timeZone)
		{
			DateTime? nextTriggerTimeUtc = null;
			if (Enabled)
			{
				foreach (SchedulePattern pattern in Patterns)
				{
					DateTime patternTriggerTime = pattern.GetNextTriggerTimeUtc(lastTimeUtc, timeZone);
					if (nextTriggerTimeUtc == null || patternTriggerTime < nextTriggerTimeUtc)
					{
						nextTriggerTimeUtc = patternTriggerTime;
					}
				}
			}
			return nextTriggerTimeUtc;
		}
	}
}
