// Copyright Epic Games, Inc. All Rights Reserved.

#include <Foundation/Foundation.h>
#include <Foundation/NSTask.h>
#include <Metal/MTLLibrary.h>

#include "tools.hpp"

#include "library.hpp"

MTLPP_BEGIN

@interface NSAutoReadPipe : NSObject

/** The pipe itself */
@property (readonly) NSPipe*			Pipe;
/** A file associated with the pipe from which we shall read data */
@property (readonly) NSFileHandle*		File;
/** Buffer that stores the output from the pipe */
@property (readonly) NSMutableData*		PipeOutput;

/** Initialization function */
-(id)init;

/** Deallocation function */
-(void)dealloc;

/** Callback function that is invoked when data is pushed onto the pipe */
-(void)readData: (NSNotification *)Notification;

/** Shutdown the background reader, and copy all the data from the pipe as a UTF8 encoded string */
-(void)copyPipeData: (ns::String&)OutString;

@end

@implementation NSAutoReadPipe

-(id)init
{
	[super init];
	
	_PipeOutput = [NSMutableData new];
	_Pipe = [NSPipe new];
	_File = [_Pipe fileHandleForReading];
	
	[[NSNotificationCenter defaultCenter] addObserver: self
											 selector: @selector(readData:)
												 name: NSFileHandleDataAvailableNotification
											   object: _File];
	
	[_File waitForDataInBackgroundAndNotify];
	return self;
}

-(void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[_Pipe release];
	[_PipeOutput release];
	
	[super dealloc];
}

-(void)readData: (NSNotification *)Notification
{
	NSFileHandle* FileHandle = (NSFileHandle*)Notification.object;
	
	// Ensure we're reading from the right file
	if (FileHandle == _File)
	{
		[_PipeOutput appendData: [FileHandle availableData]];
		[FileHandle waitForDataInBackgroundAndNotify];
	}
}

-(void)copyPipeData: (ns::String&)OutString
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	// Read any remaining data in from the pipe
	NSData* Data = [_File readDataToEndOfFile];
	if (Data && [Data length])
	{
		[_PipeOutput appendData: Data];
	}
	
	// Encode the data as a string
	NSString* String = [[NSString alloc] initWithData:_PipeOutput encoding:NSUTF8StringEncoding];
	
	OutString = ns::String(String);
	
	[String release];
}

@end // NSAutoReadPipe

namespace mtlpp
{
	static ns::String PlatformStandards[] = {
		@"ios",
		@"macos",
		@"tvos",
		@"watchos",
	};
	static ns::String PlatformNames[] = {
		@"iphoneos",
		@"macosx",
		@"tvos",
		@"watchos",
	};
	
	bool Compiler::Compile(const char* source, ns::String const& output, const CompilerOptions& options, ns::AutoReleasedError* error)
	{
		bool bOK = false;
		{
			int fd = -1;
			const char* tempname = "/tmp/metal.bc.XXXXXXXXXXXXXXXX";
			static size_t len = strlen(tempname);
			char filename[38];
			strcpy(filename, tempname);
			char* file = nullptr;
			while(fd == -1)
			{
				file = mktemp(filename);
				strcpy(file + len, ".metal");
				fd = open(file, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
				if (fd == -1 && errno != EEXIST)
				{
					return false;
				}
			}
			ssize_t err = write(fd, source, strlen(source));
			close(fd);
			if (err == strlen(source))
			{
				bOK = Compile(ns::String([NSString stringWithUTF8String:file]), output, options, error);
			}
		}
		return bOK;
	}
	
	bool Compiler::Compile(ns::String const& source, ns::String const& output, const CompilerOptions& options, ns::AutoReleasedError* error)
	{
		bool bOK = false;
		{
			NSTask* Task = [NSTask new];
			Task.launchPath = @"/usr/bin/xcrun";

			ns::String Ver;
			switch(options.GetLanguageVersion())
			{
				case LanguageVersion::Version3_0:
					Ver = @"3.0";
					break;
				case LanguageVersion::Version2_4:
					Ver = @"2.4";
					break;
                case LanguageVersion::Version2_3:
                    Ver = @"2.3";
                    break;
                case LanguageVersion::Version2_2:
                    Ver = @"2.2";
                    break;

                default:
					Ver = @"2.4";
					break;
			}
			
			ns::String Std([NSString stringWithFormat:@"-std=%@-metal%@", PlatformStandards[(int)options.Platform].GetPtr(), Ver.GetPtr()]);
            if([Ver isEqualToString:@"3.0"])
            {
                Std =([NSString stringWithFormat:@"-std=metal%@", Ver.GetPtr()]);
            }
			ns::String Math = options.IsFastMathEnabled() ? @"-ffast-math" : @"-fno-fast-math";
			ns::String Debug = options.KeepDebugInfo ? @"-gline-tables-only" : @"";
			ns::String MinOS = options.MinOS[0] > 0 ? [NSString stringWithFormat:@"-m%@-version-min=%d.%d", PlatformNames[(int)options.Platform].GetPtr(), options.MinOS[0], options.MinOS[1]] : @"";

			Task.arguments = @[ @"-sdk", PlatformNames[(int)options.Platform].GetPtr(), @"metal", Std.GetPtr(), Math.GetPtr(), Debug.GetPtr(), MinOS.GetPtr(), @"-c", @"-Wno-null-character", @"-fbracket-depth=1024", source.GetPtr(), @"-o", output.GetPtr() ];
			
			NSAutoReadPipe* StdOutPipe = [[NSAutoReadPipe new] autorelease];
			[Task setStandardOutput: (id)[StdOutPipe Pipe]];
			
			NSAutoReadPipe* StdErrPipe = [[NSAutoReadPipe new] autorelease];
			[Task setStandardError: (id)[StdErrPipe Pipe]];
			
			@try
			{
				[Task launch];
				
				[Task waitUntilExit];
				
				bOK = ([Task terminationStatus] == 0);
				
				if (error)
				{
					ns::String OutString;
					[StdOutPipe copyPipeData:OutString];
					
					ns::String ErrString;
					[StdErrPipe copyPipeData:ErrString];
					
					*error = [NSError errorWithDomain:MTLLibraryErrorDomain code:(bOK ? MTLLibraryErrorCompileWarning : MTLLibraryErrorCompileFailure) userInfo:@{ NSLocalizedDescriptionKey : OutString.GetPtr(), NSLocalizedFailureReasonErrorKey : ErrString.GetPtr() }];
				}
			}
			@catch (NSException* Exc)
			{
				ns::String([Exc reason]);
				bOK = false;
				
				if (error)
				{
					NSMutableDictionary* Dict = [NSMutableDictionary dictionaryWithDictionary:Exc.userInfo];
					[Dict addEntriesFromDictionary:@{ NSLocalizedDescriptionKey : Exc.name, NSLocalizedFailureReasonErrorKey : Exc.reason }];
					*error = [NSError errorWithDomain:Exc.name code:0 userInfo:Dict];
				}
			}
		}
		return bOK;
	}
	
	bool Compiler::Link(ns::Array<ns::String> const& source, ns::String const& output, const CompilerOptions& options, ns::AutoReleasedError* error)
	{
		bool bOK = false;
		{
			ns::String sourcePath;
			if (source.GetSize() == 1)
			{
				sourcePath = source[0];
				
				bOK = true;
			}
			else
			{
				const char* tempname = "/tmp/metalar.XXXXXXXXXXXXXXXX";
				char filename[32];
				strcpy(filename, tempname);
				
				sourcePath = [NSString stringWithUTF8String:filename];
				
				bOK = true;
				
				for (unsigned i = 0; bOK && i < source.GetSize(); i++)
				{
					NSTask* Task = [NSTask new];
					Task.launchPath = @"/usr/bin/xcrun";
					
					Task.arguments = @[ @"-sdk", PlatformNames[(int)options.Platform].GetPtr(), @"metal-ar", @"q", sourcePath.GetPtr(), source[i].GetPtr() ];
					
					NSAutoReadPipe* StdOutPipe = [[NSAutoReadPipe new] autorelease];
					[Task setStandardOutput: (id)[StdOutPipe Pipe]];
					
					NSAutoReadPipe* StdErrPipe = [[NSAutoReadPipe new] autorelease];
					[Task setStandardError: (id)[StdErrPipe Pipe]];
					
					@try
					{
						[Task launch];
						
						[Task waitUntilExit];
						
						bOK = ([Task terminationStatus] == 0);
						
						if (error && !bOK)
						{
							
							ns::String OutString;
							[StdOutPipe copyPipeData:OutString];
							
							ns::String ErrString;
							[StdErrPipe copyPipeData:ErrString];
							
							*error = [NSError errorWithDomain:MTLLibraryErrorDomain code:(MTLLibraryErrorCompileFailure) userInfo:@{ NSLocalizedDescriptionKey : OutString.GetPtr(), NSLocalizedFailureReasonErrorKey : ErrString.GetPtr() }];
						}
					}
					@catch (NSException* Exc)
					{
						ns::String([Exc reason]);
						bOK = false;
						
						if (error)
						{
							NSMutableDictionary* Dict = [NSMutableDictionary dictionaryWithDictionary:Exc.userInfo];
							[Dict addEntriesFromDictionary:@{ NSLocalizedDescriptionKey : Exc.name, NSLocalizedFailureReasonErrorKey : Exc.reason }];
							*error = [NSError errorWithDomain:Exc.name code:0 userInfo:Dict];
						}
					}
				}
			}
			
			if (bOK)
			{
				NSTask* Task = [NSTask new];
				Task.launchPath = @"/usr/bin/xcrun";
				
				if (options.AirOptions != AIROptions::none)
				{
					Task.arguments = @[ @"-sdk", PlatformNames[(int)options.Platform].GetPtr(), @"metallib", @"-o", output.GetPtr(), sourcePath.GetPtr(), @"-print-after-all" ];
				}
				else
				{
					Task.arguments = @[ @"-sdk", PlatformNames[(int)options.Platform].GetPtr(), @"metallib", @"-o", output.GetPtr(), sourcePath.GetPtr() ];
				}
				
				NSAutoReadPipe* StdOutPipe = [[NSAutoReadPipe new] autorelease];
				[Task setStandardOutput: (id)[StdOutPipe Pipe]];
				
				NSAutoReadPipe* StdErrPipe = [[NSAutoReadPipe new] autorelease];
				[Task setStandardError: (id)[StdErrPipe Pipe]];
				
				@try
				{
					[Task launch];
					
					[Task waitUntilExit];
					
					bOK = ([Task terminationStatus] == 0);
					
					ns::String OutString;
					[StdOutPipe copyPipeData:OutString];
					
					ns::String ErrString;
					[StdErrPipe copyPipeData:ErrString];
					
					if (error)
					{
						*error = [NSError errorWithDomain:MTLLibraryErrorDomain code:(bOK ? MTLLibraryErrorCompileWarning : MTLLibraryErrorCompileFailure) userInfo:@{ NSLocalizedDescriptionKey : OutString.GetPtr(), NSLocalizedFailureReasonErrorKey : ErrString.GetPtr() }];
					}
					
					if (bOK && options.AirOptions != AIROptions::none && (options.AirPath || options.AirString))
					{
						char const* cStr = ErrString.GetCStr();
						char const* subStr = options.AirOptions == AIROptions::final ? strstr(cStr, "*** IR Dump After Mark required function constants ***") : cStr;
						
						if (options.AirString)
						{
							*options.AirString = ns::String([NSString stringWithUTF8String:subStr]);
						}
						
						if (options.AirPath)
						{
							int fd = open(options.AirPath.GetCStr(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
							write(fd, subStr, strlen(subStr));
							close(fd);
						}
					}
				}
				@catch (NSException* Exc)
				{
					ns::String([Exc reason]);
					bOK = false;
					
					if (error)
					{
						NSMutableDictionary* Dict = [NSMutableDictionary dictionaryWithDictionary:Exc.userInfo];
						[Dict addEntriesFromDictionary:@{ NSLocalizedDescriptionKey : Exc.name, NSLocalizedFailureReasonErrorKey : Exc.reason }];
						*error = [NSError errorWithDomain:Exc.name code:0 userInfo:Dict];
					}
				}
			}
		}
		return bOK;
	}
}

MTLPP_END
