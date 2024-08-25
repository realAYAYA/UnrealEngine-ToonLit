// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Jobs;
using Horde.Server.Perforce;
using Horde.Server.Streams;

namespace Horde.Server.Jobs.Schedules
{
	/// <summary>
	/// Response describing a schedule
	/// </summary>
	public class GetScheduleResponse
	{
		/// <summary>
		/// Whether the schedule is currently enabled
		/// </summary>
		public bool Enabled { get; set; }

		/// <summary>
		/// Maximum number of scheduled jobs at once
		/// </summary>
		public int MaxActive { get; set; }

		/// <summary>
		/// Maximum number of changes the schedule can fall behind head revision. If greater than zero, builds will be triggered for every submitted changelist until the backlog is this size.
		/// </summary>
		public int MaxChanges { get; set; }

		/// <summary>
		/// Whether the build requires a change to be submitted
		/// </summary>
		public bool RequireSubmittedChange { get; set; }

		/// <summary>
		/// Gate for this schedule to trigger
		/// </summary>
		public ScheduleGateConfig? Gate { get; set; }

		/// <summary>
		/// Which commits to run this job for
		/// </summary>
		public List<CommitTag>? Commits { get; set; }

		/// <summary>
		/// The types of changes to run for
		/// </summary>
		[Obsolete("Use Commits instead")]
		public List<ChangeContentFlags>? Filter { get; set; }

		/// <summary>
		/// Parameters for the template
		/// </summary>
		public Dictionary<string, string> TemplateParameters { get; set; }

		/// <summary>
		/// New patterns for the schedule
		/// </summary>
		public List<SchedulePatternConfig> Patterns { get; set; }

		/// <summary>
		/// Last changelist number that this was triggered for
		/// </summary>
		public int LastTriggerChange { get; set; }

		/// <summary>
		/// Last time that the schedule was triggered
		/// </summary>
		public DateTimeOffset LastTriggerTime { get; set; }

		/// <summary>
		/// Next trigger times for schedule
		/// </summary>
		public List<DateTime> NextTriggerTimesUTC { get; set; }

		/// <summary>
		/// List of active jobs
		/// </summary>
		public List<JobId> ActiveJobs { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="schedule">Schedule to construct from</param>
		/// <param name="schedulerTimeZone">The scheduler time zone</param>
		public GetScheduleResponse(ITemplateSchedule schedule, TimeZoneInfo schedulerTimeZone)
		{
			Enabled = schedule.Config.Enabled;
			MaxActive = schedule.Config.MaxActive;
			MaxChanges = schedule.Config.MaxChanges;
			RequireSubmittedChange = schedule.Config.RequireSubmittedChange;
			Gate = schedule.Config.Gate;
			Commits = schedule.Config.Commits;
#pragma warning disable CS0618 // Type or member is obsolete
			Filter = schedule.Config.Filter;
#pragma warning restore CS0618 // Type or member is obsolete
			TemplateParameters = schedule.Config.TemplateParameters;
			Patterns = schedule.Config.Patterns;
			LastTriggerChange = schedule.LastTriggerChange;
			LastTriggerTime = schedule.LastTriggerTimeUtc;
			ActiveJobs = new List<JobId>(schedule.ActiveJobs);

			DateTime curTime = schedule.LastTriggerTimeUtc;
			NextTriggerTimesUTC = new List<DateTime>();
			for (int i = 0; i < 16; i++)
			{
				DateTime? nextTime = schedule.Config.GetNextTriggerTimeUtc(curTime, schedulerTimeZone);
				if (nextTime == null)
				{
					break;
				}

				curTime = nextTime.Value;
				NextTriggerTimesUTC.Add(curTime);
			}
		}
	}

	/// <summary>
	/// Response describing when a schedule is expected to trigger
	/// </summary>
	public class GetScheduleForecastResponse
	{
		/// <summary>
		/// Next trigger times
		/// </summary>
		public List<DateTime> Times { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="times">List of trigger times</param>
		public GetScheduleForecastResponse(List<DateTime> times)
		{
			Times = times;
		}
	}
}
