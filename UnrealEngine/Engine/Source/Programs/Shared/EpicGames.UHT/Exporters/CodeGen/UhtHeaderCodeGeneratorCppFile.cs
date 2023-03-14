// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal class UhtHeaderCodeGeneratorCppFile : UhtHeaderCodeGenerator
	{

		/// <summary>
		/// Construct an instance of this generator object
		/// </summary>
		/// <param name="codeGenerator">The base code generator</param>
		/// <param name="package">Package being generated</param>
		/// <param name="headerFile">Header file being generated</param>
		public UhtHeaderCodeGeneratorCppFile(UhtCodeGenerator codeGenerator, UhtPackage package, UhtHeaderFile headerFile)
			: base(codeGenerator, package, headerFile)
		{
		}

		/// <summary>
		/// For a given UE header file, generated the generated H file
		/// </summary>
		/// <param name="factory">Requesting factory</param>
		public void Generate(IUhtExportFactory factory)
		{
			ref UhtCodeGenerator.HeaderInfo headerInfo = ref HeaderInfos[HeaderFile.HeaderFileTypeIndex];
			{
				using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
				StringBuilder builder = borrower.StringBuilder;

				builder.Append(HeaderCopyright);
				builder.Append(RequiredCPPIncludes);
				builder.Append("#include \"").Append(headerInfo.IncludePath).Append("\"\r\n");

				bool addedStructuredArchiveFromArchiveHeader = false;
				bool addedArchiveUObjectFromStructuredArchiveHeader = false;
				bool addedCoreNetHeader = false;
				HashSet<UhtHeaderFile> addedIncludes = new();
				List<string> includesToAdd = new();
				addedIncludes.Add(HeaderFile);

				if (headerInfo.NeedsFastArrayHeaders)
				{
					includesToAdd.Add("Net/Serialization/FastArraySerializerImplementation.h");
				}

				foreach (UhtType type in HeaderFile.Children)
				{
					if (type is UhtStruct structObj)
					{
						// Functions
						foreach (UhtFunction function in structObj.Functions)
						{
							if (!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CppStatic) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetValidate))
							{
								if (!addedCoreNetHeader)
								{
									includesToAdd.Add("UObject/CoreNet.h");
									addedCoreNetHeader = true;
								}
							}
							foreach (UhtProperty property in function.Properties)
							{
								AddIncludeForProperty(property, addedIncludes, includesToAdd);
							}
						}

						// Properties
						foreach (UhtProperty property in structObj.Properties)
						{
							AddIncludeForProperty(property, addedIncludes, includesToAdd);
						}
					}

					if (type is UhtClass classObj)
					{
						if (classObj.ClassWithin != Session.UObject && !classObj.ClassWithin.HeaderFile.IsNoExportTypes)
						{
							if (addedIncludes.Add(classObj.ClassWithin.HeaderFile))
							{
								includesToAdd.Add(HeaderInfos[classObj.ClassWithin.HeaderFile.HeaderFileTypeIndex].IncludePath);
							}
						}

						switch (classObj.SerializerArchiveType)
						{
							case UhtSerializerArchiveType.None:
								break;

							case UhtSerializerArchiveType.Archive:
								if (!addedArchiveUObjectFromStructuredArchiveHeader)
								{
									includesToAdd.Add("Serialization/ArchiveUObjectFromStructuredArchive.h");
									addedArchiveUObjectFromStructuredArchiveHeader = true;
								}
								break;

							case UhtSerializerArchiveType.StructuredArchiveRecord:
								if (!addedStructuredArchiveFromArchiveHeader)
								{
									includesToAdd.Add("Serialization/StructuredArchive.h");
									addedStructuredArchiveFromArchiveHeader = true;
								}
								break;
						}
					}
					else
					{
						if (!type.HeaderFile.IsNoExportTypes && addedIncludes.Add(type.HeaderFile))
						{
							includesToAdd.Add(HeaderInfos[type.HeaderFile.HeaderFileTypeIndex].IncludePath);
						}
					}
				}

				includesToAdd.Sort(StringComparerUE.OrdinalIgnoreCase);
				foreach (string include in includesToAdd)
				{
					builder.Append("#include \"").Append(include).Append("\"\r\n");
				}

				builder.Append(DisableDeprecationWarnings).Append("\r\n");
				string cleanFileName = HeaderFile.FileNameWithoutExtension.Replace('.', '_');
				builder.Append("void EmptyLinkFunctionForGeneratedCode").Append(cleanFileName).Append("() {}\r\n");

				if (HeaderFile.References.CrossModule.References.Count > 0)
				{
					ReadOnlyMemory<string> sorted = HeaderFile.References.CrossModule.GetSortedReferences(
						(int objectIndex, bool registered) => GetCrossReference(objectIndex, registered));
					builder.Append("// Cross Module References\r\n");
					foreach (string crossReference in sorted.Span)
					{
						builder.Append(crossReference);
					}
					builder.Append("// End Cross Module References\r\n");
				}

				int generatedBodyStart = builder.Length;

				bool hasRegisteredEnums = false;
				bool hasRegisteredScriptStructs = false;
				bool hasRegisteredClasses = false;
				bool allEnumsEditorOnly = true;
				foreach (UhtField field in HeaderFile.References.ExportTypes)
				{
					if (field is UhtEnum enumObj)
					{
						AppendEnum(builder, enumObj);
						hasRegisteredEnums = true;
						allEnumsEditorOnly &= enumObj.IsEditorOnly;
					}
					else if (field is UhtScriptStruct scriptStruct)
					{
						AppendScriptStruct(builder, scriptStruct);
						if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
						{
							hasRegisteredScriptStructs = true;
						}
					}
					else if (field is UhtFunction function)
					{
						AppendDelegate(builder, function);
					}
					else if (field is UhtClass classObj)
					{
						if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
						{
							AppendClass(builder, classObj);
							hasRegisteredClasses = true;
						}
					}
				}

				if (hasRegisteredEnums || hasRegisteredScriptStructs || hasRegisteredClasses)
				{
					string name = $"Z_CompiledInDeferFile_{headerInfo.FileId}";
					string staticsName = $"{name}_Statics";

					builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
					builder.Append("\t{\r\n");

					if (hasRegisteredEnums)
					{
						if (allEnumsEditorOnly)
						{
							builder.Append("#if WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\t\tstatic const FEnumRegisterCompiledInInfo EnumInfo[];\r\n");
						if (allEnumsEditorOnly)
						{
							builder.Append("#endif\r\n");
						}
					}
					if (hasRegisteredScriptStructs)
					{
						builder.Append("\t\tstatic const FStructRegisterCompiledInInfo ScriptStructInfo[];\r\n");
					}
					if (hasRegisteredClasses)
					{
						builder.Append("\t\tstatic const FClassRegisterCompiledInInfo ClassInfo[];\r\n");
					}

					builder.Append("\t};\r\n");

					uint combinedHash = UInt32.MaxValue;

					if (hasRegisteredEnums)
					{
						if (allEnumsEditorOnly)
						{
							builder.Append("#if WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\tconst FEnumRegisterCompiledInInfo ").Append(staticsName).Append("::EnumInfo[] = {\r\n");
						foreach (UhtObject obj in HeaderFile.References.ExportTypes)
						{
							if (obj is UhtEnum enumObj)
							{
								if (!allEnumsEditorOnly && enumObj.IsEditorOnly)
								{
									builder.Append("#if WITH_EDITORONLY_DATA\r\n");
								}
								uint hash = ObjectInfos[enumObj.ObjectTypeIndex].Hash;
								builder
									.Append("\t\t{ ")
									.Append(enumObj.SourceName)
									.Append("_StaticEnum, TEXT(\"")
									.Append(enumObj.EngineName)
									.Append("\"), &Z_Registration_Info_UEnum_")
									.Append(enumObj.EngineName)
									.Append($", CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, {hash}U) }},\r\n");
								if (!allEnumsEditorOnly && enumObj.IsEditorOnly)
								{
									builder.Append("#endif\r\n");
								}
								combinedHash = HashCombine(combinedHash, hash);
							}
						}
						builder.Append("\t};\r\n");
						if (allEnumsEditorOnly)
						{
							builder.Append("#endif\r\n");
						}
					}

					if (hasRegisteredScriptStructs)
					{
						builder.Append("\tconst FStructRegisterCompiledInInfo ").Append(staticsName).Append("::ScriptStructInfo[] = {\r\n");
						foreach (UhtObject obj in HeaderFile.References.ExportTypes)
						{
							if (obj is UhtScriptStruct scriptStruct)
							{
								if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
								{
									uint hash = ObjectInfos[scriptStruct.ObjectTypeIndex].Hash;
									builder
										.Append("\t\t{ ")
										.Append(scriptStruct.SourceName)
										.Append("::StaticStruct, Z_Construct_UScriptStruct_")
										.Append(scriptStruct.SourceName)
										.Append("_Statics::NewStructOps, TEXT(\"")
										.Append(scriptStruct.EngineName)
										.Append("\"), &Z_Registration_Info_UScriptStruct_")
										.Append(scriptStruct.EngineName)
										.Append(", CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(")
										.Append(scriptStruct.SourceName)
										.Append($"), {hash}U) }},\r\n");
									combinedHash = HashCombine(combinedHash, hash);
								}
							}
						}
						builder.Append("\t};\r\n");
					}

					if (hasRegisteredClasses)
					{
						builder.Append("\tconst FClassRegisterCompiledInInfo ").Append(staticsName).Append("::ClassInfo[] = {\r\n");
						foreach (UhtObject obj in HeaderFile.References.ExportTypes)
						{
							if (obj is UhtClass classObj)
							{
								if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
								{
									uint hash = ObjectInfos[classObj.ObjectTypeIndex].Hash;
									builder
										.Append("\t\t{ Z_Construct_UClass_")
										.Append(classObj.SourceName)
										.Append(", ")
										.Append(classObj.SourceName)
										.Append("::StaticClass, TEXT(\"")
										.Append(classObj.SourceName)
										.Append("\"), &Z_Registration_Info_UClass_")
										.Append(classObj.SourceName)
										.Append(", CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(")
										.Append(classObj.SourceName)
										.Append($"), {hash}U) }},\r\n");
									combinedHash = HashCombine(combinedHash, hash);
								}
							}
						}
						builder.Append("\t};\r\n");
					}

					builder
						.Append("\tstatic FRegisterCompiledInInfo ")
						.Append(name)
						.Append($"_{combinedHash}(TEXT(\"")
						.Append(Package.EngineName)
						.Append("\"),\r\n");

					builder.Append("\t\t").AppendArray(!hasRegisteredClasses, false, staticsName, "ClassInfo").Append(",\r\n");
					builder.Append("\t\t").AppendArray(!hasRegisteredScriptStructs, false, staticsName, "ScriptStructInfo").Append(",\r\n");
					builder.Append("\t\t").AppendArray(!hasRegisteredEnums, allEnumsEditorOnly, staticsName, "EnumInfo").Append(");\r\n");
				}

				if (Session.IncludeDebugOutput)
				{
					builder.Append("#if 0\r\n");
					ReadOnlyMemory<string> sorted = HeaderFile.References.Declaration.GetSortedReferences(
						(int objectIndex, bool registered) => GetExternalDecl(objectIndex, registered));
					foreach (string declaration in sorted.Span)
					{
						builder.Append(declaration);
					}
					builder.Append("#endif\r\n");
				}

				int generatedBodyEnd = builder.Length;

				builder.Append(EnableDeprecationWarnings).Append("\r\n");

				{
					using UhtBorrowBuffer borrowBuffer = new(builder);
					string cppFilePath = factory.MakePath(HeaderFile, ".gen.cpp");
					StringView generatedBody = new(borrowBuffer.Buffer.Memory);
					if (SaveExportedHeaders)
					{
						factory.CommitOutput(cppFilePath, generatedBody);
					}

					// Save the hash of the generated body 
					HeaderInfos[HeaderFile.HeaderFileTypeIndex].BodyHash = UhtHash.GenenerateTextHash(generatedBody.Span[generatedBodyStart..generatedBodyEnd]);
				}
			}
		}

		private void AddIncludeForType(UhtProperty uhtProperty, HashSet<UhtHeaderFile> addedIncludes, IList<string> includesToAdd)
		{
			if (uhtProperty is UhtStructProperty structProperty)
			{
				UhtScriptStruct scriptStruct = structProperty.ScriptStruct;
				if (!scriptStruct.HeaderFile.IsNoExportTypes && addedIncludes.Add(scriptStruct.HeaderFile))
				{
					includesToAdd.Add(HeaderInfos[scriptStruct.HeaderFile.HeaderFileTypeIndex].IncludePath);
				}
			}
		}

		private void AddIncludeForProperty(UhtProperty property, HashSet<UhtHeaderFile> addedIncludes, IList<string> includesToAdd)
		{
			AddIncludeForType(property, addedIncludes, includesToAdd);

			if (property is UhtContainerBaseProperty containerProperty)
			{
				AddIncludeForType(containerProperty.ValueProperty, addedIncludes, includesToAdd);
			}

			if (property is UhtMapProperty mapProperty)
			{
				AddIncludeForType(mapProperty.KeyProperty, addedIncludes, includesToAdd);
			}
		}

		private StringBuilder AppendEnum(StringBuilder builder, UhtEnum enumObj)
		{
			const string MetaDataParamsName = "Enum_MetaDataParams";
			const string ObjectFlags = "RF_Public|RF_Transient|RF_MarkAsNative";
			string singletonName = GetSingletonName(enumObj, true);
			string staticsName = singletonName + "_Statics";
			string registrationName = $"Z_Registration_Info_UEnum_{enumObj.SourceName}";

			string enumDisplayNameFn = enumObj.MetaData.GetValueOrDefault(UhtNames.EnumDisplayNameFn);
			if (enumDisplayNameFn.Length == 0)
			{
				enumDisplayNameFn = "nullptr";
			}

			using (UhtMacroBlockEmitter macroBlockEmitter = new(builder, "WITH_EDITORONLY_DATA", enumObj.IsEditorOnly))
			{

				// If we don't have a zero 0 then we emit a static assert to verify we have one
				if (!enumObj.IsValidEnumValue(0) && enumObj.MetaData.ContainsKey(UhtNames.BlueprintType))
				{
					bool hasUnparsedValue = enumObj.EnumValues.Exists(x => x.Value == -1);
					if (hasUnparsedValue)
					{
						builder.Append("\tstatic_assert(");
						bool doneFirst = false;
						foreach (UhtEnumValue value in enumObj.EnumValues)
						{
							if (value.Value == -1)
							{
								if (doneFirst)
								{
									builder.Append("||");
								}
								doneFirst = true;
								builder.Append("!int64(").Append(value.Name).Append(')');
							}
						}
						builder.Append(", \"'").Append(enumObj.SourceName).Append("' does not have a 0 entry!(This is a problem when the enum is initalized by default)\");\r\n");
					}
				}

				builder.Append("\tstatic FEnumRegistrationInfo ").Append(registrationName).Append(";\r\n");
				builder.Append("\tstatic UEnum* ").Append(enumObj.SourceName).Append("_StaticEnum()\r\n");
				builder.Append("\t{\r\n");

				builder.Append("\t\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\t").Append(registrationName).Append(".OuterSingleton = GetStaticEnum(").Append(singletonName).Append(", ")
					.Append(PackageSingletonName).Append("(), TEXT(\"").Append(enumObj.SourceName).Append("\"));\r\n");
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn ").Append(registrationName).Append(".OuterSingleton;\r\n");

				builder.Append("\t}\r\n");

				builder.Append("\ttemplate<> ").Append(PackageApi).Append("UEnum* StaticEnum<").Append(enumObj.CppType).Append(">()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\treturn ").Append(enumObj.SourceName).Append("_StaticEnum();\r\n");
				builder.Append("\t}\r\n");

				// Everything from this point on will be part of the definition hash
				int hashCodeBlockStart = builder.Length;

				// Statics declaration
				builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tstatic const UECodeGen_Private::FEnumeratorParam Enumerators[];\r\n");
				builder.AppendMetaDataDecl(enumObj, MetaDataParamsName, 2);
				builder.Append("\t\tstatic const UECodeGen_Private::FEnumParams EnumParams;\r\n");
				builder.Append("\t};\r\n");

				// Enumerators
				builder.Append("\tconst UECodeGen_Private::FEnumeratorParam ").Append(staticsName).Append("::Enumerators[] = {\r\n");
				int enumIndex = 0;
				foreach (UhtEnumValue value in enumObj.EnumValues)
				{
					if (!enumObj.MetaData.TryGetValue("OverrideName", enumIndex, out string? keyName))
					{
						keyName = value.Name.ToString();
					}
					builder.Append("\t\t{ ").AppendUTF8LiteralString(keyName).Append(", (int64)").Append(value.Name).Append(" },\r\n");
					++enumIndex;
				}
				builder.Append("\t};\r\n");

				// Meta data
				builder.AppendMetaDataDef(enumObj, staticsName, MetaDataParamsName, 1);

				// Singleton parameters
				builder.Append("\tconst UECodeGen_Private::FEnumParams ").Append(staticsName).Append("::EnumParams = {\r\n");
				builder.Append("\t\t(UObject*(*)())").Append(PackageSingletonName).Append(",\r\n");
				builder.Append("\t\t").Append(enumDisplayNameFn).Append(",\r\n");
				builder.Append("\t\t").AppendUTF8LiteralString(enumObj.SourceName).Append(",\r\n");
				builder.Append("\t\t").AppendUTF8LiteralString(enumObj.CppType).Append(",\r\n");
				builder.Append("\t\t").Append(staticsName).Append("::Enumerators,\r\n");
				builder.Append("\t\tUE_ARRAY_COUNT(").Append(staticsName).Append("::Enumerators),\r\n");
				builder.Append("\t\t").Append(ObjectFlags).Append(",\r\n");
				builder.Append("\t\t").Append(enumObj.EnumFlags.HasAnyFlags(EEnumFlags.Flags) ? "EEnumFlags::Flags" : "EEnumFlags::None").Append(",\r\n");
				builder.Append("\t\t(uint8)UEnum::ECppForm::").Append(enumObj.CppForm.ToString()).Append(",\r\n");
				builder.Append("\t\t").AppendMetaDataParams(enumObj, staticsName, MetaDataParamsName).Append("\r\n");
				builder.Append("\t};\r\n");

				// Registration singleton
				builder.Append("\tUEnum* ").Append(singletonName).Append("()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tif (!").Append(registrationName).Append(".InnerSingleton)\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\tUECodeGen_Private::ConstructUEnum(").Append(registrationName).Append(".InnerSingleton, ").Append(staticsName).Append("::EnumParams);\r\n");
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn ").Append(registrationName).Append(".InnerSingleton;\r\n");
				builder.Append("\t}\r\n");

				{
					using UhtBorrowBuffer borrowBuffer = new(builder, hashCodeBlockStart, builder.Length - hashCodeBlockStart);
					ObjectInfos[enumObj.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
				}
			}
			return builder;
		}

		private StringBuilder AppendScriptStruct(StringBuilder builder, UhtScriptStruct scriptStruct)
		{
			const string MetaDataParamsName = "Struct_MetaDataParams";
			string singletonName = GetSingletonName(scriptStruct, true);
			string staticsName = singletonName + "_Statics";
			string registrationName = $"Z_Registration_Info_UScriptStruct_{scriptStruct.EngineName}";
			List<UhtScriptStruct> noExportStructs = FindNoExportStructs(scriptStruct);

			if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
			{
				// Inject static assert to verify that we do not add vtable
				if (scriptStruct.SuperScriptStruct != null)
				{
					builder.Append("\r\n");
					builder.Append("static_assert(std::is_polymorphic<")
						.Append(scriptStruct.SourceName)
						.Append(">() == std::is_polymorphic<")
						.Append(scriptStruct.SuperScriptStruct.SourceName)
						.Append(">(), \"USTRUCT ")
						.Append(scriptStruct.SourceName)
						.Append(" cannot be polymorphic unless super ")
						.Append(scriptStruct.SuperScriptStruct.SourceName)
						.Append(" is polymorphic\");\r\n");
					builder.Append("\r\n");
				}

				// Outer singleton
				builder.Append("\tstatic FStructRegistrationInfo ").Append(registrationName).Append(";\r\n");
				builder.Append("class UScriptStruct* ").Append(scriptStruct.SourceName).Append("::StaticStruct()\r\n");
				builder.Append("{\r\n");
				builder.Append("\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\t")
					.Append(registrationName)
					.Append(".OuterSingleton = GetStaticStruct(")
					.Append(singletonName)
					.Append(", ")
					.Append(PackageSingletonName)
					.Append("(), TEXT(\"")
					.Append(scriptStruct.EngineName)
					.Append("\"));\r\n");

				// if this struct has RigVM methods - we need to register the method to our central
				// registry on construction of the static struct
				if (scriptStruct.RigVMStructInfo != null)
				{
					foreach (UhtRigVMMethodInfo methodInfo in scriptStruct.RigVMStructInfo.Methods)
					{
						builder.Append("\t\tTArray<FRigVMFunctionArgument> ").AppendArgumentsName(scriptStruct, methodInfo).Append(";\r\n");
						foreach (UhtRigVMParameter parameter in scriptStruct.RigVMStructInfo.Members)
						{
							builder
								.Append("\t\t")
								.AppendArgumentsName(scriptStruct, methodInfo)
								.Append(".Emplace(TEXT(\"")
								.Append(parameter.NameOriginal())
								.Append("\"), TEXT(\"")
								.Append(parameter.TypeOriginal())
								.Append("\"));\r\n");
						}
						foreach (UhtRigVMParameter parameter in methodInfo.Parameters)
						{
							builder
								.Append("\t\t")
								.AppendArgumentsName(scriptStruct, methodInfo)
								.Append(".Emplace(TEXT(\"")
								.Append(parameter.NameOriginal())
								.Append("\"), TEXT(\"")
								.Append(parameter.TypeOriginal())
								.Append("\"));\r\n");
						}
						builder
							.Append("\t\tFRigVMRegistry::Get().Register(TEXT(\"")
							.Append(scriptStruct.SourceName)
							.Append("::")
							.Append(methodInfo.Name)
							.Append("\"), &")
							.Append(scriptStruct.SourceName)
							.Append("::RigVM")
							.Append(methodInfo.Name)
							.Append(", ")
							.Append(registrationName)
							.Append(".OuterSingleton, ")
							.AppendArgumentsName(scriptStruct, methodInfo)
							.Append(");\r\n");
					}
				}

				builder.Append("\t}\r\n");
				builder.Append("\treturn ").Append(registrationName).Append(".OuterSingleton;\r\n");
				builder.Append("}\r\n");

				// Generate the StaticStruct specialization
				builder.Append("template<> ").Append(PackageApi).Append("UScriptStruct* StaticStruct<").Append(scriptStruct.SourceName).Append(">()\r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn ").Append(scriptStruct.SourceName).Append("::StaticStruct();\r\n");
				builder.Append("}\r\n");

				// Inject implementation needed to support auto bindings of fast arrays
				if (ObjectInfos[scriptStruct.ObjectTypeIndex].FastArrayProperty != null)
				{
					builder.Append("UE_NET_IMPLEMENT_FASTARRAY(").Append(scriptStruct.SourceName).Append(");\r\n");
				}
			}

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			// Declare the statics structure
			{
				builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
				builder.Append("\t{\r\n");

				foreach (UhtScriptStruct noExportStruct in noExportStructs)
				{
					AppendMirrorsForNoexportStruct(builder, noExportStruct, 2);
				}

				// Meta data
				builder.AppendMetaDataDecl(scriptStruct, MetaDataParamsName, 2);

				// New struct ops
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					builder.Append("\t\tstatic void* NewStructOps();\r\n");
				}

				AppendPropertiesDecl(builder, scriptStruct, scriptStruct.SourceName, staticsName, 2);

				builder.Append("\t\tstatic const UECodeGen_Private::FStructParams ReturnStructParams;\r\n");
				builder.Append("\t};\r\n");
			}

			// Populate the elements of the static structure
			{
				builder.AppendMetaDataDef(scriptStruct, staticsName, MetaDataParamsName, 1);

				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					builder.Append("\tvoid* ").Append(staticsName).Append("::NewStructOps()\r\n");
					builder.Append("\t{\r\n");
					builder.Append("\t\treturn (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<").Append(scriptStruct.SourceName).Append(">();\r\n");
					builder.Append("\t}\r\n");
				}

				AppendPropertiesDefs(builder, scriptStruct, scriptStruct.SourceName, staticsName, 1);

				builder.Append("\tconst UECodeGen_Private::FStructParams ").Append(staticsName).Append("::ReturnStructParams = {\r\n");
				builder.Append("\t\t(UObject* (*)())").Append(PackageSingletonName).Append(",\r\n");
				builder.Append("\t\t").Append(GetSingletonName(scriptStruct.SuperScriptStruct, true)).Append(",\r\n");
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					builder.Append("\t\t&NewStructOps,\r\n");
				}
				else
				{
					builder.Append("\t\tnullptr,\r\n");
				}
				builder.Append("\t\t").AppendUTF8LiteralString(scriptStruct.EngineName).Append(",\r\n");
				builder.Append("\t\tsizeof(").Append(scriptStruct.SourceName).Append("),\r\n");
				builder.Append("\t\talignof(").Append(scriptStruct.SourceName).Append("),\r\n");
				builder.AppendPropertiesParams(scriptStruct, staticsName, 2, "\r\n");
				builder.Append("\t\tRF_Public|RF_Transient|RF_MarkAsNative,\r\n");
				builder.Append($"\t\tEStructFlags(0x{(uint)(scriptStruct.ScriptStructFlags & ~EStructFlags.ComputedFlags):X8}),\r\n");
				builder.Append("\t\t").AppendMetaDataParams(scriptStruct, staticsName, MetaDataParamsName).Append("\r\n");
				builder.Append("\t};\r\n");
			}

			// Generate the registration function
			{
				builder.Append("\tUScriptStruct* ").Append(singletonName).Append("()\r\n");
				builder.Append("\t{\r\n");
				string innerSingletonName;
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					innerSingletonName = $"{registrationName}.InnerSingleton";
				}
				else
				{
					builder.Append("\t\tstatic UScriptStruct* ReturnStruct = nullptr;\r\n");
					innerSingletonName = "ReturnStruct";
				}
				builder.Append("\t\tif (!").Append(innerSingletonName).Append(")\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\tUECodeGen_Private::ConstructUScriptStruct(").Append(innerSingletonName).Append(", ").Append(staticsName).Append("::ReturnStructParams);\r\n");
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn ").Append(innerSingletonName).Append(";\r\n");
				builder.Append("\t}\r\n");
			}

			using (UhtBorrowBuffer borrowBuffer = new(builder, hashCodeBlockStart, builder.Length - hashCodeBlockStart))
			{
				ObjectInfos[scriptStruct.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
			}

			// if this struct has RigVM methods we need to implement both the 
			// virtual function as well as the stub method here.
			// The static method is implemented by the user using a macro.
			if (scriptStruct.RigVMStructInfo != null)
			{
				foreach (UhtRigVMMethodInfo methodInfo in scriptStruct.RigVMStructInfo.Methods)
				{
					builder.Append("\r\n");

					builder
						.Append(methodInfo.ReturnType)
						.Append(' ')
						.Append(scriptStruct.SourceName)
						.Append("::")
						.Append(methodInfo.Name)
						.Append('(')
						.AppendParameterDecls(methodInfo.Parameters, false, ",\r\n\t\t")
						.Append(")\r\n");
					builder.Append("{\r\n");
					builder.Append("\tFRigVMExecuteContext RigVMExecuteContext;\r\n");

					bool wroteLine = false;
					foreach (UhtRigVMParameter parameter in scriptStruct.RigVMStructInfo.Members)
					{
						if (!parameter.RequiresCast())
						{
							continue;
						}
						wroteLine = true;
						builder.Append('\t').Append(parameter.CastType).Append(' ').Append(parameter.CastName).Append('(').Append(parameter.Name).Append(");\r\n");
					}
					if (wroteLine)
					{
						//COMPATIBILITY-TODO - Remove the tab
						builder.Append("\t\r\n");
					}

					//COMPATIBILITY-TODO - Replace spaces with \t
					builder.Append('\t').Append(methodInfo.ReturnPrefix()).Append("Static").Append(methodInfo.Name).Append("(\r\n");
					builder.Append("\t\tRigVMExecuteContext");
					builder.AppendParameterNames(scriptStruct.RigVMStructInfo.Members, true, ",\r\n\t\t", true);
					builder.AppendParameterNames(methodInfo.Parameters, true, ",\r\n\t\t");
					builder.Append("\r\n");
					builder.Append("\t);\r\n");
					builder.Append("}\r\n");
				}
				builder.Append("\r\n");
			}
			return builder;
		}

		private StringBuilder AppendDelegate(StringBuilder builder, UhtFunction function)
		{
			AppendFunction(builder, function, false);
			return builder;
		}

		private StringBuilder AppendFunction(StringBuilder builder, UhtFunction function, bool isNoExport)
		{
			const string MetaDataParamsName = "Function_MetaDataParams";
			string singletonName = GetSingletonName(function, true);
			string staticsName = singletonName + "_Statics";
			bool paramsInStatic = isNoExport || !function.FunctionFlags.HasAnyFlags(EFunctionFlags.Event);
			bool isNet = function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetRequest | EFunctionFlags.NetResponse);

			string strippedFunctionName = function.StrippedFunctionName;
			string eventParameters = GetEventStructParametersName(function.Outer, strippedFunctionName);

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			builder.AppendBeginEditorOnlyGuard(function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly));

			// Statics declaration
			{
				builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
				builder.Append("\t{\r\n");

				if (paramsInStatic)
				{
					List<UhtScriptStruct> noExportStructs = FindNoExportStructs(function);
					foreach (UhtScriptStruct noExportStruct in noExportStructs)
					{
						AppendMirrorsForNoexportStruct(builder, noExportStruct, 2);
					}
					AppendEventParameter(builder, function, strippedFunctionName, UhtPropertyTextType.EventParameterFunctionMember, false, 2, "\r\n");
				}

				AppendPropertiesDecl(builder, function, eventParameters, staticsName, 2);

				builder.AppendMetaDataDecl(function, MetaDataParamsName, 2);

				builder.Append("\t\tstatic const UECodeGen_Private::FFunctionParams FuncParams;\r\n");

				builder.Append("\t};\r\n");
			}

			// Statics definition
			{
				AppendPropertiesDefs(builder, function, eventParameters, staticsName, 1);

				builder.AppendMetaDataDef(function, staticsName, MetaDataParamsName, 1);

				builder
					.Append("\tconst UECodeGen_Private::FFunctionParams ")
					.Append(staticsName).Append("::FuncParams = { ")
					.Append("(UObject*(*)())").Append(GetFunctionOuterFunc(function)).Append(", ")
					.Append(GetSingletonName(function.SuperFunction, true)).Append(", ")
					.AppendUTF8LiteralString(function.EngineName).Append(", ")
					.AppendUTF8LiteralString(function.FunctionType == UhtFunctionType.SparseDelegate, function.SparseOwningClassName).Append(", ")
					.AppendUTF8LiteralString(function.FunctionType == UhtFunctionType.SparseDelegate, function.SparseDelegateName).Append(", ");

				if (function.Children.Count > 0)
				{
					UhtFunction tempFunction = function;
					while (tempFunction.SuperFunction != null)
					{
						tempFunction = tempFunction.SuperFunction;
					}

					if (paramsInStatic)
					{
						builder.Append("sizeof(").Append(staticsName).Append("::").Append(GetEventStructParametersName(tempFunction.Outer, tempFunction.StrippedFunctionName)).Append(')');
					}
					else
					{
						builder.Append("sizeof(").Append(GetEventStructParametersName(tempFunction.Outer, tempFunction.StrippedFunctionName)).Append(')');
					}
				}
				else
				{
					builder.Append('0');
				}
				builder.Append(", ");

				builder
					.AppendPropertiesParams(function, staticsName, 0, " ")
					.Append("RF_Public|RF_Transient|RF_MarkAsNative, ")
					.Append($"(EFunctionFlags)0x{(uint)function.FunctionFlags:X8}, ")
					.Append(isNet ? function.RPCId : 0).Append(", ")
					.Append(isNet ? function.RPCResponseId : 0).Append(", ")
					.AppendMetaDataParams(function, staticsName, MetaDataParamsName)
					.Append(" };\r\n");
			}

			// Registration function
			{
				builder.Append("\tUFunction* ").Append(singletonName).Append("()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tstatic UFunction* ReturnFunction = nullptr;\r\n");
				builder.Append("\t\tif (!ReturnFunction)\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\tUECodeGen_Private::ConstructUFunction(&ReturnFunction, ").Append(staticsName).Append("::FuncParams);\r\n");
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn ReturnFunction;\r\n");
				builder.Append("\t}\r\n");
			}

			builder.AppendEndEditorOnlyGuard(function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly));

			using (UhtBorrowBuffer borrowBuffer = new(builder, hashCodeBlockStart, builder.Length - hashCodeBlockStart))
			{
				ObjectInfos[function.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
			}
			return builder;
		}

		private string GetFunctionOuterFunc(UhtFunction function)
		{
			if (function.Outer == null)
			{
				return "nullptr";
			}
			else if (function.Outer is UhtHeaderFile headerFile)
			{
				return GetSingletonName(headerFile.Package, true);
			}
			else
			{
				return GetSingletonName((UhtObject)function.Outer, true);
			}
		}

		private static StringBuilder AppendMirrorsForNoexportStruct(StringBuilder builder, UhtScriptStruct noExportStruct, int tabs)
		{
			builder.AppendTabs(tabs).Append("struct ").Append(noExportStruct.SourceName);
			if (noExportStruct.SuperScriptStruct != null)
			{
				builder.Append(" : public ").Append(noExportStruct.SuperScriptStruct.SourceName);
			}
			builder.Append("\r\n");
			builder.AppendTabs(tabs).Append("{\r\n");

			// Export the struct's CPP properties
			AppendExportProperties(builder, noExportStruct, tabs + 1);

			builder.AppendTabs(tabs).Append("};\r\n");
			builder.Append("\r\n");
			return builder;
		}

		private static StringBuilder AppendExportProperties(StringBuilder builder, UhtScriptStruct scriptStruct, int tabs)
		{
			using (UhtMacroBlockEmitter emitter = new(builder, "WITH_EDITORONLY_DATA"))
			{
				foreach (UhtProperty property in scriptStruct.Properties)
				{
					emitter.Set(property.IsEditorOnlyProperty);
					builder.AppendTabs(tabs).AppendFullDecl(property, UhtPropertyTextType.ExportMember, false).Append(";\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendPropertiesDecl(StringBuilder builder, UhtStruct structObj, string structSourceName, string staticsName, int tabs)
		{
			if (!structObj.Children.Any(x => x is UhtProperty))
			{
				return builder;
			}

			PropertyMemberContextImpl context = new(CodeGenerator, structObj, structSourceName, staticsName);

			using (UhtMacroBlockEmitter emitter = new(builder, "WITH_EDITORONLY_DATA"))
			{
				bool hasAllEditorOnlyDataProperties = true;
				foreach (UhtType type in structObj.Children)
				{
					if (type is UhtProperty property)
					{
						emitter.Set(property.IsEditorOnlyProperty);
						hasAllEditorOnlyDataProperties &= property.IsEditorOnlyProperty;
						builder.AppendMemberDecl(property, context, property.EngineName, "", tabs);
					}
				}

				// This will force it off if the last one was editor only but we has some that weren't
				emitter.Set(hasAllEditorOnlyDataProperties);

				builder.AppendTabs(tabs).Append("static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];\r\n");

				foreach (UhtType type in structObj.Children)
				{
					if (type is UhtProperty property)
					{
						emitter.Set(property.IsEditorOnlyProperty);
					}
				}
				emitter.Set(hasAllEditorOnlyDataProperties);
			}
			return builder;
		}

		private StringBuilder AppendPropertiesDefs(StringBuilder builder, UhtStruct structObj, string structSourceName, string staticsName, int tabs)
		{
			if (!structObj.Children.Any(x => x is UhtProperty))
			{
				return builder;
			}

			PropertyMemberContextImpl context = new(CodeGenerator, structObj, structSourceName, staticsName);

			using (UhtMacroBlockEmitter emitter = new(builder, "WITH_EDITORONLY_DATA"))
			{
				bool hasAllEditorOnlyDataProperties = true;
				foreach (UhtType type in structObj.Children)
				{
					if (type is UhtProperty property)
					{
						emitter.Set(property.IsEditorOnlyProperty);
						hasAllEditorOnlyDataProperties &= property.IsEditorOnlyProperty;
						builder.AppendMemberDef(property, context, property.EngineName, "", null, tabs);
					}
				}

				// This will force it off if the last one was editor only but we has some that weren't
				emitter.Set(hasAllEditorOnlyDataProperties);

				builder.AppendTabs(tabs).Append("const UECodeGen_Private::FPropertyParamsBase* const ").Append(staticsName).Append("::PropPointers[] = {\r\n");
				foreach (UhtType type in structObj.Children)
				{
					if (type is UhtProperty property)
					{
						emitter.Set(property.IsEditorOnlyProperty);
						builder.AppendMemberPtr(property, context, property.EngineName, "", tabs + 1);
					}
				}

				// This will force it off if the last one was editor only but we has some that weren't
				emitter.Set(hasAllEditorOnlyDataProperties);

				builder.AppendTabs(tabs).Append("};\r\n");
			}
			return builder;
		}

		private StringBuilder AppendClass(StringBuilder builder, UhtClass classObj)
		{
			// Collect the functions in reversed order
			List<UhtFunction> reversedFunctions = new(classObj.Functions.Where(x => IsRpcFunction(x) && ShouldExportFunction(x)));
			reversedFunctions.Reverse();

			// Check to see if we have any RPC functions for the editor
			bool hasEditorRpc = reversedFunctions.Any(x => x.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly));

			// Output the RPC methods
			AppendRpcFunctions(builder, classObj, reversedFunctions, false);
			if (hasEditorRpc)
			{
				builder.AppendBeginEditorOnlyGuard();
				AppendRpcFunctions(builder, classObj, reversedFunctions, true);
				builder.AppendEndEditorOnlyGuard();
			}

			// Add the accessors
			AppendPropertyAccessors(builder, classObj);

			// Add sparse accessors
			AppendSparseAccessors(builder, classObj);

			// Collect the callback function and sort by name to make the order stable
			List<UhtFunction> callbackFunctions = new(classObj.Functions.Where(x => x.FunctionFlags.HasAnyFlags(EFunctionFlags.Event) && x.SuperFunction == null));
			callbackFunctions.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

			// Generate the callback parameter structures
			AppendCallbackParametersDecls(builder, classObj, callbackFunctions);

			// VM -> C++ proxies (events and delegates).
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
			{
				AppendCallbackFunctions(builder, classObj, callbackFunctions);
			}

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
			{
				AppendNatives(builder, classObj);
			}

			AppendNativeGeneratedInitCode(builder, classObj);

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasCustomVTableHelperConstructor))
			{
				builder.Append("\tDEFINE_VTABLE_PTR_HELPER_CTOR(").Append(classObj.SourceName).Append(");\r\n");
			}

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasDestructor))
			{
				builder.Append('\t').Append(classObj.SourceName).Append("::~").Append(classObj.SourceName).Append("() {}\r\n");
			}

			AppendFieldNotify(builder, classObj);

			// Only write out adapters if the user has provided one or the other of the Serialize overloads
			if (classObj.SerializerArchiveType != UhtSerializerArchiveType.None && classObj.SerializerArchiveType != UhtSerializerArchiveType.All)
			{
				AppendSerializer(builder, classObj, UhtSerializerArchiveType.Archive, "IMPLEMENT_FARCHIVE_SERIALIZER");
				AppendSerializer(builder, classObj, UhtSerializerArchiveType.StructuredArchiveRecord, "IMPLEMENT_FSTRUCTUREDARCHIVE_SERIALIZER");
			}

			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
			{
				AppendInterfaceCallFunctions(builder, classObj, callbackFunctions);
			}
			return builder;
		}

		private static StringBuilder AppendFieldNotify(StringBuilder builder, UhtClass classObj)
		{
			if (!NeedFieldNotifyCodeGen(classObj))
			{
				return builder;
			}

			// Scan the children to see what we have
			GetFieldNotifyStats(classObj, out bool hasProperties, out bool hasFunctions, out bool hasEditorFields, out bool allEditorFields);

			if (allEditorFields)
			{
				builder.Append("#if WITH_EDITORONLY_DATA\r\n");
			}

			//UE_FIELD_NOTIFICATION_DECLARE_FIELD
			AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields, true, true,
				(StringBuilder builder, UhtClass classObj, string name) =>
				{
					builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD({classObj.SourceName}, {name})\r\n");
				});

			//UE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD
			builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_BEGIN({classObj.SourceName})\r\n");
			AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields, true, true,
				(StringBuilder builder, UhtClass classObj, string name) =>
				{
					builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENT_ENUM_FIELD({classObj.SourceName}, {name})\r\n");
				});
			builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_END({classObj.SourceName});\r\n");

			if (allEditorFields)
			{
				builder.Append("#endif // WITH_EDITORONLY_DATA\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendPropertyAccessors(StringBuilder builder, UhtClass classObj)
		{
			foreach (UhtType type in classObj.Children)
			{
				if (type is UhtProperty property)
				{
					bool editorOnlyProperty = property.IsEditorOnlyProperty;
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.GetterFound))
					{
						builder.Append("\tvoid ").Append(classObj.SourceName).Append("::").AppendPropertyGetterWrapperName(property).Append("(const void* Object, void* OutValue)\r\n");
						builder.Append("\t{\r\n");
						if (editorOnlyProperty)
						{
							builder.Append("\t#if WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\t\tconst ").Append(classObj.SourceName).Append("* Obj = (const ").Append(classObj.SourceName).Append("*)Object;\r\n");
						if (property.IsStaticArray)
						{
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("* Source = (")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("*)Obj->")
								.Append(property.Getter!)
								.Append("();\r\n");
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("* Result = (")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("*)OutValue;\r\n");
							builder
								.Append("\t\tCopyAssignItems(Result, Source, ")
								.Append(property.ArrayDimensions)
								.Append(");\r\n");
						}
						else
						{
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("& Result = *(")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("*)OutValue;\r\n");
							builder
								.Append("\t\tResult = (")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append(")Obj->")
								.Append(property.Getter!)
								.Append("();\r\n");
						}
						if (editorOnlyProperty)
						{
							builder.Append("\t#endif // WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\t}\r\n");
					}
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.SetterFound))
					{
						builder.Append("\tvoid ").Append(classObj.SourceName).Append("::").AppendPropertySetterWrapperName(property).Append("(void* Object, const void* InValue)\r\n");
						builder.Append("\t{\r\n");
						if (editorOnlyProperty)
						{
							builder.Append("\t#if WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\t\t").Append(classObj.SourceName).Append("* Obj = (").Append(classObj.SourceName).Append("*)Object;\r\n");
						if (property.IsStaticArray)
						{
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("* Value = (")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("*)InValue;\r\n");
						}
						else if (property is UhtByteProperty byteProperty && byteProperty.Enum != null)
						{
							// If someone passed in a TEnumAsByte instead of the actual enum value, the cast in the else clause would cause an issue.
							// Since this is known to be a TEnumAsByte, we just fetch the first byte.  *HOWEVER* on MSB machines where 
							// the actual enum value is passed in this will fail and return zero if the native size of the enum > 1 byte.
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append(" Value = (")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append(")*(uint8*)InValue;\r\n");
						}
						else
						{
							builder
								.Append("\t\t")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("& Value = *(")
								.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
								.Append("*)InValue;\r\n");
						}
						builder
							.Append("\t\tObj->")
							.Append(property.Setter!)
							.Append("(Value);\r\n");
						if (editorOnlyProperty)
						{
							builder.Append("\t#endif // WITH_EDITORONLY_DATA\r\n");
						}
						builder.Append("\t}\r\n");
					}
				}
			}
			return builder;
		}

		private StringBuilder AppendSparseAccessors(StringBuilder builder, UhtClass classObj)
		{
			string[]? sparseDataTypes = classObj.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);
			if (sparseDataTypes == null)
			{
				return builder;
			}

			string[]? baseSparseDataTypes = classObj.SuperClass?.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);
			if (baseSparseDataTypes != null && Enumerable.SequenceEqual(sparseDataTypes, baseSparseDataTypes))
			{
				return builder;
			}

			foreach (string sparseDataType in sparseDataTypes)
			{
				builder.Append('F').Append(sparseDataType).Append("* ").Append(classObj.SourceName).Append("::Get").Append(sparseDataType).Append("() \r\n");
				builder.Append("{ \r\n");
				builder.Append("\treturn static_cast<F").Append(sparseDataType).Append("*>(GetClass()->GetOrCreateSparseClassData()); \r\n");
				builder.Append("} \r\n");

				builder.Append('F').Append(sparseDataType).Append("* ").Append(classObj.SourceName).Append("::Get").Append(sparseDataType).Append("() const \r\n");
				builder.Append("{ \r\n");
				builder.Append("\treturn static_cast<F").Append(sparseDataType).Append("*>(GetClass()->GetOrCreateSparseClassData()); \r\n");
				builder.Append("} \r\n");

				builder.Append("const F").Append(sparseDataType).Append("* ").Append(classObj.SourceName).Append("::Get").Append(sparseDataType).Append("(EGetSparseClassDataMethod GetMethod) const \r\n");
				builder.Append("{ \r\n");
				builder.Append("\treturn static_cast<const F").Append(sparseDataType).Append("*>(GetClass()->GetSparseClassData(GetMethod)); \r\n");
				builder.Append("} \r\n");

				builder.Append("UScriptStruct* ").Append(classObj.SourceName).Append("::StaticGet").Append(sparseDataType).Append("ScriptStruct()\r\n");
				builder.Append("{ \r\n");
				builder.Append("\treturn F").Append(sparseDataType).Append("::StaticStruct(); \r\n");
				builder.Append("} \r\n");
			}
			return builder;
		}

		private StringBuilder AppendInterfaceCallFunctions(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions)
		{
			foreach (UhtFunction function in callbackFunctions)
			{
				builder.Append("\tstatic FName NAME_").Append(function.Outer?.SourceName).Append('_').Append(function.SourceName).Append(" = FName(TEXT(\"").Append(function.EngineName).Append("\"));\r\n");
				string extraParameter = function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const UObject* O" : "UObject* O";
				AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.InterfaceFunctionArgOrRetVal, false, null, extraParameter, 0, "\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tcheck(O != NULL);\r\n");
				builder.Append("\t\tcheck(O->GetClass()->ImplementsInterface(").Append(classObj.SourceName).Append("::StaticClass()));\r\n");
				if (function.Children.Count > 0)
				{
					builder.Append("\t\t").Append(GetEventStructParametersName(classObj, function.StrippedFunctionName)).Append(" Parms;\r\n");
				}
				builder.Append("\t\tUFunction* const Func = O->FindFunction(NAME_").Append(function.Outer?.SourceName).Append('_').Append(function.SourceName).Append(");\r\n");
				builder.Append("\t\tif (Func)\r\n");
				builder.Append("\t\t{\r\n");
				foreach (UhtProperty property in function.ParameterProperties.Span)
				{
					builder.Append("\t\t\tParms.").Append(property.SourceName).Append('=').Append(property.SourceName).Append(";\r\n");
				}
				builder
					.Append("\t\t\t")
					.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const_cast<UObject*>(O)" : "O")
					.Append("->ProcessEvent(Func, ")
					.Append(function.Children.Count > 0 ? "&Parms" : "NULL")
					.Append(");\r\n");
				foreach (UhtProperty property in function.ParameterProperties.Span)
				{
					if (property.PropertyFlags.HasExactFlags(EPropertyFlags.OutParm | EPropertyFlags.ConstParm, EPropertyFlags.OutParm))
					{
						builder.Append("\t\t\t").Append(property.SourceName).Append("=Parms.").Append(property.SourceName).Append(";\r\n");
					}
				}
				builder.Append("\t\t}\r\n");

				// else clause to call back into native if it's a BlueprintNativeEvent
				if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Native))
				{
					builder
						.Append("\t\telse if (auto I = (")
						.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const I" : "I")
						.Append(classObj.EngineName)
						.Append("*)(O->GetNativeInterfaceAddress(U")
						.Append(classObj.EngineName)
						.Append("::StaticClass())))\r\n");
					builder.Append("\t\t{\r\n");
					builder.Append("\t\t\t");
					if (function.HasReturnProperty)
					{
						builder.Append("Parms.ReturnValue = ");
					}
					builder.Append("I->").Append(function.SourceName).Append("_Implementation(");

					bool first = true;
					foreach (UhtProperty property in function.ParameterProperties.Span)
					{
						if (!first)
						{
							builder.Append(',');
						}
						first = false;
						builder.Append(property.SourceName);
					}
					builder.Append(");\r\n");
					builder.Append("\t\t}\r\n");
				}

				if (function.HasReturnProperty)
				{
					builder.Append("\t\treturn Parms.ReturnValue;\r\n");
				}
				builder.Append("\t}\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendSerializer(StringBuilder builder, UhtClass classObj, UhtSerializerArchiveType serializerType, string macroText)
		{
			if (!classObj.SerializerArchiveType.HasAnyFlags(serializerType))
			{
				if (classObj.EnclosingDefine.Length > 0)
				{
					builder.Append("#if ").Append(classObj.EnclosingDefine).Append("\r\n");
				}
				builder.Append('\t').Append(macroText).Append('(').Append(classObj.SourceName).Append(")\r\n");
				if (classObj.EnclosingDefine.Length > 0)
				{
					builder.Append("#endif\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendNativeGeneratedInitCode(StringBuilder builder, UhtClass classObj)
		{
			const string MetaDataParamsName = "Class_MetaDataParams";
			string singletonName = GetSingletonName(classObj, true);
			string staticsName = singletonName + "_Statics";
			string registrationName = $"Z_Registration_Info_UClass_{classObj.SourceName}";
			string[]? sparseDataTypes = classObj.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);

			PropertyMemberContextImpl context = new(CodeGenerator, classObj, classObj.SourceName, staticsName);

			bool hasInterfaces = classObj.Bases.Any(x => x is UhtClass baseClass && baseClass.ClassFlags.HasAnyFlags(EClassFlags.Interface));

			// Collect the functions to be exported
			bool allEditorOnlyFunctions = true;
			List<UhtFunction> sortedFunctions = new();
			foreach (UhtFunction function in classObj.Functions)
			{
				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate))
				{
					allEditorOnlyFunctions &= function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly);
				}
				sortedFunctions.Add(function);
			}
			sortedFunctions.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

			// Output any function
			foreach (UhtFunction function in sortedFunctions)
			{
				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate))
				{
					AppendFunction(builder, function, classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport));
				}
			}

			builder.Append("\tIMPLEMENT_CLASS_NO_AUTO_REGISTRATION(").Append(classObj.SourceName).Append(");\r\n");

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			// simple ::StaticClass wrapper to avoid header, link and DLL hell
			{
				builder.Append("\tUClass* ").Append(GetSingletonName(classObj, false)).Append("()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\treturn ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
				builder.Append("\t}\r\n");
			}

			// Declare the statics object
			{
				builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tstatic UObject* (*const DependentSingletons[])();\r\n");

				if (sortedFunctions.Count != 0)
				{
					builder.AppendBeginEditorOnlyGuard(allEditorOnlyFunctions);
					builder.Append("\t\tstatic const FClassFunctionLinkInfo FuncInfo[];\r\n");
					builder.AppendEndEditorOnlyGuard(allEditorOnlyFunctions);
				}

				builder.AppendMetaDataDecl(classObj, MetaDataParamsName, 2);

				AppendPropertiesDecl(builder, classObj, classObj.SourceName, staticsName, 2);

				if (hasInterfaces)
				{
					builder.Append("\t\tstatic const UECodeGen_Private::FImplementedInterfaceParams InterfaceParams[];\r\n");
				}

				builder.Append("\t\tstatic const FCppClassTypeInfoStatic StaticCppClassTypeInfo;\r\n");

				builder.Append("\t\tstatic const UECodeGen_Private::FClassParams ClassParams;\r\n");

				builder.Append("\t};\r\n");
			}

			// Define the statics object
			{
				builder.Append("\tUObject* (*const ").Append(staticsName).Append("::DependentSingletons[])() = {\r\n");
				if (classObj.SuperClass != null && classObj.SuperClass != classObj)
				{
					builder.Append("\t\t(UObject* (*)())").Append(GetSingletonName(classObj.SuperClass, true)).Append(",\r\n");
				}
				builder.Append("\t\t(UObject* (*)())").Append(GetSingletonName(Package, true)).Append(",\r\n");
				builder.Append("\t};\r\n");

				if (sortedFunctions.Count > 0)
				{
					builder.AppendBeginEditorOnlyGuard(allEditorOnlyFunctions);
					builder.Append("\tconst FClassFunctionLinkInfo ").Append(staticsName).Append("::FuncInfo[] = {\r\n");

					foreach (UhtFunction function in sortedFunctions)
					{
						bool isEditorOnlyFunction = function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly);
						builder.AppendBeginEditorOnlyGuard(isEditorOnlyFunction);
						builder
							.Append("\t\t{ &")
							.Append(GetSingletonName(function, true))
							.Append(", ")
							.AppendUTF8LiteralString(function.EngineName)
							.Append(" },")
							.AppendObjectHash(classObj, context, function)
							.Append("\r\n");
						builder.AppendEndEditorOnlyGuard(isEditorOnlyFunction);
					}

					builder.Append("\t};\r\n");
					builder.AppendEndEditorOnlyGuard(allEditorOnlyFunctions);
				}

				builder.AppendMetaDataDef(classObj, staticsName, MetaDataParamsName, 1);

				AppendPropertiesDefs(builder, classObj, classObj.SourceName, staticsName, 1);

				if (hasInterfaces)
				{
					builder.Append("\t\tconst UECodeGen_Private::FImplementedInterfaceParams ").Append(staticsName).Append("::InterfaceParams[] = {\r\n");

					foreach (UhtStruct structObj in classObj.Bases)
					{
						if (structObj is UhtClass baseClass)
						{
							if (baseClass.ClassFlags.HasAnyFlags(EClassFlags.Interface))
							{
								builder
									.Append("\t\t\t{ ")
									.Append(GetSingletonName(baseClass.AlternateObject, false))
									.Append(", (int32)VTABLE_OFFSET(")
									.Append(classObj.SourceName)
									.Append(", ")
									.Append(baseClass.SourceName)
									.Append("), false }, ")
									.AppendObjectHash(classObj, context, baseClass.AlternateObject)
									.Append("\r\n");
							}
						}
					}

					builder.Append("\t\t};\r\n");
				}

				builder.Append("\tconst FCppClassTypeInfoStatic ").Append(staticsName).Append("::StaticCppClassTypeInfo = {\r\n");
				if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
				{
					builder.Append("\t\tTCppClassTypeTraits<I").Append(classObj.EngineName).Append(">::IsAbstract,\r\n");
				}
				else
				{
					builder.Append("\t\tTCppClassTypeTraits<").Append(classObj.SourceName).Append(">::IsAbstract,\r\n");
				}
				builder.Append("\t};\r\n");

				EClassFlags classFlags = classObj.ClassFlags & EClassFlags.SaveInCompiledInClasses;

				builder.Append("\tconst UECodeGen_Private::FClassParams ").Append(staticsName).Append("::ClassParams = {\r\n");
				builder.Append("\t\t&").Append(classObj.SourceName).Append("::StaticClass,\r\n");
				if (classObj.Config.Length > 0)
				{
					builder.Append("\t\t").AppendUTF8LiteralString(classObj.Config).Append(",\r\n");
				}
				else
				{
					builder.Append("\t\tnullptr,\r\n");
				}
				builder.Append("\t\t&StaticCppClassTypeInfo,\r\n");
				builder.Append("\t\tDependentSingletons,\r\n");
				if (sortedFunctions.Count == 0)
				{
					builder.Append("\t\tnullptr,\r\n");
				}
				else if (allEditorOnlyFunctions)
				{
					builder.Append("\t\tIF_WITH_EDITOR(FuncInfo, nullptr),\r\n");
				}
				else
				{
					builder.Append("\t\tFuncInfo,\r\n");
				}
				builder.AppendPropertiesParamsList(classObj, staticsName, 2, "\r\n");
				builder.Append("\t\t").Append(hasInterfaces ? "InterfaceParams" : "nullptr").Append(",\r\n");
				builder.Append("\t\tUE_ARRAY_COUNT(DependentSingletons),\r\n");
				if (sortedFunctions.Count == 0)
				{
					builder.Append("\t\t0,\r\n");
				}
				else if (allEditorOnlyFunctions)
				{
					builder.Append("\t\tIF_WITH_EDITOR(UE_ARRAY_COUNT(FuncInfo), 0),\r\n");
				}
				else
				{
					builder.Append("\t\tUE_ARRAY_COUNT(FuncInfo),\r\n");
				}
				builder.AppendPropertiesParamsCount(classObj, staticsName, 2, "\r\n");
				builder.Append("\t\t").Append(hasInterfaces ? "UE_ARRAY_COUNT(InterfaceParams)" : "0").Append(",\r\n");
				builder.Append($"\t\t0x{(uint)classFlags:X8}u,\r\n");
				builder.Append("\t\t").AppendMetaDataParams(classObj, staticsName, MetaDataParamsName).Append("\r\n");
				builder.Append("\t};\r\n");
			}

			// Class registration
			{
				builder.Append("\tUClass* ").Append(singletonName).Append("()\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\tUECodeGen_Private::ConstructUClass(").Append(registrationName).Append(".OuterSingleton, ").Append(staticsName).Append("::ClassParams);\r\n");
				if (sparseDataTypes != null)
				{
					foreach (string sparseClass in sparseDataTypes)
					{
						builder.Append("\t\t\t").Append(registrationName).Append(".OuterSingleton->SetSparseClassDataStruct(").Append(classObj.SourceName).Append("::StaticGet").Append(sparseClass).Append("ScriptStruct());\r\n");
					}
				}
				builder.Append("\t\t}\r\n");
				builder.Append("\t\treturn ").Append(registrationName).Append(".OuterSingleton;\r\n");
				builder.Append("\t}\r\n");
			}

			// At this point, we can compute the hash... HOWEVER, in the old UHT extra data is appended to the hash block that isn't emitted to the actual output
			using (BorrowStringBuilder hashBorrower = new(StringBuilderCache.Small))
			{
				StringBuilder hashBuilder = hashBorrower.StringBuilder;
				hashBuilder.Append(builder, hashCodeBlockStart, builder.Length - hashCodeBlockStart);

				int saveLength = hashBuilder.Length;

				// Append base class' hash at the end of the generated code, this will force update derived classes
				// when base class changes during hot-reload.
				uint baseClassHash = 0;
				if (classObj.SuperClass != null && !classObj.SuperClass.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
				{
					baseClassHash = ObjectInfos[classObj.SuperClass.ObjectTypeIndex].Hash;
				}
				hashBuilder.Append($"\r\n// {baseClassHash}\r\n");

				// Append info for the sparse class data struct onto the text to be hashed
				if (sparseDataTypes != null)
				{
					foreach (string sparseDataType in sparseDataTypes)
					{
						UhtType? type = Session.FindType(classObj, UhtFindOptions.ScriptStruct | UhtFindOptions.EngineName, sparseDataType);
						if (type != null)
						{
							hashBuilder.Append(type.EngineName).Append("\r\n");
							for (UhtScriptStruct? sparseStruct = type as UhtScriptStruct; sparseStruct != null; sparseStruct = sparseStruct.SuperScriptStruct)
							{
								foreach (UhtProperty property in sparseStruct.Properties)
								{
									hashBuilder.AppendPropertyText(property, UhtPropertyTextType.SparseShort).Append(' ').Append(property.SourceName).Append("\r\n");
								}
							}
						}
					}
				}

				if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
				{
					builder.Append("\t/* friend declarations for pasting into noexport class ").Append(classObj.SourceName).Append("\r\n");
					builder.Append("\tfriend struct ").Append(staticsName).Append(";\r\n");
					builder.Append("\t*/\r\n");
				}

				if (Session.IncludeDebugOutput)
				{
					using UhtBorrowBuffer borrowBuffer = new(hashBuilder, saveLength, hashBuilder.Length - saveLength);
					builder.Append("#if 0\r\n");
					builder.Append(borrowBuffer.Buffer.Memory);
					builder.Append("#endif\r\n");
				}

				// Calculate generated class initialization code hash so that we know when it changes after hot-reload
				{
					using UhtBorrowBuffer borrowBuffer = new(hashBuilder);
					ObjectInfos[classObj.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
				}
			}

			builder.Append("\ttemplate<> ").Append(PackageApi).Append("UClass* StaticClass<").Append(classObj.SourceName).Append(">()\r\n");
			builder.Append("\t{\r\n");
			builder.Append("\t\treturn ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
			builder.Append("\t}\r\n");

			if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.SelfHasReplicatedProperties))
			{
				builder.Append("\r\n");
				builder.Append("\tvoid ").Append(classObj.SourceName).Append("::ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const\r\n");
				builder.Append("\t{\r\n");

				foreach (UhtProperty property in classObj.Properties)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
					{
						builder.Append("\t\tstatic const FName Name_").Append(property.SourceName).Append("(TEXT(\"").Append(property.SourceName).Append("\"));\r\n");
					}
				}
				builder.Append("\r\n");
				builder.Append("\t\tconst bool bIsValid = true");
				foreach (UhtProperty property in classObj.Properties)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
					{
						if (!property.IsStaticArray)
						{
							builder.Append("\r\n\t\t\t&& Name_").Append(property.SourceName).Append(" == ClassReps[(int32)ENetFields_Private::").Append(property.SourceName).Append("].Property->GetFName()");
						}
						else
						{
							builder.Append("\r\n\t\t\t&& Name_").Append(property.SourceName).Append(" == ClassReps[(int32)ENetFields_Private::").Append(property.SourceName).Append("_STATIC_ARRAY].Property->GetFName()");
						}
					}
				}
				builder.Append(";\r\n");
				builder.Append("\r\n");
				builder.Append("\t\tcheckf(bIsValid, TEXT(\"UHT Generated Rep Indices do not match runtime populated Rep Indices for properties in ").Append(classObj.SourceName).Append("\"));\r\n");
				builder.Append("\t}\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendNatives(StringBuilder builder, UhtClass classObj)
		{
			builder.Append("\tvoid ").Append(classObj.SourceName).Append("::StaticRegisterNatives").Append(classObj.SourceName).Append("()\r\n");
			builder.Append("\t{\r\n");

			bool allEditorOnly = true;

			List<UhtFunction> sortedFunctions = new();
			foreach (UhtFunction function in classObj.Functions)
			{
				if (function.FunctionFlags.HasExactFlags(EFunctionFlags.Native | EFunctionFlags.NetRequest, EFunctionFlags.Native))
				{
					sortedFunctions.Add(function);
					allEditorOnly &= function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly);
				}
			}
			sortedFunctions.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

			if (sortedFunctions.Count != 0)
			{
				{
					using UhtMacroBlockEmitter blockEmitter = new(builder, "WITH_EDITOR", allEditorOnly);
					builder.Append("\t\tUClass* Class = ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
					builder.Append("\t\tstatic const FNameNativePtrPair Funcs[] = {\r\n");

					foreach (UhtFunction function in sortedFunctions)
					{
						blockEmitter.Set(function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly));
						builder
							.Append("\t\t\t{ ")
							.AppendUTF8LiteralString(function.EngineName)
							.Append(", &")
							.AppendClassSourceNameOrInterfaceName(classObj)
							.Append("::exec")
							.Append(function.EngineName)
							.Append(" },\r\n");
					}

					// This will close the block if we have one that isn't editor only
					blockEmitter.Set(allEditorOnly);

					builder.Append("\t\t};\r\n");
					builder.Append("\t\tFNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));\r\n");
				}
			}

			builder.Append("\t}\r\n");
			return builder;
		}

		private StringBuilder AppendCallbackParametersDecls(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions)
		{
			foreach (UhtFunction function in callbackFunctions)
			{
				AppendEventParameter(builder, function, function.StrippedFunctionName, UhtPropertyTextType.EventParameterMember, true, 1, "\r\n");
			}
			return builder;
		}
		private StringBuilder AppendCallbackFunctions(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions)
		{
			if (callbackFunctions.Count > 0)
			{
				bool isInterfaceClass = classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface);
				{
					using UhtMacroBlockEmitter blockEmitter = new(builder, "WITH_EDITOR");
					foreach (UhtFunction function in callbackFunctions)
					{
						// Net response functions don't go into the VM
						if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetResponse))
						{
							continue;
						}

						blockEmitter.Set(function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly));

						if (!isInterfaceClass)
						{
							builder.Append("\tstatic FName NAME_").Append(classObj.SourceName).Append('_').Append(function.EngineName).Append(" = FName(TEXT(\"").Append(function.EngineName).Append("\"));\r\n");
						}

						AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.EventFunctionArgOrRetVal, false, null, null, UhtFunctionExportFlags.None, "\r\n");

						if (isInterfaceClass)
						{
							builder.Append("\t{\r\n");

							// assert if this is ever called directly
							builder
								.Append("\t\tcheck(0 && \"Do not directly call Event functions in Interfaces. Call Execute_")
								.Append(function.EngineName)
								.Append(" instead.\");\r\n");

							// satisfy compiler if it's expecting a return value
							if (function.ReturnProperty != null)
							{
								string eventParmStructName = GetEventStructParametersName(classObj, function.EngineName);
								builder.Append("\t\t").Append(eventParmStructName).Append(" Parms;\r\n");
								builder.Append("\t\treturn Parms.ReturnValue;\r\n");
							}
							builder.Append("\t}\r\n");
						}
						else
						{
							AppendEventFunctionPrologue(builder, function, function.EngineName, 1, "\r\n");

							// Cast away const just in case, because ProcessEvent isn't const
							builder.Append("\t\t");
							if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const))
							{
								builder.Append("const_cast<").Append(classObj.SourceName).Append("*>(this)->");
							}
							builder
								.Append("ProcessEvent(FindFunctionChecked(")
								.Append("NAME_")
								.Append(classObj.SourceName)
								.Append('_')
								.Append(function.EngineName)
								.Append("),")
								.Append(function.Children.Count > 0 ? "&Parms" : "NULL")
								.Append(");\r\n");

							AppendEventFunctionEpilogue(builder, function, 1, "\r\n");
						}
					}
				}
			}
			return builder;
		}

		private static StringBuilder AppendRpcFunctions(StringBuilder builder, UhtClass classObj, List<UhtFunction> reversedFunctions, bool editorOnly)
		{
			foreach (UhtFunction function in reversedFunctions)
			{
				if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly) == editorOnly)
				{
					builder.Append("\tDEFINE_FUNCTION(").AppendClassSourceNameOrInterfaceName(classObj).Append("::").Append(function.UnMarshalAndCallName).Append(")\r\n");
					builder.Append("\t{\r\n");
					AppendFunctionThunk(builder, function);
					builder.Append("\t}\r\n");
				}
			}
			return builder;
		}

		private static StringBuilder AppendFunctionThunk(StringBuilder builder, UhtFunction function)
		{
			// Export the GET macro for the parameters
			foreach (UhtProperty parameter in function.ParameterProperties.Span)
			{
				builder.Append("\t\t").AppendFunctionThunkParameterGet(parameter).Append(";\r\n");
			}

			builder.Append("\t\tP_FINISH;\r\n");
			builder.Append("\t\tP_NATIVE_BEGIN;\r\n");

			// Call the validate function if there is one
			if (!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CppStatic) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetValidate))
			{
				builder.Append("\t\tif (!P_THIS->").Append(function.CppValidationImplName).Append('(').AppendFunctionThunkParameterNames(function).Append("))\r\n");
				builder.Append("\t\t{\r\n");
				builder.Append("\t\t\tRPC_ValidateFailed(TEXT(\"").Append(function.CppValidationImplName).Append("\"));\r\n");
				builder.Append("\t\t\treturn;\r\n");   // If we got here, the validation function check failed
				builder.Append("\t\t}\r\n");
			}

			// Write out the return value
			builder.Append("\t\t");
			UhtProperty? returnProperty = function.ReturnProperty;
			if (returnProperty != null)
			{
				builder.Append("*(").AppendFunctionThunkReturn(returnProperty).Append("*)Z_Param__Result=");
			}

			// Export the call to the C++ version
			if (function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CppStatic))
			{
				builder.Append(function.Outer?.SourceName).Append("::").Append(function.CppImplName).Append('(').AppendFunctionThunkParameterNames(function).Append(");\r\n");
			}
			else
			{
				builder.Append("P_THIS->").Append(function.CppImplName).Append('(').AppendFunctionThunkParameterNames(function).Append(");\r\n");
			}
			builder.Append("\t\tP_NATIVE_END;\r\n");
			return builder;
		}

		private static void FindNoExportStructsRecursive(List<UhtScriptStruct> outScriptStructs, UhtStruct structObj)
		{
			for (UhtStruct? current = structObj; current != null; current = current.SuperStruct)
			{
				// Is isn't true for noexport structs
				if (current is UhtScriptStruct scriptStruct)
				{
					if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
					{
						break;
					}

					// these are a special cases that already exists and if wrong if exported naively
					if (!scriptStruct.IsAlwaysAccessible)
					{
						outScriptStructs.Remove(scriptStruct);
						outScriptStructs.Add(scriptStruct);
					}
				}

				foreach (UhtType type in current.Children)
				{
					if (type is UhtProperty property)
					{
						foreach (UhtType referenceType in property.EnumerateReferencedTypes())
						{
							if (referenceType is UhtScriptStruct propertyScriptStruct)
							{
								FindNoExportStructsRecursive(outScriptStructs, propertyScriptStruct);
							}
						}
					}
				}
			}
		}

		private static List<UhtScriptStruct> FindNoExportStructs(UhtStruct structObj)
		{
			List<UhtScriptStruct> outScriptStructs = new();
			FindNoExportStructsRecursive(outScriptStructs, structObj);
			outScriptStructs.Reverse();
			return outScriptStructs;
		}

		private class PropertyMemberContextImpl : IUhtPropertyMemberContext
		{
			private readonly UhtCodeGenerator _codeGenerator;
			private readonly UhtStruct _outerStruct;
			private readonly string _outerStructSourceName;
			private readonly string _staticsName;

			public PropertyMemberContextImpl(UhtCodeGenerator codeGenerator, UhtStruct outerStruct, string outerStructSourceName, string staticsName)
			{
				_codeGenerator = codeGenerator;
				_outerStruct = outerStruct;
				_staticsName = staticsName;
				_outerStructSourceName = outerStructSourceName.Length == 0 ? outerStruct.SourceName : outerStructSourceName;
			}

			public UhtStruct OuterStruct => _outerStruct;
			public string OuterStructSourceName => _outerStructSourceName;
			public string StaticsName => _staticsName;
			public string NamePrefix => "NewProp_";
			public string MetaDataSuffix => "_MetaData";

			public string GetSingletonName(UhtObject? obj, bool registered)
			{
				return _codeGenerator.GetSingletonName(obj, registered);
			}

			public uint GetTypeHash(UhtObject obj)
			{
				return _codeGenerator.ObjectInfos[obj.ObjectTypeIndex].Hash;
			}
		}
	}

	/// <summary>
	/// Collection of string builder extensions used to generate the cpp files for individual headers.
	/// </summary>
	public static class UhtHeaderCodeGeneratorCppFileStringBuilderExtensinos
	{

		/// <summary>
		/// Append the parameter names for a function
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="function">Function in question</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendFunctionThunkParameterNames(this StringBuilder builder, UhtFunction function)
		{
			bool first = true;
			foreach (UhtProperty parameter in function.ParameterProperties.Span)
			{
				if (!first)
				{
					builder.Append(',');
				}
				builder.AppendFunctionThunkParameterArg(parameter);
				first = false;
			}
			return builder;
		}

		/// <summary>
		/// Append the parameter list and count as arguments to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="structObj">Structure in question</param>
		/// <param name="staticsName">Name of the statics section for this structure</param>
		/// <param name="tabs">Number of tabs to precede the line</param>
		/// <param name="endl">String used to terminate a line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertiesParams(this StringBuilder builder, UhtStruct structObj, string staticsName, int tabs, string endl)
		{
			return builder.AppendPropertiesParamsList(structObj, staticsName, tabs, endl).AppendPropertiesParamsCount(structObj, staticsName, tabs, endl);
		}

		/// <summary>
		/// Append the parameter list as an argument to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="structObj">Structure in question</param>
		/// <param name="staticsName">Name of the statics section for this structure</param>
		/// <param name="tabs">Number of tabs to precede the line</param>
		/// <param name="endl">String used to terminate a line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertiesParamsList(this StringBuilder builder, UhtStruct structObj, string staticsName, int tabs, string endl)
		{
			if (!structObj.Children.Any(x => x is UhtProperty))
			{
				builder.AppendTabs(tabs).Append("nullptr,").Append(endl);
			}
			else if (structObj.Properties.Any(x => !x.IsEditorOnlyProperty))
			{
				builder.AppendTabs(tabs).Append(staticsName).Append("::PropPointers,").Append(endl);
			}
			else
			{
				builder.AppendTabs(tabs).Append("IF_WITH_EDITORONLY_DATA(").Append(staticsName).Append("::PropPointers, nullptr),").Append(endl);
			}
			return builder;
		}

		/// <summary>
		/// Append the parameter count as an argument to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="structObj">Structure in question</param>
		/// <param name="staticsName">Name of the statics section for this structure</param>
		/// <param name="tabs">Number of tabs to precede the line</param>
		/// <param name="endl">String used to terminate a line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendPropertiesParamsCount(this StringBuilder builder, UhtStruct structObj, string staticsName, int tabs, string endl)
		{
			if (!structObj.Children.Any(x => x is UhtProperty))
			{
				builder.AppendTabs(tabs).Append("0,").Append(endl);
			}
			else if (structObj.Properties.Any(x => !x.IsEditorOnlyProperty))
			{
				builder.AppendTabs(tabs).Append("UE_ARRAY_COUNT(").Append(staticsName).Append("::PropPointers),").Append(endl);
			}
			else
			{
				builder.AppendTabs(tabs).Append("IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(").Append(staticsName).Append("::PropPointers), 0),").Append(endl);
			}
			return builder;
		}

		/// <summary>
		/// Append the given array list and count as arguments to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="isEmpty">If true, the array list is empty</param>
		/// <param name="allEditorOnlyData">If true, the array is all editor only data</param>
		/// <param name="staticsName">Name of the statics section</param>
		/// <param name="arrayName">The name of the arrray</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendArray(this StringBuilder builder, bool isEmpty, bool allEditorOnlyData, string staticsName, string arrayName)
		{
			if (isEmpty)
			{
				builder.Append("nullptr, 0");
			}
			else if (allEditorOnlyData)
			{
				builder
					.Append("IF_WITH_EDITORONLY_DATA(")
					.Append(staticsName).Append("::").Append(arrayName)
					.Append(", nullptr), IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(")
					.Append(staticsName).Append("::").Append(arrayName)
					.Append("), 0)");
			}
			else
			{
				builder
					.Append(staticsName).Append("::").Append(arrayName)
					.Append(", UE_ARRAY_COUNT(")
					.Append(staticsName).Append("::").Append(arrayName)
					.Append(')');
			}
			return builder;
		}
	}
}
