// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;

namespace AutomationTool.Benchmark
{
	class BenchmarkEditorStartupTask : BenchmarkEditorTaskBase
	{
		public BenchmarkEditorStartupTask(ProjectTargetInfo InTargetInfo, ProjectTaskOptions InOptions, bool InSkipBuild)
			: base(InTargetInfo, InOptions, InSkipBuild)
		{
		}
		public override string TaskName
		{
			get
			{
				return "Start Editor";
			}
		}

		protected override string GetEditorTaskArgs()
		{
			return "-execcmds=\"automation Quit\"";
		}
	}
}
