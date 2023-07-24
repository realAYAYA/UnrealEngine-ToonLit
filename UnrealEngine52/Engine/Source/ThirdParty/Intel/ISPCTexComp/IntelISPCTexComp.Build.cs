// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelISPCTexComp : ModuleRules
{
	public IntelISPCTexComp(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string SourcePath = Target.UEThirdPartySourceDirectory + "Intel/ISPCTexComp/ISPCTextureCompressor-14d998c/";
        string IncludesPath = SourcePath + "ispc_texcomp/";
        string BinaryFolder = Target.UEThirdPartyBinariesDirectory + "Intel/ISPCTexComp/";
		PublicSystemIncludePaths.Add(IncludesPath);

        //NOTE: If you change bUseDebugBuild, you must also change FTextureFormatIntelISPCTexCompModule.GetTextureFormat() to load the corresponding DLL
        bool bUseDebugBuild = false;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            string configName = bUseDebugBuild ? "Debug" : "Release";
    		string LibraryPath = SourcePath + "ISPC Texture Compressor/x64/" + configName + "/";
            string DLLFolder = BinaryFolder + "Win64-" + configName;
            string DLLFilePath = DLLFolder + "/ispc_texcomp.dll";
            PublicAdditionalLibraries.Add(LibraryPath + "ispc_texcomp.lib");
			PublicDelayLoadDLLs.Add("ispc_texcomp.dll");
            RuntimeDependencies.Add(DLLFilePath);
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string BinaryLibraryFolder = BinaryFolder + "Mac64-Release";
            string LibraryFilePath = BinaryLibraryFolder + "/libispc_texcomp.dylib";
            PublicAdditionalLibraries.Add(LibraryFilePath);
            PublicDelayLoadDLLs.Add(LibraryFilePath);
            RuntimeDependencies.Add(LibraryFilePath);
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64)
        {
            string BinaryLibraryFolder = BinaryFolder + "Linux64-Release";
			PrivateRuntimeLibraryPaths.Add(BinaryLibraryFolder);
            string LibraryFilePath = BinaryLibraryFolder + "/libispc_texcomp.so";
            PublicAdditionalLibraries.Add(LibraryFilePath);
            PublicDelayLoadDLLs.Add("libispc_texcomp.so");
            RuntimeDependencies.Add(LibraryFilePath);
        }
    }
}
