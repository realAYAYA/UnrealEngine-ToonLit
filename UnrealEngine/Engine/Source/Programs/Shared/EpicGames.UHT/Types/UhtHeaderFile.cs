// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Represents the different directories where headers can appear
	/// </summary>
	public enum UhtHeaderFileType
	{

		/// <summary>
		/// Classes folder
		/// </summary>
		Classes,

		/// <summary>
		/// Public folder
		/// </summary>
		Public,

		/// <summary>
		/// Internal folder
		/// </summary>
		Internal,

		/// <summary>
		/// Private folder
		/// </summary>
		Private,
	}

	/// <summary>
	/// Series of flags not part of the engine's class flags that affect code generation or verification
	/// </summary>
	[Flags]
	public enum UhtHeaderFileExportFlags : int
	{
		/// <summary>
		/// No export flags
		/// </summary>
		None = 0,

		/// <summary>
		/// This header is being included by another header
		/// </summary>
		Referenced = 0x00000001,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtHeaderFileExportFlagsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtHeaderFileExportFlags inFlags, UhtHeaderFileExportFlags testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtHeaderFileExportFlags inFlags, UhtHeaderFileExportFlags testFlags)
		{
			return (inFlags & testFlags) == testFlags;
		}

		/// <summary>
		/// Test to see if a specific set of flags have a specific value.
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <param name="matchFlags">Expected value of the tested flags</param>
		/// <returns>True if the given flags have a specific value.</returns>
		public static bool HasExactFlags(this UhtHeaderFileExportFlags inFlags, UhtHeaderFileExportFlags testFlags, UhtHeaderFileExportFlags matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// Represents a header file.  Unlike the engine, UhtHeader files will appear as a child 
	/// of a UhtPackage and the outer of all global types found in that header.
	/// </summary>
	public class UhtHeaderFile : UhtType
	{
		private readonly UhtSimpleMessageSite _messageSite;
		private readonly UhtSourceFile _sourceFile;
		private readonly List<UhtHeaderFile> _referencedHeaders = new();

		/// <summary>
		/// Contents of the header
		/// </summary>
		[JsonIgnore]
		public StringView Data => _sourceFile.Data;

		/// <summary>
		/// Path of the header
		/// </summary>
		[JsonIgnore]
		public string FilePath => _sourceFile.FilePath;

		/// <summary>
		/// File name without the extension
		/// </summary>
		[JsonIgnore]
		public string FileNameWithoutExtension { get; }

		/// <summary>
		/// Required name for the generated.h file name.  Used to validate parsed code
		/// </summary>
		[JsonIgnore]
		public string GeneratedHeaderFileName { get; }

		/// <summary>
		/// True if this header is NoExportTypes.h
		/// </summary>
		[JsonIgnore]
		public bool IsNoExportTypes { get; }

		/// <summary>
		/// The file path of the header relative to the module location
		/// </summary>
		public string ModuleRelativeFilePath { get; set; } = String.Empty;

		/// <summary>
		/// Include file path added as meta data to the types
		/// </summary>
		public string IncludeFilePath { get; set; } = String.Empty;

		/// <summary>
		/// Location where the header file was found
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtHeaderFileType HeaderFileType { get; set; } = UhtHeaderFileType.Private;

		/// <summary>
		/// Unique index of the header file
		/// </summary>
		[JsonIgnore]
		public int HeaderFileTypeIndex { get; }

		/// <summary>
		/// UHT flags for the header
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtHeaderFileExportFlags HeaderFileExportFlags { get; set; } = UhtHeaderFileExportFlags.None;

		/// <summary>
		/// If true, the header file should be exported
		/// </summary>
		[JsonIgnore]
		public bool ShouldExport => HeaderFileExportFlags.HasAnyFlags(UhtHeaderFileExportFlags.Referenced) || Children.Count > 0;

		/// <summary>
		/// Resource collector for the header file
		/// </summary>
		[JsonIgnore]
		public UhtReferenceCollector References { get; } = new UhtReferenceCollector();

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtPackage Package
		{
			get
			{
				if (Outer == null)
				{
					throw new UhtIceException("Attempt to fetch header file package but it has no outer");
				}
				return (UhtPackage)Outer;
			}
		}

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtHeaderFile HeaderFile => this;

		/// <inheritdoc/>
		[JsonIgnore]
		public override UhtEngineType EngineType => UhtEngineType.Header;

		/// <inheritdoc/>
		public override string EngineClassName => "UhtHeaderFile";

		/// <summary>
		/// Collection of headers directly included by this header
		/// </summary>
		[JsonIgnore]
		public List<UhtHeaderFile> IncludedHeaders { get; } = new List<UhtHeaderFile>();

		#region IUHTMessageSite implementation

		/// <inheritdoc/>
		[JsonIgnore]
		public override IUhtMessageSession MessageSession => _messageSite.MessageSession;

		/// <inheritdoc/>
		[JsonIgnore]
		public override IUhtMessageSource? MessageSource => _messageSite.MessageSource;
		#endregion

		/// <summary>
		/// Construct a new header file
		/// </summary>
		/// <param name="package">Owning package</param>
		/// <param name="path">Path to the header file</param>
		public UhtHeaderFile(UhtPackage package, string path) : base(package, 1)
		{
			HeaderFileTypeIndex = Session.GetNextHeaderFileTypeIndex();
			_messageSite = new UhtSimpleMessageSite(Session);
			_sourceFile = new UhtSourceFile(Session, path);
			_messageSite.MessageSource = _sourceFile;
			FileNameWithoutExtension = System.IO.Path.GetFileNameWithoutExtension(_sourceFile.FilePath);
			GeneratedHeaderFileName = FileNameWithoutExtension + ".generated.h";
			SourceName = System.IO.Path.GetFileName(_sourceFile.FilePath);
			IsNoExportTypes = String.Equals(_sourceFile.FileName, "NoExportTypes", StringComparison.OrdinalIgnoreCase);
		}

		/// <summary>
		/// Read the contents of the header
		/// </summary>
		public void Read()
		{
			_sourceFile.Read();
		}

		/// <summary>
		/// Add a reference to the given header
		/// </summary>
		/// <param name="id">Path of the header</param>
		/// <param name="isIncludedFile">True if this is a directly included file</param>
		public void AddReferencedHeader(string id, bool isIncludedFile)
		{
			UhtHeaderFile? headerFile = Session.FindHeaderFile(Path.GetFileName(id));
			if (headerFile != null)
			{
				AddReferencedHeader(headerFile, isIncludedFile);
			}
		}

		/// <summary>
		/// Add a reference to the header that defines the given type
		/// </summary>
		/// <param name="type">Type in question</param>
		public void AddReferencedHeader(UhtType type)
		{
			AddReferencedHeader(type.HeaderFile, false);
		}

		/// <summary>
		/// Add a reference to the given header file
		/// </summary>
		/// <param name="headerFile">Header file in question</param>
		/// <param name="isIncludedFile">True if this is a directly included file</param>
		public void AddReferencedHeader(UhtHeaderFile headerFile, bool isIncludedFile)
		{

			// Ignore direct references to myself
			if (!isIncludedFile && headerFile == this)
			{
				return;
			}

			lock (_referencedHeaders)
			{
				// Check for a duplicate
				foreach (UhtHeaderFile reference in _referencedHeaders)
				{
					if (reference == headerFile)
					{
						return;
					}
				}

				// There is questionable compatibility hack where a source file will always be exported
				// regardless of having types when it is being included by the SAME package.
				if (headerFile.Package == Package)
				{
					headerFile.HeaderFileExportFlags |= UhtHeaderFileExportFlags.Referenced;
				}
				_referencedHeaders.Add(headerFile);
				if (isIncludedFile)
				{
					IncludedHeaders.Add(headerFile);
				}
			}
		}

		/// <summary>
		/// Return an enumerator without locking.  This method can only be utilized AFTER all header parsing is complete. 
		/// </summary>
		[JsonIgnore]
		public IEnumerable<UhtHeaderFile> ReferencedHeadersNoLock => _referencedHeaders;

		/// <summary>
		/// Return an enumerator of all current referenced headers under a lock.  This should be used during parsing.
		/// </summary>
		[JsonIgnore]
		public IEnumerable<UhtHeaderFile> ReferencedHeadersLocked
		{
			get
			{
				lock (_referencedHeaders)
				{
					foreach (UhtHeaderFile reference in _referencedHeaders)
					{
						yield return reference;
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void AppendPathName(StringBuilder builder, UhtType? stopOuter = null)
		{
			// Headers do not contribute to path names
			if (this != stopOuter && Outer != null)
			{
				Outer.AppendPathName(builder, stopOuter);
			}
		}

		/// <inheritdoc/>
		protected override UhtValidationOptions Validate(UhtValidationOptions options)
		{
			options = base.Validate(options | UhtValidationOptions.Shadowing);

			Dictionary<int, UhtFunction> usedRPCIds = new();
			Dictionary<int, UhtFunction> rpcsNeedingHookup = new();
			foreach (UhtType type in Children)
			{
				if (type is UhtClass classObj)
				{
					foreach (UhtType child in classObj.Children)
					{
						if (child is UhtFunction function)
						{
							if (function.FunctionType != UhtFunctionType.Function || !function.FunctionFlags.HasAnyFlags(EFunctionFlags.Net))
							{
								continue;
							}

							if (function.RPCId > 0)
							{
								if (usedRPCIds.TryGetValue(function.RPCId, out UhtFunction? existingFunc))
								{
									function.LogError($"Function {existingFunc.SourceName} already uses identifier {function.RPCId}");
								}
								else
								{
									usedRPCIds.Add(function.RPCId, function);
								}

								if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetResponse))
								{
									// Look for another function expecting this response
									rpcsNeedingHookup.Remove(function.RPCId);
								}
							}

							if (function.RPCResponseId > 0 && function.EndpointName != "JSBridge")
							{
								// Look for an existing response function, if not found then add to the list of ids awaiting hookup
								if (!usedRPCIds.ContainsKey(function.RPCResponseId))
								{
									rpcsNeedingHookup.Add(function.RPCResponseId, function);
								}
							}
						}
					}
				}
			}

			if (rpcsNeedingHookup.Count > 0)
			{
				foreach (KeyValuePair<int, UhtFunction> kvp in rpcsNeedingHookup)
				{
					kvp.Value.LogError($"Request function '{kvp.Value.SourceName}' is missing a response function with the id of '{kvp.Key}'");
				}
			}
			return options;
		}
	}
}
