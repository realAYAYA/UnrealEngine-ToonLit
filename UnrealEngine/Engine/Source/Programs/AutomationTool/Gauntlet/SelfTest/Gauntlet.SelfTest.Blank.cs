// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	class Blank : BaseTestNode
	{
		public override void TickTest()
		{
		
			MarkComplete(TestResult.Passed);
		}
	}
}
