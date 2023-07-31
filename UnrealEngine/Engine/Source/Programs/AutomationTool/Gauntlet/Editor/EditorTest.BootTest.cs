// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EditorTest
{
	class BootTest : UnrealGame.DefaultTest
	{
		public BootTest(Gauntlet.UnrealTestContext InContext) 
			: base(InContext)
		{
		}

		public override UnrealGame.UnrealTestConfig GetConfiguration()
		{
			UnrealGame.UnrealTestConfig Config = base.GetConfiguration();

			Config.RequireRole(UnrealTargetRole.Editor);

			return Config;
		}
	}
}
