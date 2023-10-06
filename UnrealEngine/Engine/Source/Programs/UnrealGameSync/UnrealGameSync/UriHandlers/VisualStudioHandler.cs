// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging.Abstractions;

namespace UnrealGameSync
{
	/// <summary>
	/// VisualStudio Uri Handler
	/// </summary>
	static class VisualStudioHandler
	{
		[UriHandler(true)]
		public static UriResult VsOpen(string depotPath, int line = -1)
		{
			string tempFileName = P4Automation.PrintToTempFile(null, depotPath, NullLogger.Instance).GetAwaiter().GetResult();

			string? errorMessage;
			if (!VisualStudioAutomation.OpenFile(tempFileName, out errorMessage, line))
			{
				return new UriResult() { Error = errorMessage ?? "Unknown Visual Studio Error" };
			}

			return new UriResult() { Success = true };
		}
	}
}