// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;

namespace AutomationTool
{

	[Help("Opens the specified project.")]
	[Help("project=<QAGame>", "Project to open. Will search current path and paths in ueprojectdirs. If omitted will open vanilla UnrealEditor")]
	public class OpenEditor: BuildCommand
	{
		// exposed as a property so projects can derive and set this directly
		public string ProjectName { get; set; }

		private string _UnrealEditorApp = "UnrealEditor";
		public string UnrealEditorApp
		{
			get
			{
				return _UnrealEditorApp;
			}
			set
			{
				_UnrealEditorApp = value;
			}
		}

		public bool bNoProject { get; set; }

		public OpenEditor()
		{
			bNoProject = false;
		}
		
		public override ExitCode Execute()
		{
			string EditorPath = HostPlatform.Current.GetUnrealExePath(UnrealEditorApp);

			string EditorArgs = "";

			ProjectName = ParseParamValue("project", ProjectName);
			IEnumerable<string> ParamList = null;

			if (bNoProject)
			{
				ParamList = Params;
			}
			else
			{
				if (!String.IsNullOrEmpty(ProjectName))
				{
					FileReference ProjectFile = ProjectUtils.FindProjectFileFromName(ProjectName);

					if (ProjectFile == null)
					{
						throw new AutomationException("Unable to find uproject file for {0}", ProjectName);
					}

					EditorArgs = ProjectFile.FullName;
				}

				// filter out any -project argument since we want it to be the first un-prefixed argument to the editor 
				ParamList = this.Params
								.Where(P => P.StartsWith("project=", StringComparison.OrdinalIgnoreCase) == false);
			}

			ParamList = new[] { EditorArgs }.Concat(ParamList);

			bool bLaunched = RunUntrackedProcess(EditorPath, string.Join(" -", ParamList));

			return bLaunched ? ExitCode.Success : ExitCode.Error_UATLaunchFailure;
		}

		protected bool RunUntrackedProcess(string BinaryPath, string Args)
		{
			LogInformation("Running {0} {1}", BinaryPath, Args);

			var NewProcess = HostPlatform.Current.CreateProcess(BinaryPath);			
			var Result = new ProcessResult(BinaryPath, NewProcess, false, false);
			System.Diagnostics.Process Proc = Result.ProcessObject;

			Proc.StartInfo.FileName = BinaryPath;
			Proc.StartInfo.Arguments = string.IsNullOrEmpty(Args) ? "" : Args;
			Proc.StartInfo.UseShellExecute = false;
			return Proc.Start();
		}
	}

}