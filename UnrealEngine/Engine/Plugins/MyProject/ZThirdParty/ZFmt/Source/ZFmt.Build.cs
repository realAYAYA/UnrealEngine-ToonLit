using UnrealBuildTool;
using System.IO;

public class ZFmt : ModuleRules
{
	public ZFmt(ReadOnlyTargetRules Target) : base(Target)
	{
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			});


	}
}
