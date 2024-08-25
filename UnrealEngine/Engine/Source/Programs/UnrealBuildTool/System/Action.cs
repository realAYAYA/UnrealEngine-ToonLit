// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Enumerates build action types.
	/// </summary>
	enum ActionType
	{
		BuildProject,

		GatherModuleDependencies,

		CompileModuleInterface,

		Compile,

		CreateAppBundle,

		GenerateDebugInfo,

		Link,

		WriteMetadata,

		PostBuildStep,

		ParseTimingInfo,
	}

	/// <summary>
	/// Defines the action's support for artifacts
	/// </summary>
	[Flags]
	enum ArtifactMode : byte
	{

		/// <summary>
		/// Cached artifacts aren't enabled
		/// </summary>
		None = 0,

		/// <summary>
		/// If set, the outputs should be cached but respect the flags below
		/// </summary>
		Enabled = 1 << 0,

		/// <summary>
		/// Absolute file paths must be used when recording inputs and outputs
		/// </summary>
		AbsolutePath = 1 << 1,

		/// <summary>
		/// For actions that can't be cached, by setting this flag, the inputs for the action in question
		/// will be used as additional inputs for any action that references outputs of this action.  
		/// For example, PCH files shouldn't be cached, but compiles that use the PCH should still be 
		/// valid if consider the inputs used to create the PCH file.
		/// </summary>
		PropagateInputs = 1 << 2,
	}

	interface IExternalAction
	{
		/// <summary>
		/// The type of this action (for debugging purposes).
		/// </summary>
		ActionType ActionType { get; }

		/// <summary>
		/// Artifact support for this step
		/// </summary>
		ArtifactMode ArtifactMode { get; }

		/// <summary>
		/// Every file this action depends on.  These files need to exist and be up to date in order for this action to even be considered
		/// </summary>
		IEnumerable<FileItem> PrerequisiteItems { get; }

		/// <summary>
		/// The files that this action produces after completing
		/// </summary>
		IEnumerable<FileItem> ProducedItems { get; }

		/// <summary>
		/// Items that should be deleted before running this action
		/// </summary>
		IEnumerable<FileItem> DeleteItems { get; }

		/// <summary>
		/// For C++ source files, specifies a dependency list file used to check changes to header files
		/// </summary>
		FileItem? DependencyListFile { get; }

		/// <summary>
		/// Directory from which to execute the program to create produced items
		/// </summary>
		DirectoryReference WorkingDirectory { get; }

		/// <summary>
		/// The command to run to create produced items
		/// </summary>
		FileReference CommandPath { get; }

		/// <summary>
		/// Command-line parameters to pass to the program
		/// </summary>
		string CommandArguments { get; }

		/// <summary>
		/// Version of the command used for this action. This will be considered a dependency.
		/// </summary>
		string CommandVersion { get; }

		/// <summary>
		/// Optional friendly description of the type of command being performed, for example "Compile" or "Link".  Displayed by some executors.
		/// </summary>
		string CommandDescription { get; }

		/// <summary>
		/// Human-readable description of this action that may be displayed as status while invoking the action.  This is often the name of the file being compiled, or an executable file name being linked.  Displayed by some executors.
		/// </summary>
		string StatusDescription { get; }

		/// <summary>
		/// True if this action is allowed to be run on a remote machine when a distributed build system is being used, such as XGE
		/// </summary>
		bool bCanExecuteRemotely { get; }

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with SNDBS. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		bool bCanExecuteRemotelyWithSNDBS { get; }

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with XGE. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		bool bCanExecuteRemotelyWithXGE { get; }

		/// <summary>
		/// True if this action can be executed by box
		/// </summary>
		bool bCanExecuteInUBA { get; }

		/// <summary>
		/// True if this action is using the GCC compiler.  Some build systems may be able to optimize for this case.
		/// </summary>
		bool bIsGCCCompiler { get; }

		/// <summary>
		/// Whether we should log this action, whether executed locally or remotely.  This is useful for actions that take time
		/// but invoke tools without any console output.
		/// </summary>
		bool bShouldOutputStatusDescription { get; }

		/// <summary>
		/// True if any libraries produced by this action should be considered 'import libraries'
		/// </summary>
		bool bProducesImportLibrary { get; }

		/// <summary>
		/// Whether changes in the command line used to generate these produced items should invalidate the action
		/// </summary>
		bool bUseActionHistory { get; }

		/// <summary>
		/// True if this action should be scheduled early. Some actions can take a long time (pch) and need to start very early
		/// If set to true it will propagate high priority to all its prerequisite tasks
		/// </summary>
		bool bIsHighPriority { get; }

		/// <summary>
		/// Used to determine how much weight(CPU and Memory work) this action is.
		/// </summary>
		double Weight { get; }
	}

	/// <summary>
	/// A build action.
	/// </summary>
	class Action : IExternalAction
	{
		///
		/// Preparation and Assembly (serialized)
		/// 

		/// <summary>
		/// The type of this action (for debugging purposes).
		/// </summary>
		public ActionType ActionType { get; set; }

		/// <summary>
		/// Artifact support for this step
		/// </summary>
		public ArtifactMode ArtifactMode { get; set; } = ArtifactMode.None;

		/// <summary>
		/// Every file this action depends on.  These files need to exist and be up to date in order for this action to even be considered
		/// </summary>
		public SortedSet<FileItem> PrerequisiteItems { get; set; } = new SortedSet<FileItem>();

		/// <summary>
		/// The files that this action produces after completing
		/// </summary>
		public SortedSet<FileItem> ProducedItems { get; set; } = new SortedSet<FileItem>();

		/// <summary>
		/// Items that should be deleted before running this action
		/// </summary>
		public SortedSet<FileItem> DeleteItems { get; set; } = new SortedSet<FileItem>();

		/// <summary>
		/// For C++ source files, specifies a dependency list file used to check changes to header files
		/// </summary>
		public FileItem? DependencyListFile { get; set; }

		/// <summary>
		/// Directory from which to execute the program to create produced items
		/// </summary>
		public DirectoryReference WorkingDirectory { get; set; } = null!;

		/// <summary>
		/// The command to run to create produced items
		/// </summary>
		public FileReference CommandPath { get; set; } = null!;

		/// <summary>
		/// Command-line parameters to pass to the program
		/// </summary>
		public string CommandArguments { get; set; } = null!;

		/// <summary>
		/// Version of the command used for this action. This will be considered a dependency.
		/// </summary>
		public string CommandVersion { get; set; } = "0";

		/// <summary>
		/// Optional friendly description of the type of command being performed, for example "Compile" or "Link".  Displayed by some executors.
		/// </summary>
		public string CommandDescription { get; set; } = null!;

		/// <summary>
		/// Human-readable description of this action that may be displayed as status while invoking the action.  This is often the name of the file being compiled, or an executable file name being linked.  Displayed by some executors.
		/// </summary>
		public string StatusDescription { get; set; } = "...";

		/// <summary>
		/// True if this action is allowed to be run on a remote machine when a distributed build system is being used, such as XGE
		/// </summary>
		public bool bCanExecuteRemotely { get; set; } = false;

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with SNDBS. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		public bool bCanExecuteRemotelyWithSNDBS { get; set; } = true;

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with XGE. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		public bool bCanExecuteRemotelyWithXGE { get; set; } = true;

		/// <summary>
		/// True if this action can be executed in box
		/// </summary>
		public bool bCanExecuteInUBA { get; set; } = true;

		/// <summary>
		/// True if this action is using the GCC compiler.  Some build systems may be able to optimize for this case.
		/// </summary>
		public bool bIsGCCCompiler { get; set; } = false;

		/// <summary>
		/// Whether we should log this action, whether executed locally or remotely.  This is useful for actions that take time
		/// but invoke tools without any console output.
		/// </summary>
		public bool bShouldOutputStatusDescription { get; set; } = true;

		/// <summary>
		/// True if any libraries produced by this action should be considered 'import libraries'
		/// </summary>
		public bool bProducesImportLibrary { get; set; } = false;

		/// <inheritdoc/>
		public bool bUseActionHistory { get; set; } = true;

		/// <inheritdoc/>
		public bool bIsHighPriority { get; set; } = false;

		/// <inheritdoc/>
		public double Weight { get; set; } = 1.0;

		IEnumerable<FileItem> IExternalAction.PrerequisiteItems => PrerequisiteItems;
		IEnumerable<FileItem> IExternalAction.ProducedItems => ProducedItems;
		IEnumerable<FileItem> IExternalAction.DeleteItems => DeleteItems;

		public Action(ActionType InActionType)
		{
			ActionType = InActionType;

			// link actions are going to run locally on SN-DBS so don't try to distribute them as that generates warnings for missing tool templates
			if (ActionType == ActionType.Link)
			{
				bCanExecuteRemotelyWithSNDBS = false;
			}
		}

		public Action(IExternalAction InOther)
		{
			ActionType = InOther.ActionType;
			ArtifactMode = InOther.ArtifactMode;
			PrerequisiteItems = new SortedSet<FileItem>(InOther.PrerequisiteItems);
			ProducedItems = new SortedSet<FileItem>(InOther.ProducedItems);
			DeleteItems = new SortedSet<FileItem>(InOther.DeleteItems);
			DependencyListFile = InOther.DependencyListFile;
			WorkingDirectory = InOther.WorkingDirectory;
			CommandPath = InOther.CommandPath;
			CommandArguments = InOther.CommandArguments;
			CommandVersion = InOther.CommandVersion;
			CommandDescription = InOther.CommandDescription;
			StatusDescription = InOther.StatusDescription;
			bCanExecuteRemotely = InOther.bCanExecuteRemotely;
			bCanExecuteRemotelyWithSNDBS = InOther.bCanExecuteRemotelyWithSNDBS;
			bCanExecuteRemotelyWithXGE = InOther.bCanExecuteRemotelyWithXGE;
			bCanExecuteInUBA = InOther.bCanExecuteInUBA;
			bIsGCCCompiler = InOther.bIsGCCCompiler;
			bShouldOutputStatusDescription = InOther.bShouldOutputStatusDescription;
			bProducesImportLibrary = InOther.bProducesImportLibrary;
			bUseActionHistory = InOther.bUseActionHistory;
			bIsHighPriority = InOther.bIsHighPriority;
			Weight = InOther.Weight;
		}

		public Action(BinaryArchiveReader Reader)
		{
			ActionType = (ActionType)Reader.ReadByte();
			ArtifactMode = (ArtifactMode)Reader.ReadByte();
			WorkingDirectory = Reader.ReadDirectoryReferenceNotNull();
			CommandPath = Reader.ReadFileReference();
			CommandArguments = Reader.ReadString()!;
			CommandVersion = Reader.ReadString()!;
			CommandDescription = Reader.ReadString()!;
			StatusDescription = Reader.ReadString()!;
			bCanExecuteRemotely = Reader.ReadBool();
			bCanExecuteRemotelyWithSNDBS = Reader.ReadBool();
			bCanExecuteRemotelyWithXGE = Reader.ReadBool();
			bCanExecuteInUBA = Reader.ReadBool();
			bIsGCCCompiler = Reader.ReadBool();
			bShouldOutputStatusDescription = Reader.ReadBool();
			bProducesImportLibrary = Reader.ReadBool();
			PrerequisiteItems = Reader.ReadSortedSet(() => Reader.ReadFileItem())!;
			ProducedItems = Reader.ReadSortedSet(() => Reader.ReadFileItem())!;
			DeleteItems = Reader.ReadSortedSet(() => Reader.ReadFileItem())!;
			DependencyListFile = Reader.ReadFileItem();
			bUseActionHistory = Reader.ReadBool();
			bIsHighPriority = Reader.ReadBool();
			Weight = Reader.ReadDouble();

		}

		/// <summary>
		/// ISerializable: Called when serialized to report additional properties that should be saved
		/// </summary>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteByte((byte)ActionType);
			Writer.WriteByte((byte)ArtifactMode);
			Writer.WriteDirectoryReference(WorkingDirectory);
			Writer.WriteFileReference(CommandPath);
			Writer.WriteString(CommandArguments);
			Writer.WriteString(CommandVersion);
			Writer.WriteString(CommandDescription);
			Writer.WriteString(StatusDescription);
			Writer.WriteBool(bCanExecuteRemotely);
			Writer.WriteBool(bCanExecuteRemotelyWithSNDBS);
			Writer.WriteBool(bCanExecuteRemotelyWithXGE);
			Writer.WriteBool(bCanExecuteInUBA);
			Writer.WriteBool(bIsGCCCompiler);
			Writer.WriteBool(bShouldOutputStatusDescription);
			Writer.WriteBool(bProducesImportLibrary);
			Writer.WriteSortedSet(PrerequisiteItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteSortedSet(ProducedItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteSortedSet(DeleteItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteFileItem(DependencyListFile);
			Writer.WriteBool(bUseActionHistory);
			Writer.WriteBool(bIsHighPriority);
			Writer.WriteDouble(Weight);
		}

		/// <summary>
		/// Writes an action to a json file
		/// </summary>
		/// <param name="Object">The object to parse</param>
		public static Action ImportJson(JsonObject Object)
		{
			Action Action = new Action(Object.GetEnumField<ActionType>("Type"));

			ArtifactMode ArtifactMode;
			if (Object.TryGetEnumField("ArtifactMode", out ArtifactMode))
			{
				Action.ArtifactMode = ArtifactMode;
			}

			string? WorkingDirectory;
			if (Object.TryGetStringField("WorkingDirectory", out WorkingDirectory))
			{
				Action.WorkingDirectory = new DirectoryReference(WorkingDirectory);
			}

			string? CommandPath;
			if (Object.TryGetStringField("CommandPath", out CommandPath))
			{
				Action.CommandPath = new FileReference(CommandPath);
			}

			string? CommandArguments;
			if (Object.TryGetStringField("CommandArguments", out CommandArguments))
			{
				Action.CommandArguments = CommandArguments;
			}

			string? CommandVersion;
			if (Object.TryGetStringField("CommandVersion", out CommandVersion))
			{
				Action.CommandVersion = CommandVersion;
			}

			string? CommandDescription;
			if (Object.TryGetStringField("CommandDescription", out CommandDescription))
			{
				Action.CommandDescription = CommandDescription;
			}

			string? StatusDescription;
			if (Object.TryGetStringField("StatusDescription", out StatusDescription))
			{
				Action.StatusDescription = StatusDescription;
			}

			bool bCanExecuteRemotely;
			if (Object.TryGetBoolField("bCanExecuteRemotely", out bCanExecuteRemotely))
			{
				Action.bCanExecuteRemotely = bCanExecuteRemotely;
			}

			bool bCanExecuteRemotelyWithSNDBS;
			if (Object.TryGetBoolField("bCanExecuteRemotelyWithSNDBS", out bCanExecuteRemotelyWithSNDBS))
			{
				Action.bCanExecuteRemotelyWithSNDBS = bCanExecuteRemotelyWithSNDBS;
			}

			bool bCanExecuteRemotelyWithXGE;
			if (Object.TryGetBoolField("bCanExecuteRemotelyWithXGE", out bCanExecuteRemotelyWithXGE))
			{
				Action.bCanExecuteRemotelyWithXGE = bCanExecuteRemotelyWithXGE;
			}

			bool bCanExecuteInUBA;
			if (Object.TryGetBoolField("bCanExecuteInUBA", out bCanExecuteInUBA))
			{
				Action.bCanExecuteInUBA = bCanExecuteInUBA;
			}

			bool bIsGCCCompiler;
			if (Object.TryGetBoolField("bIsGCCCompiler", out bIsGCCCompiler))
			{
				Action.bIsGCCCompiler = bIsGCCCompiler;
			}

			bool bShouldOutputStatusDescription;
			if (Object.TryGetBoolField("bShouldOutputStatusDescription", out bShouldOutputStatusDescription))
			{
				Action.bShouldOutputStatusDescription = bShouldOutputStatusDescription;
			}

			bool bProducesImportLibrary;
			if (Object.TryGetBoolField("bProducesImportLibrary", out bProducesImportLibrary))
			{
				Action.bProducesImportLibrary = bProducesImportLibrary;
			}

			string[]? PrerequisiteItems;
			if (Object.TryGetStringArrayField("PrerequisiteItems", out PrerequisiteItems))
			{
				Action.PrerequisiteItems.UnionWith(PrerequisiteItems.Select(x => FileItem.GetItemByPath(x)));
			}

			string[]? ProducedItems;
			if (Object.TryGetStringArrayField("ProducedItems", out ProducedItems))
			{
				Action.ProducedItems.UnionWith(ProducedItems.Select(x => FileItem.GetItemByPath(x)));
			}

			string[]? DeleteItems;
			if (Object.TryGetStringArrayField("DeleteItems", out DeleteItems))
			{
				Action.DeleteItems.UnionWith(DeleteItems.Select(x => FileItem.GetItemByPath(x)));
			}

			string? DependencyListFile;
			if (Object.TryGetStringField("DependencyListFile", out DependencyListFile))
			{
				Action.DependencyListFile = FileItem.GetItemByPath(DependencyListFile);
			}

			double Weight;
			if (Object.TryGetDoubleField("Weight", out Weight))
			{
				Action.Weight = Weight;
			}

			return Action;
		}

		public override string ToString()
		{
			string ReturnString = "";
			if (CommandPath != null)
			{
				ReturnString += CommandPath + " - ";
			}
			if (CommandArguments != null)
			{
				ReturnString += CommandArguments;
			}
			return ReturnString;
		}
	}

	/// <summary>
	/// Interface that is used when -SingleFile=xx is used on the cmd line to make specific files compile
	/// Actions that has this interface can be used to create real actions that can compile the specific files
	/// </summary>
	interface ISpecificFileAction
	{
		/// <summary>
		/// The directory this action can create actions for
		/// </summary>
		DirectoryReference RootDirectory { get; }

		/// <summary>
		/// Creates an action for a specific file. It can return null if for example file extension is not handled
		/// </summary>
		IExternalAction? CreateAction(FileItem SourceFile, ILogger Logger);
	}

	/// <summary>
	/// Extension methods for action classes
	/// </summary>
	static class ActionExtensions
	{
		/// <summary>
		/// Writes an action to a json file
		/// </summary>
		/// <param name="Action">The action to write</param>
		/// <param name="LinkedActionToId">Map of action to unique id</param>
		/// <param name="Writer">Writer to receive the output</param>
		public static void ExportJson(this LinkedAction Action, Dictionary<LinkedAction, int> LinkedActionToId, JsonWriter Writer)
		{
			Writer.WriteValue("Id", LinkedActionToId[Action]);
			Writer.WriteEnumValue("Type", Action.ActionType);
			Writer.WriteEnumValue("ArtifactMode", Action.ArtifactMode);
			Writer.WriteValue("WorkingDirectory", Action.WorkingDirectory.FullName);
			Writer.WriteValue("CommandPath", Action.CommandPath.FullName);
			Writer.WriteValue("CommandArguments", Action.CommandArguments);
			Writer.WriteValue("CommandVersion", Action.CommandVersion);
			Writer.WriteValue("CommandDescription", Action.CommandDescription);
			Writer.WriteValue("StatusDescription", Action.StatusDescription);
			Writer.WriteValue("bCanExecuteRemotely", Action.bCanExecuteRemotely);
			Writer.WriteValue("bCanExecuteRemotelyWithSNDBS", Action.bCanExecuteRemotelyWithSNDBS);
			Writer.WriteValue("bCanExecuteRemotelyWithXGE", Action.bCanExecuteRemotelyWithXGE);
			Writer.WriteValue("bCanExecuteInUBA", Action.bCanExecuteInUBA);
			Writer.WriteValue("bIsGCCCompiler", Action.bIsGCCCompiler);
			Writer.WriteValue("bShouldOutputStatusDescription", Action.bShouldOutputStatusDescription);
			Writer.WriteValue("bProducesImportLibrary", Action.bProducesImportLibrary);
			Writer.WriteValue("Weight", Action.Weight);

			Writer.WriteArrayStart("PrerequisiteActions");
			foreach (LinkedAction PrerequisiteAction in Action.PrerequisiteActions)
			{
				Writer.WriteValue(LinkedActionToId[PrerequisiteAction]);
			}
			Writer.WriteArrayEnd();

			Writer.WriteArrayStart("ProducedItems");
			foreach (FileItem ProducedItem in Action.ProducedItems)
			{
				Writer.WriteValue(ProducedItem.AbsolutePath);
			}
			Writer.WriteArrayEnd();

			Writer.WriteArrayStart("DeleteItems");
			foreach (FileItem DeleteItem in Action.DeleteItems)
			{
				Writer.WriteValue(DeleteItem.AbsolutePath);
			}
			Writer.WriteArrayEnd();

			if (Action.DependencyListFile != null)
			{
				Writer.WriteValue("DependencyListFile", Action.DependencyListFile.AbsolutePath);
			}
		}

		/// <summary>
		/// Determine if this Action ignores output conficts.
		/// </summary>
		/// <param name="Action"> The Action to check</param>
		/// <returns>True if conflicts are ignored, else false.</returns>
		public static bool IgnoreConflicts(this IExternalAction Action)
		{
			return Action.ActionType == ActionType.WriteMetadata ||
				Action.ActionType == ActionType.CreateAppBundle;
		}
	}

	/// <summary>
	/// Default serializer for <see cref="Action"/> instances
	/// </summary>
	class DefaultActionSerializer : ActionSerializerBase<Action>
	{
		/// <inheritdoc/>
		public override Action Read(BinaryArchiveReader Reader)
		{
			return new Action(Reader);
		}

		/// <inheritdoc/>
		public override void Write(BinaryArchiveWriter Writer, Action Action)
		{
			Action.Write(Writer);
		}
	}

	/// <summary>
	/// Information about an action queued to be executed
	/// </summary>
	[DebuggerDisplay("{StatusDescription}")]
	class LinkedAction : IExternalAction
	{
		/// <summary>
		/// The inner action instance
		/// </summary>
		public IExternalAction Inner;

		/// <summary>
		/// A target that this action contributes to
		/// </summary>
		public TargetDescriptor? Target;

		/// <summary>
		/// Set of other actions that this action depends on. This set is built when the action graph is linked.
		/// </summary>
		public HashSet<LinkedAction> PrerequisiteActions = null!;

		/// <summary>
		/// Total number of actions depending on this one.
		/// </summary>
		public int NumTotalDependentActions = 0;

		/// <summary>
		/// Additional field used for sorting that can be used to control sorting of link actions from outside
		/// </summary>
		public int SortIndex = 0;

		/// <summary>
		/// True if this action should be scheduled early. Some actions can take a long time (pch) and need to start very early
		/// If set to 1 it will propagate high priority to all its prerequisite tasks. (Note it is an int to be able to do interlocked operations)
		/// </summary>
		public int IsHighPriority;

		/// <summary>
		/// If set, will be output whenever the group differs to the last executed action. Set when executing multiple targets at once.
		/// </summary>
		public SortedSet<string> GroupNames = new SortedSet<string>();

		#region Wrapper implementation of IAction

		public ActionType ActionType => Inner.ActionType;
		public ArtifactMode ArtifactMode => Inner.ArtifactMode;
		public IEnumerable<FileItem> PrerequisiteItems => Inner.PrerequisiteItems;
		public IEnumerable<FileItem> ProducedItems => Inner.ProducedItems;
		public IEnumerable<FileItem> DeleteItems => Inner.DeleteItems;
		public FileItem? DependencyListFile => Inner.DependencyListFile;
		public DirectoryReference WorkingDirectory => Inner.WorkingDirectory;
		public FileReference CommandPath => Inner.CommandPath;
		public string CommandArguments => Inner.CommandArguments;
		public string CommandVersion => Inner.CommandVersion;
		public string CommandDescription => Inner.CommandDescription;
		public string StatusDescription => Inner.StatusDescription;
		public bool bCanExecuteRemotely => Inner.bCanExecuteRemotely;
		public bool bCanExecuteRemotelyWithSNDBS => Inner.bCanExecuteRemotelyWithSNDBS;
		public bool bCanExecuteRemotelyWithXGE => Inner.bCanExecuteRemotelyWithXGE;
		public bool bCanExecuteInUBA => Inner.bCanExecuteInUBA;
		public bool bIsGCCCompiler => Inner.bIsGCCCompiler;
		public bool bShouldOutputStatusDescription => Inner.bShouldOutputStatusDescription;
		public bool bProducesImportLibrary => Inner.bProducesImportLibrary;
		public bool bUseActionHistory => Inner.bUseActionHistory;
		public bool bIsHighPriority => IsHighPriority != 0;
		public double Weight => Inner.Weight;

		#endregion

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The inner action instance</param>
		/// <param name="Target"></param>
		public LinkedAction(IExternalAction Inner, TargetDescriptor? Target)
		{
			this.Inner = Inner;
			this.Target = Target;
			IsHighPriority = Inner.bIsHighPriority ? 1 : 0;
		}

		/// <summary>
		/// Increment the number of dependents, recursively
		/// </summary>
		/// <param name="VisitedActions">Set of visited actions</param>
		/// <param name="bIsHighPriority">Propagate high priority to this and prerequisite actions</param>
		public void IncrementDependentCount(HashSet<LinkedAction> VisitedActions, bool bIsHighPriority)
		{
			if (VisitedActions.Add(this))
			{
				if (bIsHighPriority)
				{
					Interlocked.Exchange(ref IsHighPriority, 1);
				}

				Interlocked.Increment(ref NumTotalDependentActions);
				foreach (LinkedAction PrerequisiteAction in PrerequisiteActions)
				{
					PrerequisiteAction.IncrementDependentCount(VisitedActions, bIsHighPriority);
				}
			}
		}

		/// <summary>
		/// Compares two actions based on total number of sort index, priority and dependent items, descending.
		/// </summary>
		/// <param name="A">Action to compare</param>
		/// <param name="B">Action to compare</param>
		public static int Compare(LinkedAction A, LinkedAction B)
		{
			if (A.SortIndex != B.SortIndex)
			{
				return Math.Sign(A.SortIndex - B.SortIndex);
			}

			if (A.IsHighPriority != B.IsHighPriority)
			{
				return B.IsHighPriority - A.IsHighPriority;
			}

			// Primary sort criteria is total number of dependent files, up to max depth.
			if (B.NumTotalDependentActions != A.NumTotalDependentActions)
			{
				return Math.Sign(B.NumTotalDependentActions - A.NumTotalDependentActions);
			}

			// Secondary sort criteria is directory name of first produced item.
			// This will sort actions from same module closer to each other
			// Also sort in alphabetic order since when building with unity files it is likely that the last unity file is the fastest to compile
			// and we want longer actions to run earlier
			int Result = String.Compare(A.ProducedItems.FirstOrDefault()?.FullName, B.ProducedItems.FirstOrDefault()?.FullName, StringComparison.Ordinal);
			if (Result != 0)
			{
				return Math.Sign(Result);
			}

			// Third sort criteria is number of pre-requisites.
			return Math.Sign(B.PrerequisiteItems.Count() - A.PrerequisiteItems.Count());
		}
	}
}
