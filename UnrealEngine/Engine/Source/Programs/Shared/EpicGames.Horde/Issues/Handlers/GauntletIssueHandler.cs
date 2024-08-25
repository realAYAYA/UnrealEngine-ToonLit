// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular Gauntlet error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class GauntletIssueHandler : IssueHandler
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

		readonly IssueHandlerContext _context;
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <summary>
		///  Known Gauntlet events
		/// </summary>
		static readonly Dictionary<EventId, string> s_knownGauntletEvents = new Dictionary<EventId, string>
		{
			{ KnownLogEvents.Gauntlet, FrameworkPrefix},
			{ KnownLogEvents.Gauntlet_TestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_DeviceEvent, DevicePrefix},
			{ KnownLogEvents.Gauntlet_UnrealEngineTestEvent, TestPrefix},
			{ KnownLogEvents.Gauntlet_BuildDropEvent, AccessPrefix},
			{ KnownLogEvents.Gauntlet_FatalEvent, FatalPrefix}
		};

		/// <summary>
		/// Constructor
		/// </summary>
		public GauntletIssueHandler(IssueHandlerContext context) => _context = context;

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
		/// <param name="issueEvent">The log event data</param>
		/// <param name="testNames">Receives a set of the test names</param>
		/// <param name="metadata"></param>
		private static void GetNames(IssueEvent issueEvent, HashSet<IssueKey> testNames, HashSet<IssueMetadata> metadata)
		{
			foreach (JsonLogEvent line in issueEvent.Lines)
			{
				JsonDocument document = JsonDocument.Parse(line.Data);

				string? name = null;

				string? value;
				if (document.RootElement.TryGetNestedProperty("properties.Name", out value))
				{
					name = value;
				}

				if (name != null)
				{
					string prefix = GetEventPrefix(issueEvent.EventId!.Value);
					testNames.Add($"{prefix}:{name}", IssueKeyType.None);
					metadata.Add("Context", $"with {name}");
				}
			}
		}

		/// <summary>
		/// Parses symbol file or directory from a log event
		/// </summary>
		/// <param name="issueEvent">The log event data</param>
		/// <param name="paths">Receives a set of the paths</param>
		/// <param name="metadata"></param>
		private static void GetPaths(IssueEvent issueEvent, HashSet<IssueKey> paths, HashSet<IssueMetadata> metadata)
		{
			if (issueEvent.EventId == KnownLogEvents.Gauntlet_BuildDropEvent)
			{
				foreach (JsonLogEvent line in issueEvent.Lines)
				{
					JsonDocument document = JsonDocument.Parse(line.Data);

					string? path = null;

					string? value;
					if (document.RootElement.TryGetNestedProperty("properties.File", out value))
					{
						path = value;
					}
					else if (document.RootElement.TryGetNestedProperty("properties.Directory", out value))
					{
						path = value;
					}

					if (path != null)
					{
						paths.Add($"{AccessPrefix}:{path}", IssueKeyType.None);
						metadata.Add("Context", $"with {path}");
					}
				}
			}
		}

		/// <summary>
		/// Produce a hash from error message
		/// </summary>
		/// <param name="message">The log event message</param>
		/// <param name="keys">Receives a set of the keys</param>
		/// <param name="metadata">Receives a set of metadata</param>
		private void GetHash(string message, HashSet<IssueKey> keys, HashSet<IssueMetadata> metadata)
		{
			string error = message.Length > MaxMessageLength ? message.Substring(0, MaxMessageLength) : message;

			if (TryGetHash(error, out Md5Hash hash))
			{
				keys.Add($"hash:{hash}:stream:{_context.StreamId}", IssueKeyType.None);
			}
			else
			{
				keys.Add($"{_context.NodeName}", IssueKeyType.None);
			}
			metadata.Add("Context", $"in {_context.NodeName}");
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
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId != null && IsMatchingEventId(issueEvent.EventId.Value))
			{
				IssueEventGroup issue = new IssueEventGroup("Gauntlet", "Gauntlet {Meta:Type} {Severity} {Meta:Context}", IssueChangeFilter.Code);
				issue.Events.Add(issueEvent);

				GetNames(issueEvent, issue.Keys, issue.Metadata);
				GetPaths(issueEvent, issue.Keys, issue.Metadata);
				if (issue.Keys.Count == 0)
				{
					GetHash(issueEvent.Message, issue.Keys, issue.Metadata);
				}

				issue.Metadata.Add(new IssueMetadata("type", GetEventPrefix(issueEvent.EventId.Value)));
				_issues.Add(issue);

				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
