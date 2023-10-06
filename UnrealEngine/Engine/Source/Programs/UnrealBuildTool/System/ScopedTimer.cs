// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Scoped timer, start is in the constructor, end in Dispose. Best used with using(ScopedTimer Timer = new ScopedTimer()). Suports nesting.
	/// </summary>
	public class ScopedTimer : IDisposable
	{
		DateTime StartTime;
		string Name;
		ILogger Logger;
		LogLevel Level;
		bool bIncreaseIndent;
		static int Indent = 0;
		static object IndentLock = new object();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the block being measured</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="InLevel">Verbosity for output messages</param>
		/// <param name="bIncreaseIndent">Whether gobal indent should be increased or not; set to false when running a scope in parallel. Message will still be printed indented relative to parent scope.</param>
		public ScopedTimer(string Name, ILogger Logger, LogLevel InLevel = LogLevel.Debug, bool bIncreaseIndent = true)
		{
			this.Name = Name;
			this.Logger = Logger;
			if (bIncreaseIndent)
			{
				lock (IndentLock)
				{
					Indent++;
				}
			}
			Level = InLevel;
			StartTime = DateTime.UtcNow;
			this.bIncreaseIndent = bIncreaseIndent;
		}

		/// <summary>
		/// Prints out the timing message
		/// </summary>
		public void Dispose()
		{
			double TotalSeconds = (DateTime.UtcNow - StartTime).TotalSeconds;
			int LogIndent = Indent;
			if (bIncreaseIndent)
			{
				lock (IndentLock)
				{
					LogIndent = --Indent;
				}
			}
			StringBuilder IndentText = new StringBuilder(LogIndent * 2);
			IndentText.Append(' ', LogIndent * 2);

			Logger.Log(Level, "{Indent}{Name} took {TimeSeconds}s", IndentText.ToString(), Name, TotalSeconds);
		}
	}
}
