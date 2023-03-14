// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Caches information about C++ source files; whether they contain reflection markup, what the first included header is, and so on.
	/// </summary>
	class SourceFileMetadataCache
	{
		/// <summary>
		/// Information about the first file included from a source file
		/// </summary>
		class IncludeInfo
		{
			/// <summary>
			/// Last write time of the file when the data was cached
			/// </summary>
			public long LastWriteTimeUtc;

			/// <summary>
			/// Contents of the include directive
			/// </summary>
			public string? IncludeText;
		}

		/// <summary>
		/// Information about whether a file contains reflection markup
		/// </summary>
		class ReflectionInfo
		{
			/// <summary>
			/// Last write time of the file when the data was cached
			/// </summary>
			public long LastWriteTimeUtc;

			/// <summary>
			/// Whether or not the file contains reflection markup
			/// </summary>
			public bool bContainsMarkup;
		}

		/// <summary>
		/// Information about whether a file contains the inline gen.cpp macro
		/// </summary>
		class InlineReflectionInfo
		{
			/// <summary>
			/// Last write time of the file when the data was cached
			/// </summary>
			public long LastWriteTimeUtc;

			/// <summary>
			/// List of files this particular file is inlining
			/// </summary>
			public List<string> InlinedFileNames = new List<string>();
		}

		/// <summary>
		/// The current file version
		/// </summary>
		public const int CurrentVersion = 4;

		/// <summary>
		/// Location of this dependency cache
		/// </summary>
		FileReference Location;

		/// <summary>
		/// Directory for files to cache dependencies for.
		/// </summary>
		DirectoryReference BaseDirectory;

		/// <summary>
		/// The parent cache.
		/// </summary>
		SourceFileMetadataCache? Parent;

		/// <summary>
		/// Map from file item to source file info
		/// </summary>
		ConcurrentDictionary<FileItem, IncludeInfo> FileToIncludeInfo = new ConcurrentDictionary<FileItem, IncludeInfo>();

		/// <summary>
		/// Map from file item to header file info
		/// </summary>
		ConcurrentDictionary<FileItem, ReflectionInfo> FileToReflectionInfo = new ConcurrentDictionary<FileItem, ReflectionInfo>();

		/// <summary>
		/// Map from file item to inline info
		/// </summary>
		ConcurrentDictionary<FileItem, InlineReflectionInfo> FileToInlineReflectionInfo = new ConcurrentDictionary<FileItem, InlineReflectionInfo>();

		/// <summary>
		/// Map from file item to source file info
		/// </summary>
		ConcurrentDictionary<FileItem, SourceFile> FileToSourceFile = new ConcurrentDictionary<FileItem, SourceFile>();

		/// <summary>
		/// Whether the cache has been modified and needs to be saved
		/// </summary>
		bool bModified;

		/// <summary>
		/// Logger for output
		/// </summary>
		ILogger Logger;

		/// <summary>
		/// Regex that matches C++ code with UObject declarations which we will need to generated code for.
		/// </summary>
		static readonly Regex ReflectionMarkupRegex = new Regex("^\\s*U(CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\\b", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Regex that matches #include UE_INLINE_GENERATED_CPP_BY_NAME(****) statements.
		/// </summary>
		static readonly Regex InlineReflectionMarkupRegex = new Regex("^\\s*#\\s*include\\s*UE_INLINE_GENERATED_CPP_BY_NAME\\((.+)\\)", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Regex that matches #include statements.
		/// </summary>
		static readonly Regex IncludeRegex = new Regex("^[ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">]", RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex that matches #import directives in mm files
		/// </summary>
		static readonly Regex ImportRegex = new Regex("^[ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">]", RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Static cache of all constructed dependency caches
		/// </summary>
		static Dictionary<FileReference, SourceFileMetadataCache> Caches = new Dictionary<FileReference, SourceFileMetadataCache>();

		/// <summary>
		/// Constructs a dependency cache. This method is private; call CppDependencyCache.Create() to create a cache hierarchy for a given project.
		/// </summary>
		/// <param name="Location">File to store the cache</param>
		/// <param name="BaseDir">Base directory for files that this cache should store data for</param>
		/// <param name="Parent">The parent cache to use</param>
		/// <param name="Logger">Logger for output</param>
		private SourceFileMetadataCache(FileReference Location, DirectoryReference BaseDir, SourceFileMetadataCache? Parent, ILogger Logger)
		{
			this.Location = Location;
			this.BaseDirectory = BaseDir;
			this.Parent = Parent;
			this.Logger = Logger;

			if(FileReference.Exists(Location))
			{
				using (GlobalTracer.Instance.BuildSpan("Reading source file metadata cache").StartActive())
				{
					Read();
				}
			}
		}

		/// <summary>
		/// Gets the first included file from a source file
		/// </summary>
		/// <param name="SourceFile">The source file to parse</param>
		/// <returns>Text from the first include directive. Null if the file did not contain any include directives.</returns>
		public string? GetFirstInclude(FileItem SourceFile)
		{
			if(Parent != null && !SourceFile.Location.IsUnderDirectory(BaseDirectory))
			{
				return Parent.GetFirstInclude(SourceFile);
			}
			else
			{
				IncludeInfo? IncludeInfo;
				if(!FileToIncludeInfo.TryGetValue(SourceFile, out IncludeInfo) || SourceFile.LastWriteTimeUtc.Ticks > IncludeInfo.LastWriteTimeUtc)
				{
					IncludeInfo = new IncludeInfo();
					IncludeInfo.LastWriteTimeUtc = SourceFile.LastWriteTimeUtc.Ticks;
					IncludeInfo.IncludeText = ParseFirstInclude(SourceFile.Location);
					FileToIncludeInfo[SourceFile] = IncludeInfo;
					bModified = true;
				}
				return IncludeInfo.IncludeText;
			}
		}

		/// <summary>
		/// Finds or adds a SourceFile class for the given file
		/// </summary>
		/// <param name="File">File to fetch the source file data for</param>
		/// <returns>SourceFile instance corresponding to the given source file</returns>
		public SourceFile GetSourceFile(FileItem File)
		{
			if (Parent != null && !File.Location.IsUnderDirectory(BaseDirectory))
			{
				return Parent.GetSourceFile(File);
			}
			else
			{
				SourceFile? Result;
				if (!FileToSourceFile.TryGetValue(File, out Result) || File.LastWriteTimeUtc.Ticks > Result.LastWriteTimeUtc)
				{
					SourceFile NewSourceFile = new SourceFile(File);
					if (Result == null)
					{
						if (FileToSourceFile.TryAdd(File, NewSourceFile))
						{
							Result = NewSourceFile;
						}
						else
						{
							Result = FileToSourceFile[File];
						}
					}
					else
					{
						if (FileToSourceFile.TryUpdate(File, NewSourceFile, Result))
						{
							Result = NewSourceFile;
						}
						else
						{
							Result = FileToSourceFile[File];
						}
					}
				}
				return Result;
			}
		}

		/// <summary>
		/// Determines whether the given file contains reflection markup
		/// </summary>
		/// <param name="SourceFile">The source file to parse</param>
		/// <returns>True if the file contains reflection markup</returns>
		public bool ContainsReflectionMarkup(FileItem SourceFile)
		{
			if(Parent != null && !SourceFile.Location.IsUnderDirectory(BaseDirectory))
			{
				return Parent.ContainsReflectionMarkup(SourceFile);
			}
			else
			{
				ReflectionInfo? ReflectionInfo;
				if(!FileToReflectionInfo.TryGetValue(SourceFile, out ReflectionInfo) || SourceFile.LastWriteTimeUtc.Ticks > ReflectionInfo.LastWriteTimeUtc)
				{
					ReflectionInfo = new ReflectionInfo();
					ReflectionInfo.LastWriteTimeUtc = SourceFile.LastWriteTimeUtc.Ticks;
					ReflectionInfo.bContainsMarkup = ReflectionMarkupRegex.IsMatch(FileReference.ReadAllText(SourceFile.Location));
					FileToReflectionInfo[SourceFile] = ReflectionInfo;
					bModified = true;
				}
				return ReflectionInfo.bContainsMarkup;
			}
		}


		/// <summary>
		/// Returns a list of inlined generated cpps that this source file contains.
		/// </summary>
		/// <param name="SourceFile">The source file to parse</param>
		/// <returns>List of marked files this source file contains</returns>
		public IList<string> GetListOfInlinedGeneratedCppFiles(FileItem SourceFile)
		{
			if (Parent != null && !SourceFile.Location.IsUnderDirectory(BaseDirectory))
			{
				return Parent.GetListOfInlinedGeneratedCppFiles(SourceFile);
			}
			else
			{
				InlineReflectionInfo? InlineReflectionInfo;
				if (!FileToInlineReflectionInfo.TryGetValue(SourceFile, out InlineReflectionInfo) || SourceFile.LastWriteTimeUtc.Ticks > InlineReflectionInfo.LastWriteTimeUtc)
				{
					InlineReflectionInfo = new InlineReflectionInfo();
					InlineReflectionInfo.LastWriteTimeUtc = SourceFile.LastWriteTimeUtc.Ticks;
					MatchCollection FileMatches = InlineReflectionMarkupRegex.Matches(FileReference.ReadAllText(SourceFile.Location));
					foreach (Match Match in FileMatches)
					{
						InlineReflectionInfo.InlinedFileNames.Add(Match.Groups[1].Value);
					}
					FileToInlineReflectionInfo[SourceFile] = InlineReflectionInfo;
					bModified = true;
				}
				return InlineReflectionInfo.InlinedFileNames;
			}
		}

		/// <summary>
		/// Parse the first include directive from a source file
		/// </summary>
		/// <param name="SourceFile">The source file to parse</param>
		/// <returns>The first include directive</returns>
		static string? ParseFirstInclude(FileReference SourceFile)
		{
			bool bMatchImport = SourceFile.HasExtension(".m") || SourceFile.HasExtension(".mm");
			using(StreamReader Reader = new StreamReader(SourceFile.FullName, true))
			{
				for(;;)
				{
					string? Line = Reader.ReadLine();
					if(Line == null)
					{
						return null;
					}

					Match IncludeMatch = IncludeRegex.Match(Line);
					if(IncludeMatch.Success)
					{
						return IncludeMatch.Groups[1].Value;
					}

					if(bMatchImport)
					{
						Match ImportMatch = ImportRegex.Match(Line);
						if(ImportMatch.Success)
						{
							return IncludeMatch.Groups[1].Value;
						}
					}
				}
			}
		}

		/// <summary>
		/// Creates a cache hierarchy for a particular target
		/// </summary>
		/// <param name="ProjectFile">Project file for the target being built</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Dependency cache hierarchy for the given project</returns>
		public static SourceFileMetadataCache CreateHierarchy(FileReference? ProjectFile, ILogger Logger)
		{
			SourceFileMetadataCache? Cache = null;

			if(ProjectFile == null || !Unreal.IsEngineInstalled())
			{
				FileReference EngineCacheLocation = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "SourceFileCache.bin");
				Cache = FindOrAddCache(EngineCacheLocation, Unreal.EngineDirectory, Cache, Logger);
			}

			if(ProjectFile != null)
			{
				FileReference ProjectCacheLocation = FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", "SourceFileCache.bin");
				Cache = FindOrAddCache(ProjectCacheLocation, ProjectFile.Directory, Cache, Logger);
			}

			return Cache!;
		}

		/// <summary>
		/// Enumerates all the locations of metadata caches for the given target
		/// </summary>
		/// <param name="ProjectFile">Project file for the target being built</param>
		/// <returns>Dependency cache hierarchy for the given project</returns>
		public static IEnumerable<FileReference> GetFilesToClean(FileReference? ProjectFile)
		{
			if(ProjectFile == null || !Unreal.IsEngineInstalled())
			{
				yield return FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "SourceFileCache.bin");
			}
			if(ProjectFile != null)
			{
				yield return FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", "SourceFileCache.bin");
			}
		}

		/// <summary>
		/// Reads a cache from the given location, or creates it with the given settings
		/// </summary>
		/// <param name="Location">File to store the cache</param>
		/// <param name="BaseDirectory">Base directory for files that this cache should store data for</param>
		/// <param name="Parent">The parent cache to use</param>
		/// <param name="Logger"></param>
		/// <returns>Reference to a dependency cache with the given settings</returns>
		static SourceFileMetadataCache FindOrAddCache(FileReference Location, DirectoryReference BaseDirectory, SourceFileMetadataCache? Parent, ILogger Logger)
		{
			lock(Caches)
			{
				SourceFileMetadataCache? Cache;
				if(Caches.TryGetValue(Location, out Cache))
				{
					Debug.Assert(Cache.BaseDirectory == BaseDirectory);
					Debug.Assert(Cache.Parent == Parent);
				}
				else
				{
					Cache = new SourceFileMetadataCache(Location, BaseDirectory, Parent, Logger);
					Caches.Add(Location, Cache);
				}
				return Cache;
			}
		}

		/// <summary>
		/// Save all the caches that have been modified
		/// </summary>
		public static void SaveAll()
		{
			Parallel.ForEach(Caches.Values, Cache => { if(Cache.bModified){ Cache.Write(); } });
		}

		/// <summary>
		/// Reads data for this dependency cache from disk
		/// </summary>
		private void Read()
		{
			try
			{
				using(BinaryArchiveReader Reader = new BinaryArchiveReader(Location))
				{
					int Version = Reader.ReadInt();
					if(Version != CurrentVersion)
					{
						Logger.LogDebug("Unable to read dependency cache from {File}; version {Version} vs current {CurrentVersion}", Location, Version, CurrentVersion);
						return;
					}

					int FileToFirstIncludeCount = Reader.ReadInt();
					for(int Idx = 0; Idx < FileToFirstIncludeCount; Idx++)
					{
						FileItem File = Reader.ReadCompactFileItem();
						
						IncludeInfo IncludeInfo = new IncludeInfo();
						IncludeInfo.LastWriteTimeUtc = Reader.ReadLong();
						IncludeInfo.IncludeText = Reader.ReadString();

						FileToIncludeInfo[File] = IncludeInfo;
					}

					int FileToMarkupFlagCount = Reader.ReadInt();
					for(int Idx = 0; Idx < FileToMarkupFlagCount; Idx++)
					{
						FileItem File = Reader.ReadCompactFileItem();

						ReflectionInfo ReflectionInfo = new ReflectionInfo();
						ReflectionInfo.LastWriteTimeUtc = Reader.ReadLong();
						ReflectionInfo.bContainsMarkup = Reader.ReadBool();

						FileToReflectionInfo[File] = ReflectionInfo;
					}

					int FileToInlineMarkupFlagCount = Reader.ReadInt();
					for (int Idx = 0; Idx < FileToInlineMarkupFlagCount; Idx++)
					{
						FileItem File = Reader.ReadCompactFileItem();

						InlineReflectionInfo InlineReflectionInfo = new InlineReflectionInfo();
						InlineReflectionInfo.LastWriteTimeUtc = Reader.ReadLong();
						InlineReflectionInfo.InlinedFileNames = Reader.ReadList(() => Reader.ReadString())!;

						FileToInlineReflectionInfo[File] = InlineReflectionInfo;
					}
				}
			}
			catch(Exception Ex)
			{
				Logger.LogWarning("Unable to read {Location}. See log for additional information.", Location);
				Logger.LogDebug(Ex, "{Ex}", ExceptionUtils.FormatExceptionDetails(Ex));
			}
		}

		/// <summary>
		/// Writes data for this dependency cache to disk
		/// </summary>
		private void Write()
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using(FileStream Stream = File.Open(Location.FullName, FileMode.Create, FileAccess.Write, FileShare.Read))
			{
				using(BinaryArchiveWriter Writer = new BinaryArchiveWriter(Stream))
				{
					Writer.WriteInt(CurrentVersion);

					Writer.WriteInt(FileToIncludeInfo.Count);
					foreach(KeyValuePair<FileItem, IncludeInfo> Pair in FileToIncludeInfo)
					{
						Writer.WriteCompactFileItem(Pair.Key);
						Writer.WriteLong(Pair.Value.LastWriteTimeUtc);
						Writer.WriteString(Pair.Value.IncludeText);
					}

					Writer.WriteInt(FileToReflectionInfo.Count);
					foreach(KeyValuePair<FileItem, ReflectionInfo> Pair in FileToReflectionInfo)
					{
						Writer.WriteCompactFileItem(Pair.Key);
						Writer.WriteLong(Pair.Value.LastWriteTimeUtc);
						Writer.WriteBool(Pair.Value.bContainsMarkup);
					}

					Writer.WriteInt(FileToInlineReflectionInfo.Count);
					foreach (KeyValuePair<FileItem, InlineReflectionInfo> Pair in FileToInlineReflectionInfo)
					{
						Writer.WriteCompactFileItem(Pair.Key);
						Writer.WriteLong(Pair.Value.LastWriteTimeUtc);
						Writer.WriteList(Pair.Value.InlinedFileNames, Item => Writer.WriteString(Item));
					}
				}
			}
			bModified = false;
		}
	}
}
