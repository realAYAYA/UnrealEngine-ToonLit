// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	abstract class ActionExecutor : IDisposable
	{
		public abstract string Name
		{
			get;
		}

		protected static double MemoryPerActionBytesOverride
		{
			get;
			private set;
		} = 0.0;

		readonly LogEventParser Parser;

		public ActionExecutor(ILogger Logger)
		{
			Parser = new LogEventParser(Logger);
			Parser.AddMatchersFromAssembly(Assembly.GetExecutingAssembly());
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				Parser.Dispose();
			}
		}

		/// <summary>
		/// Allow targets to override the expected amount of memory required for compiles, used to control the number
		/// of parallel action processes.
		/// </summary>
		/// <param name="MemoryPerActionOverrideGB"></param>
		public static void SetMemoryPerActionOverride(double MemoryPerActionOverrideGB)
		{
			MemoryPerActionBytesOverride = Math.Max(MemoryPerActionBytesOverride, MemoryPerActionOverrideGB * 1024 * 1024 * 1024);
		}

		/// <summary>
		/// Execute the given actions
		/// </summary>
		/// <param name="ActionsToExecute">Actions to be executed</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="actionArtifactCache">Cache used to read/write action outputs.</param>
		/// <returns>True if the build succeeded, false otherwise</returns>
		public abstract Task<bool> ExecuteActionsAsync(IEnumerable<LinkedAction> ActionsToExecute, ILogger Logger, IActionArtifactCache? actionArtifactCache = null);

		/// <summary>
		/// Will verify that produced items exists on disk
		/// </summary>
		public virtual bool VerifyOutputs => true;

		protected void WriteToolOutput(string Line)
		{
			lock (Parser)
			{
				Parser.WriteLine(Line);
			}
		}

		protected void FlushToolOutput()
		{
			lock (Parser)
			{
				Parser.Flush();
			}
		}
	}
}
