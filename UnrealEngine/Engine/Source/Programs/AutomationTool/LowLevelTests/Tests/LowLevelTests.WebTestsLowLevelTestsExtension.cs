// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;
using Gauntlet;
using UnrealBuildBase;
using System.Net.Sockets;

namespace LowLevelTests
{
	public class WebTestsLowLevelTestsExtension : ILowLevelTestsExtension
	{
		private Process ServerProcess;

		public bool IsSupported(UnrealTargetPlatform InPlatform, string InTestApp)
		{
			return InTestApp == "WebTests";
		}

		public string ExtraCommandLine(UnrealTargetPlatform InPlatform, string InTestApp, string InBuildPath)
		{
			return string.Format("--web_server_ip={0}", UnrealHelpers.GetHostIpAddress());
		}

		public void PreRunTests()
		{
			InstallWebServer();
			AsyncLaunchWebServerProcess();
			// If start WebTests right after launching web server process without waiting, it could get refused to connect, especially on Linux
			WaitUntilWebServerPortOpen();
		}

		private string WebTestsServerDir()
		{
			return Path.Combine(Unreal.EngineDirectory.FullName, "Source", "Programs", "WebTestsServer");
		}

		private void InstallWebServer()
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WebTestsServerDir();
			StartInfo.FileName = RuntimePlatform.IsWindows ? "cmd.exe" : "/bin/sh";
			StartInfo.Arguments = RuntimePlatform.IsWindows ? "/c createenv.bat" : "-c './createenv.sh'";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;

			Process InstallProcess = new Process();
			InstallProcess.StartInfo = StartInfo;
			InstallProcess.Start();
			InstallProcess.WaitForExit();

			Console.WriteLine("Requirements installed.");
		}

		private void AsyncLaunchWebServerProcess()
		{
			string WorkingDir = WebTestsServerDir();
			string PythonFile = Path.Combine(WorkingDir, "env", RuntimePlatform.IsWindows ? "Scripts" : "bin", RuntimePlatform.IsWindows ? "python.exe" : "python");

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDir;
			StartInfo.FileName = PythonFile;
			StartInfo.WindowStyle = ProcessWindowStyle.Normal;
			StartInfo.Arguments = "manage.py runserver 0.0.0.0:8000";
			StartInfo.UseShellExecute = true;
			StartInfo.CreateNoWindow = false;

			ServerProcess = new Process();
			ServerProcess.StartInfo = StartInfo;
			ServerProcess.Start();

			Console.WriteLine("Web server process is now running.");
		}

		private void WaitUntilWebServerPortOpen()
		{
			Stopwatch sw = new Stopwatch();
			sw.Start();

			while (!IsServerPortOpen(UnrealHelpers.GetHostIpAddress(), 8000))
			{
				if (sw.ElapsedMilliseconds > 60000)
				{
					sw.Stop();
					throw new TimeoutException("Server port did not open within the specified time.");
				}
				System.Threading.Thread.Sleep(1000);
			}

			sw.Stop();

			Console.WriteLine("Web server port is now open.");
		}

		private bool IsServerPortOpen(string ipAddress, int port)
		{
			using (TcpClient client = new TcpClient())
			{
				try
				{
					client.Connect(ipAddress, port);
					return true;
				}
				catch
				{
					return false;
				}
			}
		}

		public void PostRunTests()
		{
			CloseWebServer();
		}

		private void CloseWebServer()
		{
			if (ServerProcess != null)
			{
				ServerProcess.CloseMainWindow();
				ServerProcess = null;

				Console.WriteLine("Web server killed.");
			}
		}
	}
}
