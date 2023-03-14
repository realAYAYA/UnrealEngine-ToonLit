// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool.Benchmark
{
	class BenchmarPIEEditorTask : BenchmarkEditorStartupTask
	{
		public BenchmarPIEEditorTask(ProjectTargetInfo InTargetInfo, ProjectTaskOptions InOptions, bool InSkipBuild)
			: base(InTargetInfo, InOptions, InSkipBuild)
		{
		}
		public override string TaskName
		{
			get
			{
				return "Start PIE";
			}
		}

		protected override string GetEditorTaskArgs()
		{
			return "-execcmds=\"automation IgnoreLogEvents;runtest Project.Maps.PIE;Quit\"";
		}
	}
}
