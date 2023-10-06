// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Eigen : ModuleRules
{
    public Eigen(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
		
		PublicSystemIncludePaths.Add(ModuleDirectory);
        PublicSystemIncludePaths.Add(Path.Join(ModuleDirectory, "Eigen"));
		PublicDefinitions.Add("EIGEN_MPL2_ONLY");
		PublicDefinitions.Add("EIGEN_USE_THREADS");
		PublicDefinitions.Add("EIGEN_HAS_C99_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CONSTEXPR");
		PublicDefinitions.Add("EIGEN_HAS_VARIADIC_TEMPLATES");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_ATOMIC");
		PublicDefinitions.Add("EIGEN_STRONG_INLINE=inline");
		PublicDefinitions.Add("EIGEN_UE_OVERRIDE_ALLOCATORS=1");

		ShadowVariableWarningLevel = WarningLevel.Off;
		bDisableStaticAnalysis = true;
	}
}
