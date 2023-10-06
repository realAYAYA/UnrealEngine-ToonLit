// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Runtime.InteropServices;

namespace P4VUtils
{
	public static class ProcessUtils
	{
		/// <summary>
		/// Opens the given path in a new process. Which process is launched
		/// will be decided by the OS based on association with the Path itself.
		/// A .txt file will probably open a text editor, a url will open the
		/// default browser etc.
		/// </summary>
		/// <param name="Path"The path to be opened></param>
		public static void OpenInNewProcess(string Path)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				Process.Start(new ProcessStartInfo(Path) { UseShellExecute = true });
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				Process.Start("xdg-open", Path);
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				Process.Start("open", Path);
			}
		}
	}
}
