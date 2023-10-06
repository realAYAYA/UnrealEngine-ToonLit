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
	public class LyraTestConfig : EpicGameTestConfig
	{
		[AutoParam]
		public int TargetNumOfCycledMatches = 2;

		[AutoParam]
		public bool WithPacketHandlerEncryption = false;

		public override void ApplyToConfig(UnrealAppConfig AppConfig, UnrealSessionRole ConfigRole, IEnumerable<UnrealSessionRole> OtherRoles)
		{
			base.ApplyToConfig(AppConfig, ConfigRole, OtherRoles);

			if (AppConfig.ProcessType.IsClient())
			{
				AppConfig.CommandLine += string.Format(" -TargetNumOfCycledMatches={0}", TargetNumOfCycledMatches);
			}

			if (WithPacketHandlerEncryption)
			{
				AppConfig.CommandLine += " -ini:Engine:[PacketHandlerComponents]:EncryptionComponent=AESGCMHandlerComponent -execcmds=\"Lyra.TestEncryption true\"";
			}

			const float InitTime = 120.0f;
			const float MatchTime = 300.0f;
			MaxDuration = InitTime + (MatchTime * TargetNumOfCycledMatches);
		}
	}
}
