// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Class to display an incrementing progress percentage. Handles progress markup and direct console output.
	/// </summary>
	public class ProgressWriter : IDisposable
	{
		/// <summary>
		/// Global setting controlling whether to output markup
		/// </summary>
		public static bool bWriteMarkup = false;

		/// <summary>
		/// Logger for output
		/// </summary>
		ILogger Logger;

		/// <summary>
		/// The name to include with the status message
		/// </summary>
		string Message;

		/// <summary>
		/// The inner scope object
		/// </summary>
		LogStatusScope? Status;

		/// <summary>
		/// The current progress message
		/// </summary>
		string? CurrentProgressString;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InMessage">The message to display before the progress percentage</param>
		/// <param name="bInUpdateStatus">Whether to write messages to the console</param>
		/// <param name="InLogger">Logger for output</param>
		public ProgressWriter(string InMessage, bool bInUpdateStatus, ILogger InLogger)
		{
			Message = InMessage;
			Logger = InLogger;
			if (bInUpdateStatus)
			{
				Status = new LogStatusScope(InMessage);
			}
			Write(0, 100);
		}

		/// <summary>
		/// Write the terminating newline
		/// </summary>
		public void Dispose()
		{
			if (Status != null)
			{
				Status.Dispose();
				Status = null;
			}
		}

		/// <summary>
		/// Writes the current progress
		/// </summary>
		/// <param name="Numerator">Numerator for the progress fraction</param>
		/// <param name="Denominator">Denominator for the progress fraction</param>
		public void Write(int Numerator, int Denominator)
		{
			float ProgressValue = Denominator > 0 ? ((float)Numerator / (float)Denominator) : 1.0f;
			string ProgressString = String.Format("{0}%", Math.Round(ProgressValue * 100.0f));
			if (ProgressString != CurrentProgressString)
			{
				CurrentProgressString = ProgressString;
				if (bWriteMarkup)
				{
					Logger.LogInformation("@progress '{Message}' {ProgressString}", Message, ProgressString);
				}
				if (Status != null)
				{
					Status.SetProgress(ProgressString);
				}
			}
		}
	}
}
