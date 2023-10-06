// Copyright Epic Games, Inc.All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGame;
using Gauntlet;

namespace LyraTest
{
	/// <summary>
	/// Basic Boot Test
	/// </summary>
	public class BootTest : EpicGameTestNode<LyraTestConfig>
	{
		public BootTest(UnrealTestContext InContext) : base (InContext)
		{
		}

		public override LyraTestConfig GetConfiguration()
		{
			LyraTestConfig Config = base.GetConfiguration();
			Config.NoMCP = true;

			UnrealTestRole Client = Config.RequireRole(UnrealTargetRole.Client);
			Client.Controllers.Add("BootTest");

			return Config;
		}
	}
}
