// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Eigen : ModuleRules
{
    public Eigen(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
		
		PublicIncludePaths.Add(ModuleDirectory);
        PublicIncludePaths.Add( ModuleDirectory + "/Eigen/" );
		PublicDefinitions.Add("EIGEN_MPL2_ONLY");
		PublicDefinitions.Add("EIGEN_USE_THREADS");
		PublicDefinitions.Add("EIGEN_HAS_C99_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CONSTEXPR");
		PublicDefinitions.Add("EIGEN_HAS_VARIADIC_TEMPLATES");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_MATH");
		PublicDefinitions.Add("EIGEN_HAS_CXX11_ATOMIC");
		PublicDefinitions.Add("EIGEN_STRONG_INLINE=inline");
		ShadowVariableWarningLevel = WarningLevel.Off;
		bDisableStaticAnalysis = true;
	}
}
