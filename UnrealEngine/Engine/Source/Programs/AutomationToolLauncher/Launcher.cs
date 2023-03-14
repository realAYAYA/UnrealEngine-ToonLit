// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Reflection;
using System.IO;
using System.Linq;
using System.Diagnostics;
using System.Dynamic;

namespace AutomationToolLauncher
{
	class Launcher
	{
		static int Main(string[] Arguments)
		{
			return Run(Arguments);

		}
		static int Run(string[] Arguments)
		{

			string ApplicationBase = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			string UATExecutable = Path.Combine(ApplicationBase, "..\\AutomationTool", "AutomationTool.exe");

			if (!File.Exists(UATExecutable))
			{
				Console.WriteLine(string.Format("AutomationTool does not exist at: {0}", UATExecutable));
				return -1;
			}

			try
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo(UATExecutable);
				foreach (string s in Arguments)
				{
					StartInfo.ArgumentList.Add(s);
				}
				Process uatProcess = Process.Start(StartInfo);
				uatProcess.WaitForExit();
				Environment.Exit(uatProcess.ExitCode);
			}
			catch (Exception Ex)
			{
				Console.WriteLine(Ex.Message);
				Console.WriteLine(Ex.StackTrace);
			}

			return -1;

		}
	}
}
