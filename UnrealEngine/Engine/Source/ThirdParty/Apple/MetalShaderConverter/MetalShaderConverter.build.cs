// Copyright (C) 2022 Apple Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetalShaderConverter : ModuleRules
{
    public MetalShaderConverter(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string MetalShaderConverterPath = Target.UEThirdPartySourceDirectory + "Apple/MetalShaderConverter";

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicIncludePaths.Add(MetalShaderConverterPath + "/include");

			string DylibPath = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Apple", "MetalShaderConverter", "Mac", "libmetalirconverter.dylib");
						
            PublicAdditionalLibraries.Add(DylibPath);
            RuntimeDependencies.Add(DylibPath);
        }
    }
}
