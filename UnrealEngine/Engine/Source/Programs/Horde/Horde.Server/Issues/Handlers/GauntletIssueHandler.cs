// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular Gauntlet error
	/// </summary>
	class GauntletIssueHandler : IssueHandler
	{
		/// <summary>
		/// Prefix for framework keys
		/// </summary>
		const string FrameworkPrefix = "framework";

		/// <summary>
		/// Prefix for test keys
		/// </summary>
		const string TestPrefix = "test";

		/// <summary>
		/// Prefix for device keys
		/// </summary>
		const string DevicePrefix = "device";

		/// <summary>
		/// Prefix for access keys
		/// </summary>
		const string AccessPrefix = "access";

		/// <summary>
		/// Prefix for fatal failure keys
		/// </summary>
		const string FatalPrefix = "fatal";

		/// <summary>
		/// Max Message Length to hash
		/// </summary>
		const int MaxMessageLength = 2000;

		/// <inheritdoc/>
		public override string Type => "Gauntlet";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		///  Known Gauntlet events
		/// </summary>
		static readonly Dictionary<EventId, string> s_knownGauntletEvents = new Dictionary<EventId, string> {
			{ KnownLogEvents.Gauntlet, FrameworkPrefix},
			{ KnownLogEvents.Gauntlet_TestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_DeviceEvent, DevicePrefix},
			{ KnownLogEvents.Gauntlet_UnrealEngineTestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_BuildDropEvent, AccessPrefix},
			{ KnownLogEvents.Gauntlet_FatalEvent, FatalPrefix}
		};


		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return s_knownGauntletEvents.ContainsKey(eventId);
		}

		/// <summary>
		/// Return the prefix string associate with the event id
		/// </summary>
		/// <param name="eventId">The event id to get the information from</param>
		/// <returns>The corresponding prefix as a string</returns>
		public static string GetEventPrefix(EventId eventId)
		{
			return s_knownGauntletEvents[eventId];
		}

		/// <summary>
		/// Parses symbol names from a log event
		/// </summary>
		/// <param name="eventData">The log event data</param>
		/// <param name="testNames">Receives a set of the test names</param>
		/// <param name="metadata"></param>
		private static void GetNames(ILogEventData eventData, HashSet<string> testNames, HashSet<string> metadata)
		{
			foreach (ILogEventLine line in eventData.Lines)
			{
				string? name = null;

				string? value;
				if (line.Data.TryGetNestedProperty("properties.Name", out value))
				{
					name = value;
				}

				if (name != null)
				{
					string prefix = GetEventPrefix(eventData.EventId!.Value);
					testNames.Add($"{prefix}:{name}");
					metadata.Add($"context=with {name}");
				}
			}
		}

		/// <summary>
		/// Parses symbol file or directory from a log event
		/// </summary>
		/// <param name="eventData">The log event data</param>
		/// <param name="paths">Receives a set of the paths</param>
		/// <param name="metadata"></param>
		private static void GetPaths(ILogEventData eventData, HashSet<string> paths, HashSet<string> metadata)
		{
			if(eventData.EventId == KnownLogEvents.Gauntlet_BuildDropEvent)
			{
				foreach (ILogEventLine line in eventData.Lines)
				{
					string? path = null;

					string? value;
					if (line.Data.TryGetNestedProperty("properties.File", out value))
					{
						path = value;
					}
					else if (line.Data.TryGetNestedProperty("properties.Directory", out value))
					{
						path = value;
					}

					if (path != null)
					{
						paths.Add($"{AccessPrefix}:{path}");
						metadata.Add($"context=with {path}");
					}
				}
			}
		}

		/// <summary>
		/// Produce a hash from error message
		/// </summary>
		/// <param name="job">The job that spawned the event</param>
		/// <param name="eventData">The log event data</param>
		/// <param name="keys">Receives a set of the keys</param>
		/// <param name="metadata">Receives a set of metadata</param>
		/// <param name="node">Node that was executed</param>
		private static void GetHash(IJob job, ILogEventData eventData, HashSet<string> keys, HashSet<string> metadata, INode node)
		{

			string error = eventData.Message.Length > MaxMessageLength? eventData.Message.Substring(0, MaxMessageLength): eventData.Message;

			if (TryGetHash(error, out Md5Hash hash))
			{
				keys.Add($"hash:{hash}:stream:{job.StreamId}");
			}
			else
			{
				keys.Add($"{node.Name}");
			}
			metadata.Add($"context=in {node.Name}");
		}

		private static bool TryGetHash(string message, out Md5Hash hash)
		{
			string sanitized = message.ToUpperInvariant();
			sanitized = Regex.Replace(sanitized, @"(?<![a-zA-Z])(?:[A-Z]:|/)[^ :]+[/\\]SYNC[/\\]", "{root}/"); // Redact things that look like workspace roots; may be different between agents
			sanitized = Regex.Replace(sanitized, @"0[xX][0-9a-fA-F]+", "H"); // Redact hex strings
			sanitized = Regex.Replace(sanitized, @"\d[\d.,:]*", "n"); // Redact numbers and timestamp like things

			if (sanitized.Length > 30)
			{
				hash = Md5Hash.Compute(Encoding.UTF8.GetBytes(sanitized));
				return true;
			}
			else
			{
				hash = Md5Hash.Zero;
				return false;
			}
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null && IsMatchingEventId(stepEvent.EventId.Value))
				{
					HashSet<string> keys = new HashSet<string>();
					HashSet<string> metadata = new HashSet<string>();
					GetNames(stepEvent.EventData, keys, metadata);
					GetPaths(stepEvent.EventData, keys, metadata);
					if(keys.Count == 0)
					{
						GetHash(job, stepEvent.EventData, keys, metadata, node);
					}
					metadata.Add($"type={GetEventPrefix(stepEvent.EventId.Value)}");

					stepEvent.Fingerprint = new NewIssueFingerprint(Type, keys, null, metadata);
				}
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> changes)
		{
			foreach (SuspectChange change in changes)
			{
				if (change.ContainsCode)
				{
					change.Rank += 10;
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string Title = fingerprint.GetMetadataValues("type").FirstOrDefault() ?? "unknown";
			string severityText = (severity == IssueSeverity.Warning) ? "warnings" : "errors";
			string[] Errors = fingerprint.GetMetadataValues("context").ToArray();
			return $"{Type} {Title} {severityText} {StringUtils.FormatList(Errors, 2)}";
		}
	}
}
