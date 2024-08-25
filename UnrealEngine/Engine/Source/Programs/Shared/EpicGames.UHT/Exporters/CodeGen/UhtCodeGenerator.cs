// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	[UnrealHeaderTool]
	class UhtCodeGenerator
	{
		[UhtExporter(Name = "CodeGen", Description = "Standard UnrealEngine code generation", Options = UhtExporterOptions.Default,
			CppFilters = new string[] { "*.generated.cpp", "*.generated.*.cpp", "*.gen.cpp", "*.gen.*.cpp" },
			HeaderFilters = new string[] { "*.generated.h" })]
		public static void CodeGenerator(IUhtExportFactory factory)
		{
			UhtCodeGenerator generator = new(factory);
			generator.Generate();
		}

		public struct PackageInfo
		{
			public string StrippedName { get; set; }
			public string Api { get; set; }
		}
		public PackageInfo[] PackageInfos { get; set; }

		public struct HeaderInfo
		{
			public Task? Task { get; set; }
			public string IncludePath { get; set; }
			public string FileId { get; set; }
			public uint BodyHash { get; set; }
			public bool NeedsPushModelHeaders { get; set; }
			public bool NeedsFastArrayHeaders { get; set; }
			public bool NeedsVerseHeaders { get; set; }
		}
		public HeaderInfo[] HeaderInfos { get; set; }

		public struct ObjectInfo
		{
			public string RegisteredSingletonName { get; set; }
			public string UnregisteredSingletonName { get; set; }
			public string RegsiteredExternalDecl { get; set; }
			public string UnregisteredExternalDecl { get; set; }
			public UhtClass? NativeInterface { get; set; }
			public UhtProperty? FastArrayProperty { get; set; }
			public uint Hash { get; set; }
		}
		public ObjectInfo[] ObjectInfos { get; set; }

		public readonly IUhtExportFactory Factory;
		public UhtSession Session => Factory.Session;
		public UhtScriptStruct? FastArraySerializer { get; set; } = null;

		private UhtCodeGenerator(IUhtExportFactory factory)
		{
			Factory = factory;
			HeaderInfos = new HeaderInfo[Factory.Session.HeaderFileTypeCount];
			ObjectInfos = new ObjectInfo[Factory.Session.ObjectTypeCount];
			PackageInfos = new PackageInfo[Factory.Session.PackageTypeCount];
		}

		private void Generate()
		{
			List<Task?> prereqs = new();

			FastArraySerializer = Session.FindType(null, UhtFindOptions.SourceName | UhtFindOptions.ScriptStruct, "FFastArraySerializer") as UhtScriptStruct;

			// Perform some startup initialization to compute things we need over and over again
			if (Session.GoWide)
			{
				Parallel.ForEach(Factory.Session.Packages, package =>
				{
					InitPackageInfo(package);
				});
			}
			else
			{
				foreach (UhtPackage package in Factory.Session.Packages)
				{
					InitPackageInfo(package);
				}
			}

			// Generate the files for the header files
			foreach (UhtHeaderFile headerFile in Session.SortedHeaderFiles)
			{
				if (headerFile.ShouldExport)
				{
					UhtPackage package = headerFile.Package;
					UHTManifest.Module module = package.Module;

					prereqs.Clear();
					foreach (UhtHeaderFile referenced in headerFile.ReferencedHeadersNoLock)
					{
						if (headerFile != referenced)
						{
							prereqs.Add(HeaderInfos[referenced.HeaderFileTypeIndex].Task);
						}
					}

					HeaderInfos[headerFile.HeaderFileTypeIndex].Task = Factory.CreateTask(prereqs,
						(IUhtExportFactory factory) =>
						{
							new UhtHeaderCodeGeneratorHFile(this, package, headerFile).Generate(factory);
							new UhtHeaderCodeGeneratorCppFile(this, package, headerFile).Generate(factory);
						});
				}
			}

			// Generate the files for the packages
			List<Task?> generatedPackages = new(Session.PackageTypeCount);
			foreach (UhtPackage package in Session.Packages)
			{
				UHTManifest.Module module = package.Module;

				bool writeHeader = false;
				prereqs.Clear();
				foreach (UhtType packageChild in package.Children)
				{
					if (packageChild is UhtHeaderFile headerFile)
					{
						prereqs.Add(HeaderInfos[headerFile.HeaderFileTypeIndex].Task);
						if (!writeHeader)
						{
							foreach (UhtType type in headerFile.Children)
							{
								if (type is UhtClass classObj)
								{
									if (classObj.ClassType != UhtClassType.NativeInterface &&
										classObj.ClassFlags.HasExactFlags(EClassFlags.Native | EClassFlags.Intrinsic, EClassFlags.Native) &&
										!classObj.ClassExportFlags.HasAllFlags(UhtClassExportFlags.NoExport))
									{
										writeHeader = true;
										break;
									}
								}
							}
						}
					}
				}

				generatedPackages.Add(Factory.CreateTask(prereqs,
					(IUhtExportFactory factory) =>
					{
						List<UhtHeaderFile> packageSortedHeaders = GetSortedHeaderFiles(package);
						if (writeHeader)
						{
							new UhtPackageCodeGeneratorHFile(this, package).Generate(factory, packageSortedHeaders);
						}
						new UhtPackageCodeGeneratorCppFile(this, package).Generate(factory, packageSortedHeaders);
					}));
			}

			// Wait for all the packages to complete
			List<Task> packageTasks = new(Session.PackageTypeCount);
			foreach (Task? output in generatedPackages)
			{
				if (output != null)
				{
					packageTasks.Add(output);
				}
			}
			Task.WaitAll(packageTasks.ToArray());
		}

		#region Utility functions
		/// <summary>
		/// Return the singleton name for an object
		/// </summary>
		/// <param name="obj">The object in question.</param>
		/// <param name="registered">If true, return the registered singleton name.  Otherwise return the unregistered.</param>
		/// <returns>Singleton name or "nullptr" if Object is null</returns>
		public string GetSingletonName(UhtObject? obj, bool registered)
		{
			if (obj == null)
			{
				return "nullptr";
			}
			return registered ? ObjectInfos[obj.ObjectTypeIndex].RegisteredSingletonName : ObjectInfos[obj.ObjectTypeIndex].UnregisteredSingletonName;
		}

		/// <summary>
		/// Return the external declaration for an object
		/// </summary>
		/// <param name="obj">The object in question.</param>
		/// <param name="registered">If true, return the registered external declaration.  Otherwise return the unregistered.</param>
		/// <returns>External declaration</returns>
		public string GetExternalDecl(UhtObject obj, bool registered)
		{
			return GetExternalDecl(obj.ObjectTypeIndex, registered);
		}

		/// <summary>
		/// Return the external declaration for an object
		/// </summary>
		/// <param name="objectIndex">The object in question.</param>
		/// <param name="registered">If true, return the registered external declaration.  Otherwise return the unregistered.</param>
		/// <returns>External declaration</returns>
		public string GetExternalDecl(int objectIndex, bool registered)
		{
			return registered ? ObjectInfos[objectIndex].RegsiteredExternalDecl : ObjectInfos[objectIndex].UnregisteredExternalDecl;
		}
		#endregion

		#region Information initialization
		private void InitPackageInfo(UhtPackage package)
		{
			StringBuilder builder = new();

			ref PackageInfo packageInfo = ref PackageInfos[package.PackageTypeIndex];
			packageInfo.StrippedName = package.SourceName.Replace('/', '_');
			packageInfo.Api = $"{package.ShortName.ToString().ToUpper()}_API ";

			// Construct the names used commonly during export
			ref ObjectInfo objectInfo = ref ObjectInfos[package.ObjectTypeIndex];
			builder.Append("Z_Construct_UPackage_");
			builder.Append(packageInfo.StrippedName);
			objectInfo.UnregisteredSingletonName = objectInfo.RegisteredSingletonName = builder.ToString();
			objectInfo.UnregisteredExternalDecl = objectInfo.RegsiteredExternalDecl = $"\tUPackage* {objectInfo.RegisteredSingletonName}();\r\n";

			foreach (UhtType packageChild in package.Children)
			{
				if (packageChild is UhtHeaderFile headerFile)
				{
					InitHeaderInfo(builder, package, ref packageInfo, headerFile);
				}
			}
		}

		private void InitHeaderInfo(StringBuilder builder, UhtPackage package, ref PackageInfo packageInfo, UhtHeaderFile headerFile)
		{
			ref HeaderInfo headerInfo = ref HeaderInfos[headerFile.HeaderFileTypeIndex];

			headerInfo.IncludePath = Path.GetRelativePath(package.Module.IncludeBase, headerFile.FilePath).Replace('\\', '/');

			// Convert the file path to a C identifier
			string filePath = headerFile.FilePath;
			bool isRelative = !Path.IsPathRooted(filePath);
			if (!isRelative && Session.EngineDirectory != null)
			{
				string? directory = Path.GetDirectoryName(Session.EngineDirectory);
				if (!String.IsNullOrEmpty(directory))
				{
					filePath = Path.GetRelativePath(directory, filePath);
					isRelative = !Path.IsPathRooted(filePath);
				}
			}
			if (!isRelative && Session.ProjectDirectory != null)
			{
				string? directory = Path.GetDirectoryName(Session.ProjectDirectory);
				if (!String.IsNullOrEmpty(directory))
				{
					filePath = Path.GetRelativePath(directory, filePath);
					isRelative = !Path.IsPathRooted(filePath);
				}
			}
			filePath = filePath.Replace('\\', '/');
			if (isRelative)
			{
				while (filePath.StartsWith("../", StringComparison.Ordinal))
				{
					filePath = filePath[3..];
				}
			}

			char[] outFilePath = new char[filePath.Length + 4];
			outFilePath[0] = 'F';
			outFilePath[1] = 'I';
			outFilePath[2] = 'D';
			outFilePath[3] = '_';
			for (int index = 0; index < filePath.Length; ++index)
			{
				outFilePath[index + 4] = UhtFCString.IsAlnum(filePath[index]) ? filePath[index] : '_';
			}
			headerInfo.FileId = new string(outFilePath);

			foreach (UhtType headerFileChild in headerFile.Children)
			{
				if (headerFileChild is UhtObject obj)
				{
					InitObjectInfo(builder, package, ref packageInfo, ref headerInfo, obj);
				}
			}
		}

		private void InitObjectInfo(StringBuilder builder, UhtPackage package, ref PackageInfo packageInfo, ref HeaderInfo headerInfo, UhtObject obj)
		{
			ref ObjectInfo objectInfo = ref ObjectInfos[obj.ObjectTypeIndex];

			builder.Clear();

			// Construct the names used commonly during export
			bool isNonIntrinsicClass = false;
			builder.Append("Z_Construct_U").Append(obj.EngineClassName).AppendOuterNames(obj);

			string engineClassName = obj.EngineClassName;
			if (obj is UhtClass classObj)
			{
				if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
				{
					isNonIntrinsicClass = true;
				}
				if (classObj.ClassExportFlags.HasExactFlags(UhtClassExportFlags.HasReplciatedProperties, UhtClassExportFlags.SelfHasReplicatedProperties))
				{
					headerInfo.NeedsPushModelHeaders = true;
				}
				if (classObj.ClassType == UhtClassType.NativeInterface)
				{
					if (classObj.AlternateObject != null)
					{
						ObjectInfos[classObj.AlternateObject.ObjectTypeIndex].NativeInterface = classObj;
					}
				}
				headerInfo.NeedsVerseHeaders = classObj.Children.Any(x => x is UhtVerseValueProperty);
			}
			else if (obj is UhtScriptStruct scriptStruct)
			{
				// Check to see if we are a FastArraySerializer and should try to deduce the FastArraySerializerItemType
				// To fulfill that requirement the struct should be derived from FFastArraySerializer and have a single replicated TArrayProperty
				if (scriptStruct.IsChildOf(FastArraySerializer))
				{
					foreach (UhtType child in scriptStruct.Children)
					{
						if (child is UhtProperty property)
						{
							if (!property.PropertyFlags.HasAnyFlags(EPropertyFlags.RepSkip) && property is UhtArrayProperty)
							{
								if (objectInfo.FastArrayProperty != null)
								{
									objectInfo.FastArrayProperty = null;
									break;
								}
								objectInfo.FastArrayProperty = property;
							}
						}
					}
					if (objectInfo.FastArrayProperty != null)
					{
						headerInfo.NeedsFastArrayHeaders = true;
					}
				}
				headerInfo.NeedsVerseHeaders = scriptStruct.Children.Any(x => x is UhtVerseValueProperty);
			}
			else if (obj is UhtFunction)
			{
				// The method for EngineClassName returns type specific where in this case we need just the simple return type
				engineClassName = "Function";
			}

			if (isNonIntrinsicClass)
			{
				objectInfo.RegisteredSingletonName = builder.ToString();
				builder.Append("_NoRegister");
				objectInfo.UnregisteredSingletonName = builder.ToString();

				objectInfo.UnregisteredExternalDecl = $"\t{packageInfo.Api}U{engineClassName}* {objectInfo.UnregisteredSingletonName}();\r\n";
				objectInfo.RegsiteredExternalDecl = $"\t{packageInfo.Api}U{engineClassName}* {objectInfo.RegisteredSingletonName}();\r\n";
			}
			else
			{
				objectInfo.UnregisteredSingletonName = objectInfo.RegisteredSingletonName = builder.ToString();
				objectInfo.UnregisteredExternalDecl = objectInfo.RegsiteredExternalDecl = $"\t{packageInfo.Api}U{engineClassName}* {objectInfo.RegisteredSingletonName}();\r\n";
			}

			// Init the children
			foreach (UhtType child in obj.Children)
			{
				if (child is UhtObject childObject)
				{
					InitObjectInfo(builder, package, ref packageInfo, ref headerInfo, childObject);
				}
			}
		}
		#endregion

		#region Utility functions
		/// <summary>
		/// Return a package's sorted header file list of all header files that or referenced or have declarations.
		/// </summary>
		/// <param name="package">The package in question</param>
		/// <returns>Sorted list of the header files</returns>
		private static List<UhtHeaderFile> GetSortedHeaderFiles(UhtPackage package)
		{
			List<UhtHeaderFile> sortedHeaders = new(package.Children.Count);
			foreach (UhtType packageChild in package.Children)
			{
				if (packageChild is UhtHeaderFile headerFile)
				{
					if (headerFile.ShouldExport)
					{
						sortedHeaders.Add(headerFile);
					}
				}
			}
			sortedHeaders.Sort((lhs, rhs) => { return StringComparerUE.OrdinalIgnoreCase.Compare(lhs.FilePath, rhs.FilePath); });
			return sortedHeaders;
		}
		#endregion
	}
}
