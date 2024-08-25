// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// The type of shell supported by this platform. Used to configure command line arguments.
	/// </summary>
	public enum ShellType
	{
		/// <summary>
		/// The Bourne shell
		/// </summary>
		Sh,

		/// <summary>
		/// Windows command interpreter
		/// </summary>
		Cmd,
	}

	/// <summary>
	/// Host platform abstraction
	/// </summary>
	public abstract class BuildHostPlatform
	{
		private static BuildHostPlatform? CurrentPlatform;

		/// <summary>
		/// Host platform singleton.
		/// </summary>
		public static BuildHostPlatform Current
		{
			get
			{
				if (CurrentPlatform == null)
				{
					if (RuntimePlatform.IsWindows)
					{
						CurrentPlatform = new WindowsBuildHostPlatform();
					}
					else if (RuntimePlatform.IsMac)
					{
						CurrentPlatform = new MacBuildHostPlatform();
					}
					else if (RuntimePlatform.IsLinux)
					{
						CurrentPlatform = new LinuxBuildHostPlatform();
					}
					else
					{
						throw new NotImplementedException();
					}
				}
				return CurrentPlatform;
			}
		}

		/// <summary>
		/// Gets the current host platform type.
		/// </summary>
		public abstract UnrealTargetPlatform Platform { get; }

		/// <summary>
		/// Gets the path to the shell for this platform
		/// </summary>
		public abstract FileReference Shell { get; }

		/// <summary>
		/// The type of shell returned by the Shell parameter
		/// </summary>
		public abstract ShellType ShellType { get; }

		/// <summary>
		/// The executable binary suffix for this platform
		/// </summary>
		public abstract string BinarySuffix { get; }

		/// <summary>
		/// Class that holds information about a running process
		/// </summary>
		public class ProcessInfo
		{
			/// <summary>
			/// Process ID
			/// </summary>
			public int PID;

			/// <summary>
			/// Name of the process
			/// </summary>
			public string Name;

			/// <summary>
			/// Filename of the process binary
			/// </summary>
			public string Filename;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InPID">The process ID</param>
			/// <param name="InName">The process name</param>
			/// <param name="InFilename">The process filename</param>
			public ProcessInfo(int InPID, string InName, string InFilename)
			{
				PID = InPID;
				Name = InName;
				Filename = InFilename;
			}

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="Proc">Process to take information from</param>
			public ProcessInfo(Process Proc)
			{
				PID = Proc.Id;
				Name = Proc.ProcessName;
				Filename = Proc.MainModule?.FileName != null ? Path.GetFullPath(Proc.MainModule.FileName) : String.Empty;
			}

			/// <summary>
			/// Format as a string for debugging
			/// </summary>
			/// <returns>String containing process info</returns>
			public override string ToString()
			{
				return String.Format("{0}, {1}", Name, Filename);
			}
		}

		/// <summary>
		/// Gets all currently running processes.
		/// </summary>
		/// <returns></returns>
		public virtual ProcessInfo[] GetProcesses()
		{
			Process[] AllProcesses = Process.GetProcesses();
			List<ProcessInfo> Result = new List<ProcessInfo>(AllProcesses.Length);
			foreach (Process Proc in AllProcesses)
			{
				try
				{
					if (!Proc.HasExited)
					{
						Result.Add(new ProcessInfo(Proc));
					}
				}
				catch { }
			}
			return Result.ToArray();
		}

		/// <summary>
		/// Gets a process by name.
		/// </summary>
		/// <param name="Name">Name of the process to get information for.</param>
		/// <returns></returns>
		public virtual ProcessInfo? GetProcessByName(string Name)
		{
			ProcessInfo[] AllProcess = GetProcesses();
			foreach (ProcessInfo Info in AllProcess)
			{
				if (Info.Name == Name)
				{
					return Info;
				}
			}
			return null;
		}

		/// <summary>
		/// Gets processes by name.
		/// </summary>
		/// <param name="Name">Name of the process to get information for.</param>
		/// <returns></returns>
		public virtual ProcessInfo[] GetProcessesByName(string Name)
		{
			ProcessInfo[] AllProcess = GetProcesses();
			List<ProcessInfo> Result = new List<ProcessInfo>();
			foreach (ProcessInfo Info in AllProcess)
			{
				if (Info.Name == Name)
				{
					Result.Add(Info);
				}
			}
			return Result.ToArray();
		}

		/// <summary>
		/// Gets the filenames of all modules associated with a process
		/// </summary>
		/// <param name="PID">Process ID</param>
		/// <param name="Filename">Filename of the binary associated with the process.</param>
		/// <returns>An array of all module filenames associated with the process. Can be empty of the process is no longer running.</returns>
		public virtual string[] GetProcessModules(int PID, string Filename)
		{
			List<string> Modules = new List<string>();
			try
			{
				Process Proc = Process.GetProcessById(PID);
				if (Proc != null)
				{
					foreach (ProcessModule Module in Proc.Modules.Cast<System.Diagnostics.ProcessModule>())
					{
						if (Module.FileName != null)
						{
							Modules.Add(Path.GetFullPath(Module.FileName));
						}
					}
				}
			}
			catch { }
			return Modules.ToArray();
		}

		/// <summary>
		/// Determines if the UBT process is running through WINE
		/// </summary>
		/// <returns>Sequence of project file formats</returns>
		public virtual bool IsRunningOnWine()
		{
			return false;
		}

		/// <summary>
		/// Determines the default project file formats for this platform
		/// </summary>
		/// <returns>Sequence of project file formats</returns>
		internal abstract IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats();
	}

	class WindowsBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform => UnrealTargetPlatform.Win64;

		public override FileReference Shell => new FileReference(Environment.GetEnvironmentVariable("COMSPEC")!);

		public override ShellType ShellType => ShellType.Cmd;

		public override string BinarySuffix => ".exe";

		[DllImport("kernel32.dll", CharSet = CharSet.Auto)]
		private static extern IntPtr GetModuleHandle(string lpModuleName);

		[DllImport("kernel32.dll", CharSet = CharSet.Auto)]
		private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

		public override bool IsRunningOnWine()
		{
			IntPtr NtdllHandle = GetModuleHandle("ntdll.dll");
			return NtdllHandle.ToInt64() != 0 && GetProcAddress(NtdllHandle, "wine_get_version").ToInt64() != 0;
		}

		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.VisualStudio;
#if __VPROJECT_AVAILABLE__
			yield return ProjectFileFormat.VProject;
#endif
		}
	}

	class MacBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform => UnrealTargetPlatform.Mac;

		public override FileReference Shell => new FileReference("/bin/sh");

		public override ShellType ShellType => ShellType.Sh;

		public override string BinarySuffix => String.Empty;

		/// <summary>
		/// (needs confirmation) Currently returns incomplete process names in Process.GetProcesses() so we need to parse 'ps' output.
		/// </summary>
		/// <returns></returns>
		public override ProcessInfo[] GetProcesses()
		{
			List<ProcessInfo> Result = new List<ProcessInfo>();

			string TempFile = Path.Combine("/var/tmp", Path.GetTempFileName());
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = "/bin/sh";
			StartInfo.Arguments = "-c \"ps -eaw -o pid,comm > " + TempFile + "\"";
			StartInfo.CreateNoWindow = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			try
			{
				Proc.Start();
				foreach (string FileLine in File.ReadAllLines(TempFile))
				{
					string Line = FileLine.Trim();
					int PIDEnd = Line.IndexOf(' ');
					string PIDString = Line.Substring(0, PIDEnd);
					if (PIDString != "PID")
					{
						string Filename = Line.Substring(PIDEnd + 1);
						int Pid = Int32.Parse(PIDString);
						try
						{
							Process ExistingProc = Process.GetProcessById(Pid);
							if (ExistingProc != null && Pid != Process.GetCurrentProcess().Id && ExistingProc.HasExited == false)
							{
								ProcessInfo ProcInfo = new ProcessInfo(ExistingProc.Id, Path.GetFileName(Filename), Filename);
								Result.Add(ProcInfo);
							}
						}
						catch { }
					}
				}
				File.Delete(TempFile);
				Proc.WaitForExit();
			}
			catch { }
			return Result.ToArray();
		}

		/// <summary>
		/// (needs confirmation) Currently returns incomplete list of modules for Process.Modules so we need to parse vmmap output.
		/// </summary>
		/// <param name="PID"></param>
		/// <param name="Filename"></param>
		/// <returns></returns>
		public override string[] GetProcessModules(int PID, string Filename)
		{
			HashSet<string> Modules = new HashSet<string>();
			// Add the process file name to the module list. This is to make it compatible with the results of Process.Modules on Windows.
			Modules.Add(Filename);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = "vmmap";
			StartInfo.Arguments = String.Format("{0} -w", PID);
			StartInfo.CreateNoWindow = true;
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardOutput = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			try
			{
				Proc.Start();
				// Start processing output before vmmap exits otherwise it's going to hang
				while (!Proc.WaitForExit(1))
				{
					ProcessVMMapOutput(Proc, Modules);
				}
				ProcessVMMapOutput(Proc, Modules);
			}
			catch { }
			return Modules.ToArray();
		}
		private void ProcessVMMapOutput(Process Proc, HashSet<string> Modules)
		{
			for (string? Line = Proc.StandardOutput.ReadLine(); Line != null; Line = Proc.StandardOutput.ReadLine())
			{
				Line = Line.Trim();
				if (Line.EndsWith(".dylib"))
				{
					const int SharingModeLength = 6;
					int SMStart = Line.IndexOf("SM=");
					int PathStart = SMStart + SharingModeLength;
					string Module = Line.Substring(PathStart).Trim();
					if (!Modules.Contains(Module))
					{
						Modules.Add(Module);
					}
				}
			}
		}

		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.XCode;
			yield return ProjectFileFormat.VisualStudioMac;
#if __VPROJECT_AVAILABLE__
			yield return ProjectFileFormat.VProject;
#endif
		}
	}

	class LinuxBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform => UnrealTargetPlatform.Linux;

		public override FileReference Shell => new FileReference("/bin/sh");

		public override ShellType ShellType => ShellType.Sh;

		public override string BinarySuffix => String.Empty;

		/// <summary>
		/// (needs confirmation) Currently returns incomplete process names in Process.GetProcesses() so we need to use /proc
		/// (also, locks up during process traversal sometimes, trying to open /dev/snd/pcm*)
		/// </summary>
		/// <returns></returns>
		public override ProcessInfo[] GetProcesses()
		{
			// @TODO: Implement for Linux
			return new List<ProcessInfo>().ToArray();
		}

		/// <summary>
		/// (needs confirmation) Currently returns incomplete list of modules for Process.Modules so we need to parse /proc/PID/maps.
		/// (also, locks up during process traversal sometimes, trying to open /dev/snd/pcm*)
		/// </summary>
		/// <param name="PID"></param>
		/// <param name="Filename"></param>
		/// <returns></returns>
		public override string[] GetProcessModules(int PID, string Filename)
		{
			// @TODO: Implement for Linux
			return new List<string>().ToArray();
		}

		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.Make;
			yield return ProjectFileFormat.VisualStudioCode;
#if __VPROJECT_AVAILABLE__
			yield return ProjectFileFormat.VProject;
#endif
		}
	}
}
