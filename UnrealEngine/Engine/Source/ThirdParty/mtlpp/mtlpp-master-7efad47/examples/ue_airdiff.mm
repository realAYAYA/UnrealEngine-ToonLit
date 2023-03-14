// Copyright Epic Games, Inc. All Rights Reserved.

#include "../src/mtlpp.hpp"

bool DumpAIR(ns::String const& sourcePath, ns::String const& tmpOutput, ns::String const& LibOutput, mtlpp::CompilerOptions const& options)
{
	ns::AutoReleasedError AutoReleasedError;
	bool bOK = mtlpp::Compiler::Compile(sourcePath, tmpOutput, options, &AutoReleasedError);
	
	if (AutoReleasedError.GetPtr())
	{
		NSLog(@"%@: %@ %d %@ %@", bOK ? @"Warning" : @"AutoReleasedError", AutoReleasedError.GetDomain().GetPtr(), AutoReleasedError.GetCode(), AutoReleasedError.GetLocalizedDescription().GetPtr(), AutoReleasedError.GetLocalizedFailureReason().GetPtr());
	}
	
	if (bOK)
	{
		ns::Array<ns::String> Array = @[ tmpOutput.GetPtr() ];
		bOK = mtlpp::Compiler::Link(Array, LibOutput, options, &AutoReleasedError);
		
		if (AutoReleasedError.GetPtr())
		{
			NSLog(@"%@: %@ %d %@ %@", bOK ? @"Warning" : @"AutoReleasedError", AutoReleasedError.GetDomain().GetPtr(), AutoReleasedError.GetCode(), AutoReleasedError.GetLocalizedDescription().GetPtr(), AutoReleasedError.GetLocalizedFailureReason().GetPtr());
		}
	}
	
	return bOK;
}

int main(int argc, const char * argv[])
{
	bool bOK = true;
	
	if (argc >= 3)
	{
		const char* left = argv[1];
		const char* right = argv[2];
		
		mtlpp::CompilerOptions OptionsLeft;
		OptionsLeft.AirOptions = mtlpp::AIROptions::final;
		OptionsLeft.AirPath = @"/tmp/Left.air";
		
		mtlpp::CompilerOptions OptionsRight;
		OptionsRight.AirOptions = mtlpp::AIROptions::final;
		OptionsRight.AirPath = @"/tmp/Right.air";

		for (int i = 3; i < argc; i++)
		{
			if (!strcmp(argv[i], "-l-keep-debug-info"))
			{
				OptionsLeft.KeepDebugInfo = true;
			}
			else if (!strcmp(argv[i], "-r-keep-debug-info"))
			{
				OptionsRight.KeepDebugInfo = true;
			}
			else if (!strcmp(argv[i], "-l-no-fast-math"))
			{
				OptionsLeft.SetFastMathEnabled(false);
			}
			else if (!strcmp(argv[i], "-r-no-fast-math"))
			{
				OptionsRight.SetFastMathEnabled(false);
			}
			else if (!strcmp(argv[i], "-l-fast-math"))
			{
				OptionsLeft.SetFastMathEnabled(true);
			}
			else if (!strcmp(argv[i], "-r-fast-math"))
			{
				OptionsRight.SetFastMathEnabled(true);
			}
			else if ((!strcmp(argv[i], "-l-sdk") || !strcmp(argv[i], "-r-sdk")) && i+1 < argc)
			{
				mtlpp::CompilerOptions& Options = (!strcmp(argv[i], "-l-sdk")) ? OptionsLeft : OptionsRight;
				i++;
				
				if (!strcmp(argv[i], "macosx"))
				{
					Options.Platform = mtlpp::Platform::macOS;
				}
				else if (!strcmp(argv[i], "iphoneos"))
				{
					Options.Platform = mtlpp::Platform::iOS;
				}
				else if (!strcmp(argv[i], "tvos"))
				{
					Options.Platform = mtlpp::Platform::tvOS;
				}
				else if (!strcmp(argv[i], "watchos"))
				{
					Options.Platform = mtlpp::Platform::watchOS;
				}
			}
			else if ((!strcmp(argv[i], "-l-vers") || !strcmp(argv[i], "-r-vers")) && i+1 < argc)
			{
				mtlpp::CompilerOptions& Options = (!strcmp(argv[i], "-l-vers")) ? OptionsLeft : OptionsRight;
				i++;
				if (!strcmp(argv[i], "1.0"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_0);
				}
				else if (!strcmp(argv[i], "1.1"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_1);
				}
				else if (!strcmp(argv[i], "1.2"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_2);
				}
				else if (!strcmp(argv[i], "2.0"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_0);
				}
				else if (!strcmp(argv[i], "2.1"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_1);
				}
				else if (!strcmp(argv[i], "2.2"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_2);
				}
				else if (!strcmp(argv[i], "2.3"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_3);
				}
			}
		}

		bOK = DumpAIR([NSString stringWithUTF8String:left], @"/tmp/Left.tmp", @"/tmp/Left.metallib", OptionsLeft);
		if (bOK)
		{
			bOK = DumpAIR([NSString stringWithUTF8String:right], @"/tmp/Right.tmp", @"/tmp/Right.metallib", OptionsRight);
		}
		
		if (bOK)
		{
			NSTask* t = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/opendiff" arguments:@[ OptionsLeft.AirPath.GetPtr(), OptionsRight.AirPath.GetPtr() ]];
			[t waitUntilExit];
		}
	}
	else
	{
		printf("airdiff left-path right-path <options>\n");
		printf("options:\n");
		printf("\t[-l/-r]-keep-debug-info: if specified the selected file will be compiled to keep debug info.\n");
		printf("\t[-l/-r]-no-fast-math: if specified the selected file will be compiled with fast-math explicitly disabled.\n");
		printf("\t[-l/-r]-fast-math: if specified the selected file will be compiled with fast-math explicitly enabled.\n");
		printf("\t[-l/-r]-sdk [macosx/iphoneos/tvos/watchos]: if specified the selected file will be compiled with specified platform that must follow.\n");
		printf("\t[-l/-r]-vers [1.0/1.1/1.2/2.0]: if specified the selected file will be compiled with specified shader standard that must follow.\n");
	}
	
	return !bOK;
}
