// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTracing.Util;
using UnrealBuildBase;
using UnrealBuildTool;

namespace UnrealBuildToolTests
{
	/// <summary>
	/// Tests for reading source file markup
	/// </summary>
	[TestClass]
	public class SourceFileTests
	{
		[TestMethod]
		public void Run()
		{
			List<DirectoryReference> BaseDirectories = new List<DirectoryReference>();
			BaseDirectories.Add(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Runtime"));
			BaseDirectories.Add(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Developer"));
			BaseDirectories.Add(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Editor"));

			foreach (FileReference PluginFile in PluginsBase.EnumeratePlugins((FileReference)null))
			{
				DirectoryReference PluginSourceDir = DirectoryReference.Combine(PluginFile.Directory, "Source");
				if (DirectoryReference.Exists(PluginSourceDir))
				{
					BaseDirectories.Add(PluginSourceDir);
				}
			}

			ConcurrentBag<SourceFile> SourceFiles = new ConcurrentBag<SourceFile>();
			using (GlobalTracer.Instance.BuildSpan("Scanning source files").StartActive())
			{
				using (ThreadPoolWorkQueue Queue = new ThreadPoolWorkQueue())
				{
					foreach (DirectoryReference BaseDirectory in BaseDirectories)
					{
						Queue.Enqueue(() => ParseSourceFiles(DirectoryItem.GetItemByDirectoryReference(BaseDirectory), SourceFiles, Queue));
					}
				}
			}

			FileReference TempDataFile = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Temp", "SourceFileTests.bin");
			DirectoryReference.CreateDirectory(TempDataFile.Directory);

			using (GlobalTracer.Instance.BuildSpan("Writing source file data").StartActive())
			{
				using (BinaryArchiveWriter Writer = new BinaryArchiveWriter(TempDataFile))
				{
					Writer.WriteList(SourceFiles.ToList(), x => x.Write(Writer));
				}
			}

			List<SourceFile> ReadSourceFiles = new List<SourceFile>();
			using (GlobalTracer.Instance.BuildSpan("Reading source file data").StartActive())
			{
				using (BinaryArchiveReader Reader = new BinaryArchiveReader(TempDataFile))
				{
					ReadSourceFiles = Reader.ReadList(() => new SourceFile(Reader));
				}
			}
		}

		static void ParseSourceFiles(DirectoryItem Directory, ConcurrentBag<SourceFile> SourceFiles, ThreadPoolWorkQueue Queue)
		{
			foreach (DirectoryItem SubDirectory in Directory.EnumerateDirectories())
			{
				Queue.Enqueue(() => ParseSourceFiles(SubDirectory, SourceFiles, Queue));
			}

			foreach (FileItem File in Directory.EnumerateFiles())
			{
				if (File.HasExtension(".h") || File.HasExtension(".cpp"))
				{
					Queue.Enqueue(() => SourceFiles.Add(new SourceFile(File)));
				}
			}
		}
	}
}
