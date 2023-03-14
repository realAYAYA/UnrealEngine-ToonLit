// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.IO;
using System.Threading;
using System.Xml;
using System.Diagnostics;

using Action = System.Action;
using EpicGames.Core;
using UnrealBuildBase;
using EpicGames.BuildGraph;
using AutomationTool.Tasks;
using System.Runtime.Versioning;

namespace Win.Automation
{
	/// <summary>
	/// Parameters for a task that uploads symbols to a symbol server
	/// </summary>
	public class SrcSrvTaskParameters
	{
		/// <summary>
		/// List of output files. PDBs will be extracted from this list.
		/// </summary>
		[TaskParameter]
		public string BinaryFiles;

		/// <summary>
		/// List of source files to index and embed into the PDBs.
		/// </summary>
		[TaskParameter]
		public string SourceFiles;

		/// <summary>
		/// Branch to base all the depot source files from
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// Changelist to sync files from
		/// </summary>
		[TaskParameter]
		public int Change;
	}

	/// <summary>
	/// Task which strips symbols from a set of files
	/// </summary>
	[TaskElement("SrcSrv", typeof(SrcSrvTaskParameters))]
	public class SrcSrvTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SrcSrvTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public SrcSrvTask(SrcSrvTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		[SupportedOSPlatform("windows")]
		public override void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			FileReference[] BinaryFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.BinaryFiles, TagNameToFileSet).ToArray();
			FileReference[] SourceFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.SourceFiles, TagNameToFileSet).ToArray();
			Execute(BinaryFiles, SourceFiles, Parameters.Branch, Parameters.Change);
		}

		[SupportedOSPlatform("windows")]
		internal static void Execute(FileReference[] BinaryFiles, FileReference[] SourceFiles, string Branch, int Change)
		{
			FileReference[] PdbFiles = BinaryFiles.Where(x => x.HasExtension(".pdb")).ToArray();

			// Get the PDBSTR.EXE path, using the latest SDK version we can find.
			FileReference PdbStrExe = GetPdbStrExe();

			// Get the path to the generated SRCSRV.INI file
			FileReference SrcSrvIni = FileReference.Combine(Unreal.RootDirectory, "Engine", "Intermediate", "SrcSrv.ini");
			DirectoryReference.CreateDirectory(SrcSrvIni.Directory);

			// Generate the SRCSRV.INI file
			using (StreamWriter Writer = new StreamWriter(SrcSrvIni.FullName))
			{
				Writer.WriteLine("SRCSRV: ini------------------------------------------------");
				Writer.WriteLine("VERSION=1");
				Writer.WriteLine("VERCTRL=Perforce");
				Writer.WriteLine("SRCSRV: variables------------------------------------------");
				Writer.WriteLine("SRCSRVTRG=%sdtrg%");
				Writer.WriteLine("SRCSRVCMD=%sdcmd%");
				Writer.WriteLine("SDCMD=p4.exe print -o %srcsrvtrg% \"{0}/%var2%@{1}\"", Branch.TrimEnd('/'), Change);
				Writer.WriteLine("SDTRG=%targ%\\{0}\\{1}\\%fnbksl%(%var2%)", Branch.Replace('/', '+'), Change);
				Writer.WriteLine("SRCSRV: source files ---------------------------------------");
				foreach (FileReference SourceFile in SourceFiles)
				{
					string RelativeSourceFile = SourceFile.MakeRelativeTo(Unreal.RootDirectory);
					Writer.WriteLine("{0}*{1}", SourceFile.FullName, RelativeSourceFile.Replace('\\', '/'));
				}
				Writer.WriteLine("SRCSRV: end------------------------------------------------");
			}

            // Execute PDBSTR on the PDB files in parallel.
            Parallel.For(0, PdbFiles.Length, (Idx, State) => { ExecuteTool(PdbStrExe, PdbFiles[Idx], SrcSrvIni, State); });
		}

        /// <summary>
        /// Executes the PdbStr tool.
        /// </summary>
        /// <param name="PdbStrExe">Path to PdbStr.exe</param>
        /// <param name="PdbFile">The PDB file to embed source information for</param>
        /// <param name="SrcSrvIni">Ini file containing settings to embed</param>
        /// <param name="State">The current loop state</param>
        /// <returns>True if the tool executed succesfully</returns>
        static void ExecuteTool(FileReference PdbStrExe, FileReference PdbFile, FileReference SrcSrvIni, ParallelLoopState State)
        {
            using (Process Process = new Process())
            {
                List<string> Messages = new List<string>();
                Messages.Add(String.Format("Writing source server data: {0}", PdbFile));

                DataReceivedEventHandler OutputHandler = (s, e) => { if (e.Data != null) { Messages.Add(e.Data); } };
                Process.StartInfo.FileName = PdbStrExe.FullName;
                Process.StartInfo.Arguments = String.Format("-w -p:\"{0}\" -i:\"{1}\" -s:srcsrv", PdbFile.FullName, SrcSrvIni.FullName);
                Process.StartInfo.UseShellExecute = false;
                Process.StartInfo.RedirectStandardOutput = true;
                Process.StartInfo.RedirectStandardError = true;
                Process.StartInfo.RedirectStandardInput = false;
                Process.StartInfo.CreateNoWindow = true;
                Process.OutputDataReceived += OutputHandler;
                Process.ErrorDataReceived += OutputHandler;
                Process.Start();
                Process.BeginOutputReadLine();
                Process.BeginErrorReadLine();
                Process.WaitForExit();

                if(Process.ExitCode != 0)
                {
                    Messages.Add(String.Format("Exit code: {0}", Process.ExitCode));
                    State.Break();
                }

                lock (State)
                {
                    foreach (string Message in Messages)
                    {
                        CommandUtils.LogInformation(Message);
                    }
                }

				if(Process.ExitCode != 0)
				{
					throw new AutomationException("Failed to embed source server data for {0}", PdbFile);
				}
            }
        }

        /// <summary>
        /// Output this task out to an XML writer.
        /// </summary>
        public override void Write(XmlWriter Writer)
        {
            Write(Writer, Parameters);
        }

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.BinaryFiles))
			{
				yield return TagName;
			}
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.SourceFiles))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Try to get the PDBSTR.EXE path from the Windows SDK
		/// </summary>
		/// <returns>Path to PDBSTR.EXE</returns>
		[SupportedOSPlatform("windows")]
		public static FileReference GetPdbStrExe()
		{
			List<KeyValuePair<string, DirectoryReference>> WindowsSdkDirs = WindowsExports.GetWindowsSdkDirs();
			foreach (DirectoryReference WindowsSdkDir in WindowsSdkDirs.Select(x => x.Value))
			{
				FileReference CheckPdbStrExe64 = FileReference.Combine(WindowsSdkDir, "Debuggers", "x64", "SrcSrv", "PdbStr.exe");
				if (FileReference.Exists(CheckPdbStrExe64))
				{
					return CheckPdbStrExe64;
				}

				FileReference CheckPdbStrExe32 = FileReference.Combine(WindowsSdkDir, "Debuggers", "x86", "SrcSrv", "PdbStr.exe");
				if (FileReference.Exists(CheckPdbStrExe32))
				{
					return CheckPdbStrExe32;
				}
			}
			throw new AutomationException("Unable to find a Windows SDK installation containing PDBSTR.EXE");
		}
	}

	public static partial class BgStateExtensions
	{
		/// <summary>
		/// Uploads symbols to a symbol server
		/// </summary>
		/// <param name="BinaryFiles">List of output files. PDBs will be extracted from this list.</param>
		/// <param name="SourceFiles">List of source files to index and embed into the PDBs.</param>
		/// <param name="Branch">Branch to base all the depot source files from.</param>
		/// <param name="Change">Changelist to sync files from.</param>
		/// <returns></returns>
		[SupportedOSPlatform("windows")]
		public static void SrcSrv(this BgContext State, HashSet<FileReference> BinaryFiles, HashSet<FileReference> SourceFiles, string Branch, int Change)
		{
			SrcSrvTask.Execute(BinaryFiles.ToArray(), SourceFiles.ToArray(), Branch, Change);
		}
	}
}
