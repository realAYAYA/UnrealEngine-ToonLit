// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;

namespace Gauntlet
{
	/// <summary>
	/// Unreal-specific implementation of a TestEvent
	/// </summary>
	public class UnrealTestEvent : ITestEvent
	{
		/// <summary>
		/// Level of severity that this event represents
		/// </summary>
		public EventSeverity Severity { get; protected set; }

		// Title/Summary of event
		public string Summary { get; protected set; }

		// Event details
		public IEnumerable<string> Details { get; protected set; }

		// Callstack 
		public IEnumerable<string> Callstack { get; protected set; }

		//True if this is an ensure (Gauntlet does not define a level for this, but we log differently).
		public bool IsEnsure { get; protected set; }

		// Constructor that requires all properties
		public UnrealTestEvent(EventSeverity InSeverity, string InSummary, IEnumerable<string> InDetails, UnrealLog.CallstackMessage InCallstack = null)
		{
			Severity = InSeverity;
			Summary = InSummary;
			Details = InDetails.ToArray();

			if (InCallstack != null)
			{
				IsEnsure = InCallstack.IsEnsure;
				Callstack = InCallstack.Callstack;
			}
			else
			{
				IsEnsure = false;
				Callstack = Enumerable.Empty<string>();
			}
		}
	}
}
