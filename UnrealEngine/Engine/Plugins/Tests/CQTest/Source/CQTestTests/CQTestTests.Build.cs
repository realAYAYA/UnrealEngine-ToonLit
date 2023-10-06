// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CQTestTests : ModuleRules
{
	public CQTestTests(ReadOnlyTargetRules Target)
		: base(Target)
	{
		// Does not compile with C++20:
		// error C2088: '<<': illegal for class
		// error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator
		// <<< std::char_traits<char> > (std::basic_ostream<char, std::char_traits<char>> &,const wchar_t*)': attempting to
		// reference a deleted function
		CppStandard = CppStandardVersion.Cpp17;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"CQTest"
				 }
			);
	}
}
