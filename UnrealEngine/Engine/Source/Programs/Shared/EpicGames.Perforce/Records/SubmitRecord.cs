// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Perforce
{
	/// <summary>
	/// Contains information about a submitted changelist
	/// </summary>
	public class SubmitRecord
	{
		/// <summary>
		/// Submitted changelist number
		/// </summary>
		[PerforceTag("change", Optional = true)]
		public int ChangeNumber { get; set; } = -1;

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return ChangeNumber.ToString();
		}
	}
}
