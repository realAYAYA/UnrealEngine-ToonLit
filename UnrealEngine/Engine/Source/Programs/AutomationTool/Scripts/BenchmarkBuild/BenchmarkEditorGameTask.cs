// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool.Benchmark
{
	class BenchmarkEditorGameTask : BenchmarkEditorStartupTask
	{
		public BenchmarkEditorGameTask(ProjectTargetInfo InTargetInfo, ProjectTaskOptions InOptions, bool InSkipBuild)
			: base(InTargetInfo, InOptions, InSkipBuild)
		{
		}

		public override string TaskName
		{
			get
			{
				return "EditorGame";
			}
		}

		protected override string GetEditorTaskArgs()
		{
			return "-windowed -game -execcmds=\"automation Quit\"";
		}
	}
}
