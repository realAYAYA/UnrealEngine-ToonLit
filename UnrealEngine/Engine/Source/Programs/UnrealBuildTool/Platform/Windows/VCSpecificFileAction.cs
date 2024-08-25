// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	internal class VcSpecificFileAction : Action, ISpecificFileAction
	{
		DirectoryReference SourceDir;
		DirectoryReference OutputDir;
		VCCompileAction BaseAction;

		// TODO: Entire CppCompileEnvironment needs to be saved with BinaryArchiveWriter, some options may still be unused
		bool bPreprocessOnly;
		bool bWithAssembly;

		Dictionary<string, List<FileItem>> SingleFiles = new();

		internal VcSpecificFileAction(DirectoryReference Source, DirectoryReference Output, VCCompileAction Action, CppCompileEnvironment? CompileEnvironment = null) : base(ActionType.Compile)
		{
			SourceDir = Source;
			OutputDir = Output;
			BaseAction = Action;
			bPreprocessOnly = CompileEnvironment?.bPreprocessOnly ?? false;
			bWithAssembly = CompileEnvironment?.bWithAssembly ?? false;
		}

		public VcSpecificFileAction(BinaryArchiveReader Reader) : base(ActionType.Compile)
		{
			SourceDir = Reader.ReadCompactDirectoryReference();
			OutputDir = Reader.ReadCompactDirectoryReference();
			bPreprocessOnly = Reader.ReadBool();
			bWithAssembly = Reader.ReadBool();
			BaseAction = new VCCompileAction(Reader);
		}

		public new void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteCompactDirectoryReference(SourceDir);
			Writer.WriteCompactDirectoryReference(OutputDir);
			Writer.WriteBool(bPreprocessOnly);
			Writer.WriteBool(bWithAssembly);
			BaseAction.Write(Writer);
		}

		public DirectoryReference RootDirectory => SourceDir;

		public IExternalAction? CreateAction(FileItem SourceFile, ILogger Logger)
		{
			DirectoryReference IntermediateDirectory = DirectoryReference.Combine(OutputDir, "SingleFile");
			// Keep track of all specific files, so the output file can be renamed if there's a naming conflict
			string Filename = SourceFile.Name;
			if (!SingleFiles.ContainsKey(SourceFile.Name))
			{
				SingleFiles[SourceFile.Name] = new();
			}
			else
			{
				Filename = $"{Path.GetFileNameWithoutExtension(SourceFile.Name)}{SingleFiles[Filename].Count}{Path.GetExtension(SourceFile.Name)}";
			}
			SingleFiles[SourceFile.Name].Add(SourceFile);

			// If it is a header file we need to wrap it from another file.. because otherwise it will fail when there are circular dependencies
			// (There are a lot of those, very few of the .h files in core can be compiled without errors)
			if (SourceFile.HasExtension(".h"))
			{
				DirectoryReference.CreateDirectory(IntermediateDirectory);
				FileItem DummyFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, Filename));
				Directory.CreateDirectory(DummyFile.Directory.FullName);
				File.WriteAllText(DummyFile.FullName, $"#include \"{SourceFile.FullName.Replace('\\', '/')}\"");
				SourceFile = DummyFile;
			}
			else if (!SourceFile.HasExtension(".cpp"))
			{
				return null;
			}

			DirectoryReference.CreateDirectory(IntermediateDirectory);
			VCCompileAction Action = new VCCompileAction(BaseAction);
			Action.SourceFile = SourceFile;
			if (bPreprocessOnly)
			{
				Action.PreprocessedFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.i"));
			}
			else
			{
				Action.ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.obj"));
				if (BaseAction.CompilerType.IsMSVC() && bWithAssembly)
				{
					Action.AssemblyFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.asm"));
				}
			}

			if (BaseAction.CompilerType.IsMSVC())
			{
				Action.DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.dep.json"));
			}
			else if (BaseAction.CompilerType.IsClang())
			{
				Action.DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.d"));
			}
			Action.ResponseFile = FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, $"{Filename}.rsp"));
			File.WriteAllLines(Action.ResponseFile.FullName, Action.GetCompilerArguments(Logger));

			return Action;
		}
	}

	class VcSpecificFileActionSerializer : ActionSerializerBase<VcSpecificFileAction>
	{
		/// <inheritdoc/>
		public override VcSpecificFileAction Read(BinaryArchiveReader Reader)
		{
			return new VcSpecificFileAction(Reader);
		}

		/// <inheritdoc/>
		public override void Write(BinaryArchiveWriter Writer, VcSpecificFileAction Action)
		{
			Action.Write(Writer);
		}
	}
}