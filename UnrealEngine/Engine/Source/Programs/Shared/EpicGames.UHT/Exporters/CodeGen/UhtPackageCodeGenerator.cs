// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Text;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	class UhtPackageCodeGenerator
	{
		public static string HeaderCopyright =
			"// Copyright Epic Games, Inc. All Rights Reserved.\r\n" +
			"/*===========================================================================\r\n" +
			"\tGenerated code exported from UnrealHeaderTool.\r\n" +
			"\tDO NOT modify this manually! Edit the corresponding .h files instead!\r\n" +
			"===========================================================================*/\r\n" +
			"\r\n";

		public static string RequiredCPPIncludes = "#include \"UObject/GeneratedCppIncludes.h\"\r\n";

		public static string EnableDeprecationWarnings = "PRAGMA_ENABLE_DEPRECATION_WARNINGS";
		public static string DisableDeprecationWarnings = "PRAGMA_DISABLE_DEPRECATION_WARNINGS";

		public readonly UhtCodeGenerator CodeGenerator;
		public readonly UhtPackage Package;
		public bool SaveExportedHeaders => Package.Module.SaveExportedHeaders;

		public Utils.UhtSession Session => CodeGenerator.Session;
		public UhtCodeGenerator.PackageInfo[] PackageInfos => CodeGenerator.PackageInfos;
		public UhtCodeGenerator.HeaderInfo[] HeaderInfos => CodeGenerator.HeaderInfos;
		public UhtCodeGenerator.ObjectInfo[] ObjectInfos => CodeGenerator.ObjectInfos;
		public string PackageApi => PackageInfos[Package.PackageTypeIndex].Api;
		public string PackageSingletonName => ObjectInfos[Package.ObjectTypeIndex].RegisteredSingletonName;

		public UhtPackageCodeGenerator(UhtCodeGenerator codeGenerator, UhtPackage package)
		{
			CodeGenerator = codeGenerator;
			Package = package;
		}

		#region Utility functions

		/// <summary>
		/// Return the singleton name for an object
		/// </summary>
		/// <param name="obj">The object in question.</param>
		/// <param name="registered">If true, return the registered singleton name.  Otherwise return the unregistered.</param>
		/// <returns>Singleton name of "nullptr" if Object is null</returns>
		public string GetSingletonName(UhtObject? obj, bool registered)
		{
			return CodeGenerator.GetSingletonName(obj, registered);
		}

		/// <summary>
		/// Return the external declaration for an object
		/// </summary>
		/// <param name="obj">The object in question.</param>
		/// <param name="registered">If true, return the registered external declaration.  Otherwise return the unregistered.</param>
		/// <returns>External declaration</returns>
		public string GetExternalDecl(UhtObject obj, bool registered)
		{
			return CodeGenerator.GetExternalDecl(obj, registered);
		}

		/// <summary>
		/// Return the external declaration for an object
		/// </summary>
		/// <param name="objectIndex">The object in question.</param>
		/// <param name="registered">If true, return the registered external declaration.  Otherwise return the unregistered.</param>
		/// <returns>External declaration</returns>
		public string GetExternalDecl(int objectIndex, bool registered)
		{
			return CodeGenerator.GetExternalDecl(objectIndex, registered);
		}

		/// <summary>
		/// Test to see if the given field is a delegate function
		/// </summary>
		/// <param name="field">Field to be tested</param>
		/// <returns>True if the field is a delegate function</returns>
		public static bool IsDelegateFunction(UhtField field)
		{
			if (field is UhtFunction function)
			{
				return function.FunctionType.IsDelegate();
			}
			return false;
		}

		/// <summary>
		/// Combines two hash values to get a third.
		/// Note - this function is not commutative.
		///
		/// This function cannot change for backward compatibility reasons.
		/// You may want to choose HashCombineFast for a better in-memory hash combining function.
		/// 
		/// NOTE: This is a copy of the method in TypeHash.h
		/// </summary>
		/// <param name="A">Hash to merge</param>
		/// <param name="C">Previously combined hash</param>
		/// <returns>Resulting hash value</returns>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "<Pending>")]
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE2001:Embedded statements must be on their own line", Justification = "<Pending>")]
		public static uint HashCombine(uint A, uint C)
		{
			uint B = 0x9e3779b9;
			A += B;

			A -= B; A -= C; A ^= (C >> 13);
			B -= C; B -= A; B ^= (A << 8);
			C -= A; C -= B; C ^= (B >> 13);
			A -= B; A -= C; A ^= (C >> 12);
			B -= C; B -= A; B ^= (A << 16);
			C -= A; C -= B; C ^= (B >> 5);
			A -= B; A -= C; A ^= (C >> 3);
			B -= C; B -= A; B ^= (A << 10);
			C -= A; C -= B; C ^= (B >> 15);

			return C;
		}
		#endregion
	}

	/// <summary>
	/// Helper formatting methods
	/// </summary>
	public static class UhtPackageCodeGeneratorExtensions
	{
		/// <summary>
		/// Append the meta data declaration
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="type">Source type containing the meta data</param>
		/// <param name="propertyContext">Context for formatting properties</param>
		/// <param name="properties">Optional collection of properties to output</param>
		/// <param name="name">Name</param>
		/// <param name="tabs">Number of tabs to indent</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendMetaDataDecl(this StringBuilder builder, UhtType type, IUhtPropertyMemberContext? propertyContext, UhtUsedDefineScopes<UhtProperty>? properties, string name, int tabs)
		{
			UhtStruct? structObj = type as UhtStruct;
			if (!type.MetaData.IsEmpty() || (properties != null && properties.Instances.Any(x => !x.MetaData.IsEmpty())))
			{
				builder.Append("#if WITH_METADATA\r\n");
				builder.AppendMetaDataDecl(type, null, name, null, null, tabs);
				if (propertyContext != null && properties != null)
				{
					builder.AppendInstances(properties, UhtDefineScopeNames.Standard,
						(builder, property) =>
						{
							builder.AppendMetaDataDecl(property, propertyContext, property.EngineName, "", tabs);
						});
				}
				builder.Append("#endif // WITH_METADATA\r\n");
			}
			return builder;
		}
	}
}
