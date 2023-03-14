// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class llvm : ModuleRules
{
	public llvm(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform != UnrealTargetPlatform.Win64)
		{
			// Currently we support only Win64 llvm builds.
			return;
		}

		var LLVMVersion = "8";
		var TargetArch = "Win64";
		var VSVersion = "VS2017";
		var RootDirectory = Path.Combine(ModuleDirectory, LLVMVersion);
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(RootDirectory, "include"),
			});

		string LibDir = Path.Combine(RootDirectory, "lib", TargetArch, VSVersion, "Release");

		string[] Libs = new string[] {
				"LLVMAggressiveInstCombine.lib",
				"LLVMAnalysis.lib",
				"LLVMAsmPrinter.lib",
				"LLVMBinaryFormat.lib",
				"LLVMBitReader.lib",
				"LLVMBitWriter.lib",
				"LLVMCodeGen.lib",
				"LLVMCore.lib",
				"LLVMDebugInfoCodeView.lib",
				"LLVMDebugInfoMSF.lib",
				"LLVMDemangle.lib",
				"LLVMExecutionEngine.lib",
				"LLVMGlobalISel.lib",
				"LLVMInstCombine.lib",
				"LLVMInterpreter.lib",
				"LLVMMC.lib",
				"LLVMMCDisassembler.lib",
				"LLVMMCJIT.lib",
				"LLVMMCParser.lib",
				"LLVMObject.lib",
				"LLVMProfileData.lib",
				"LLVMRuntimeDyld.lib",
				"LLVMScalarOpts.lib",
				"LLVMSelectionDAG.lib",
				"LLVMSupport.lib",
				"LLVMTarget.lib",
				"LLVMTransformUtils.lib",
				"LLVMX86AsmPrinter.lib",
				"LLVMX86CodeGen.lib",
				"LLVMX86Desc.lib",
				"LLVMX86Info.lib",
				"LLVMX86Utils.lib",
			};

			foreach(string Lib in Libs)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, Lib));
			}
	}
}
