// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal class UhtPackageCodeGeneratorCppFile : UhtPackageCodeGenerator
	{
		/// <summary>
		/// Construct an instance of this generator object
		/// </summary>
		/// <param name="codeGenerator">The base code generator</param>
		/// <param name="package">Package being generated</param>
		public UhtPackageCodeGeneratorCppFile(UhtCodeGenerator codeGenerator, UhtPackage package)
			: base(codeGenerator, package)
		{
		}

		/// <summary>
		/// For a given UE header file, generated the generated cpp file
		/// </summary>
		/// <param name="factory">Requesting factory</param>
		/// <param name="packageSortedHeaders">Sorted list of headers by name of all headers in the package</param>
		public void Generate(IUhtExportFactory factory, List<UhtHeaderFile> packageSortedHeaders)
		{
			{
				using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
				const string MetaDataParamsName = "Package_MetaDataParams";
				StringBuilder builder = borrower.StringBuilder;

				// Collect information from all of the headers
				List<UhtField> singletons = new();
				StringBuilder declarations = new();
				uint bodiesHash = 0;
				foreach (UhtHeaderFile headerFile in packageSortedHeaders)
				{
					ref UhtCodeGenerator.HeaderInfo headerInfo = ref HeaderInfos[headerFile.HeaderFileTypeIndex];
					ReadOnlyMemory<string> sorted = headerFile.References.Declaration.GetSortedReferences(
						(int objectIndex, bool registered) => GetExternalDecl(objectIndex, registered));
					foreach (string declaration in sorted.Span)
					{
						declarations.Append(declaration);
					}

					singletons.AddRange(headerFile.References.Singletons);

					uint bodyHash = HeaderInfos[headerFile.HeaderFileTypeIndex].BodyHash;
					if (bodiesHash == 0)
					{
						// Don't combine in the first case because it keeps GUID backwards compatibility
						bodiesHash = bodyHash;
					}
					else
					{
						bodiesHash = HashCombine(bodyHash, bodiesHash);
					}
				}

				// No need to generate output if we have no declarations
				if (declarations.Length == 0)
				{
					if (SaveExportedHeaders)
					{
						// We need to create the directory, otherwise UBT will think that this module has not been properly updated and won't write a Timestamp file
						System.IO.Directory.CreateDirectory(Package.Module.OutputDirectory);
					}
					return;
				}
				uint declarationsHash = UhtHash.GenenerateTextHash(declarations.ToString());

				string strippedName = PackageInfos[Package.PackageTypeIndex].StrippedName;
				string singletonName = PackageSingletonName;
				singletons.Sort((UhtField lhs, UhtField rhs) =>
				{
					bool lhsIsDel = IsDelegateFunction(lhs);
					bool rhsIsDel = IsDelegateFunction(rhs);
					if (lhsIsDel != rhsIsDel)
					{
						return !lhsIsDel ? -1 : 1;
					}
					return StringComparerUE.OrdinalIgnoreCase.Compare(
						ObjectInfos[lhs.ObjectTypeIndex].RegisteredSingletonName,
						ObjectInfos[rhs.ObjectTypeIndex].RegisteredSingletonName);
				});

				builder.Append(HeaderCopyright);
				builder.Append(RequiredCPPIncludes);
				builder.Append(DisableDeprecationWarnings).Append("\r\n");
				builder.Append("void EmptyLinkFunctionForGeneratedCode").Append(Package.ShortName).Append("_init() {}\r\n");

				if (Session.IncludeDebugOutput)
				{
					builder.Append("#if 0\r\n");
					foreach (UhtHeaderFile headerFile in packageSortedHeaders)
					{
						builder.Append('\t').Append(headerFile.FilePath).Append("\r\n");
					}
					builder.Append(declarations);
					builder.Append("#endif\r\n");
				}

				foreach (UhtObject obj in singletons)
				{
					ref UhtCodeGenerator.ObjectInfo info = ref ObjectInfos[obj.ObjectTypeIndex];
					builder.Append(info.RegsiteredExternalDecl);
				}

				builder.Append("\tstatic FPackageRegistrationInfo Z_Registration_Info_UPackage_").Append(strippedName).Append(";\r\n");
				builder.Append("\tFORCENOINLINE UPackage* ").Append(singletonName).Append("()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tif (!Z_Registration_Info_UPackage_").Append(strippedName).Append(".OuterSingleton)\r\n");
				builder.Append("\t\t{\r\n");

				if (singletons.Count != 0)
				{
					builder.Append("\t\t\tstatic UObject* (*const SingletonFuncArray[])() = {\r\n");
					foreach (UhtField field in singletons)
					{
						builder.Append("\t\t\t\t(UObject* (*)())").Append(ObjectInfos[field.ObjectTypeIndex].RegisteredSingletonName).Append(",\r\n");
					}
					builder.Append("\t\t\t};\r\n");
				}

				EPackageFlags flags = Package.PackageFlags & (EPackageFlags.ClientOptional | EPackageFlags.ServerSideOnly | EPackageFlags.EditorOnly | EPackageFlags.Developer | EPackageFlags.UncookedOnly);
				builder.Append("\t\t\tstatic const UECodeGen_Private::FPackageParams PackageParams = {\r\n");
				builder.Append("\t\t\t\t").AppendUTF8LiteralString(Package.SourceName).Append(",\r\n");
				builder.Append("\t\t\t\t").Append(singletons.Count != 0 ? "SingletonFuncArray" : "nullptr").Append(",\r\n");
				builder.Append("\t\t\t\t").Append(singletons.Count != 0 ? "UE_ARRAY_COUNT(SingletonFuncArray)" : "0").Append(",\r\n");
				builder.Append("\t\t\t\tPKG_CompiledIn | ").Append($"0x{(uint)flags:X8}").Append(",\r\n");
				builder.Append("\t\t\t\t").Append($"0x{bodiesHash:X8}").Append(",\r\n");
				builder.Append("\t\t\t\t").Append($"0x{declarationsHash:X8}").Append(",\r\n");
				builder.Append("\t\t\t\t").AppendMetaDataParams(Package, null, MetaDataParamsName).Append("\r\n");
				builder.Append("\t\t\t};\r\n");
				builder.Append("\t\t\tUECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage_").Append(strippedName).Append(".OuterSingleton, PackageParams);\r\n");
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn Z_Registration_Info_UPackage_").Append(strippedName).Append(".OuterSingleton;\r\n");
				builder.Append("\t}\r\n");

				// Do not change the Z_CompiledInDeferPackage_UPackage_ without changing LC_SymbolPatterns
				builder.Append("\tstatic FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage_").Append(strippedName).Append('(').Append(singletonName)
					.Append(", TEXT(\"").Append(Package.SourceName).Append("\"), Z_Registration_Info_UPackage_").Append(strippedName).Append(", CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, ")
					.Append($"0x{bodiesHash:X8}, 0x{declarationsHash:X8}").Append("));\r\n");

				builder.Append(EnableDeprecationWarnings).Append("\r\n");

				if (SaveExportedHeaders)
				{
					string cppFilePath = factory.MakePath(Package, ".init.gen.cpp");
					factory.CommitOutput(cppFilePath, builder);
				}
			}
		}
	}
}
