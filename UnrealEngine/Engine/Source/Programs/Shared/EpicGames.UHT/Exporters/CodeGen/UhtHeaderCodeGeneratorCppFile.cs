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
	internal struct UhtCodeBlockComment : IDisposable
	{
		private readonly StringBuilder _builder;
		private readonly UhtType? _primaryType = null;
		private readonly UhtType? _secondaryType = null;
		private readonly string? _text = null;

		public UhtCodeBlockComment(StringBuilder builder, string text)
		{
			_builder = builder;
			_text = text;
			builder.Append("\r\n// Begin");
			AppendText();
			_builder.Append("\r\n");
		}

		public UhtCodeBlockComment(StringBuilder builder, UhtType primaryType, UhtType? secondaryType = null)
		{
			_builder = builder;
			_primaryType = primaryType;
			_secondaryType = secondaryType;
			builder.Append("\r\n// Begin");
			AppendText();
			_builder.Append("\r\n");
		}

		public void Dispose()
		{
			_builder.Append("// End");
			AppendText();
			_builder.Append("\r\n");
		}

		private void AppendText()
		{
			if (!String.IsNullOrEmpty(_text))
			{
				_builder.Append(' ').Append(_text);
			}
			if (_primaryType != null)
			{
				_builder.Append(' ').Append(_primaryType.EngineType).Append(' ').Append(_primaryType.SourceName);
			}
			if (_secondaryType != null)
			{
				_builder.Append(' ').Append(_secondaryType.EngineType).Append(' ').Append(_secondaryType.SourceName);
			}
		}
	}

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

							bool requireIncludeForClasses = IsRpcFunction(function) && ShouldExportFunction(function);

							foreach (UhtProperty property in function.Properties)
							{
								AddIncludeForProperty(property, requireIncludeForClasses, addedIncludes, includesToAdd);
							}
						}

						// Properties
						foreach (UhtProperty property in structObj.Properties)
						{
							AddIncludeForProperty(property, false, addedIncludes, includesToAdd);
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
						(int objectIndex, bool registered) => GetExternalDecl(objectIndex, registered));
					using UhtCodeBlockComment blockComment = new(builder, "Cross Module References");
					foreach (string crossReference in sorted.Span)
					{
						builder.Append(crossReference.AsSpan().TrimStart());
					}
				}

				int generatedBodyStart = builder.Length;

				UhtUsedDefineScopes<UhtEnum> enums = new();
				UhtUsedDefineScopes<UhtScriptStruct> scriptStructs = new();
				UhtUsedDefineScopes<UhtClass> classes = new();
				foreach (UhtField field in HeaderFile.References.ExportTypes)
				{
					if (field is UhtEnum enumObj)
					{
						using UhtCodeBlockComment blockComment = new(builder, field);
						AppendEnum(builder, enumObj);
						enums.Add(enumObj);
					}
					else if (field is UhtScriptStruct scriptStruct)
					{
						using UhtCodeBlockComment blockComment = new(builder, field);
						AppendScriptStruct(builder, scriptStruct);
						if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
						{
							scriptStructs.Add(scriptStruct);
						}
					}
					else if (field is UhtFunction function)
					{
						using UhtCodeBlockComment blockComment = new(builder, field);
						AppendDelegate(builder, function);
					}
					else if (field is UhtClass classObj)
					{
						if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
						{
							// Collect the functions to be exported
							UhtUsedDefineScopes<UhtFunction> functions = new(classObj.Functions);
							functions.Instances.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

							// Output any functions
							foreach (UhtFunction classFunction in functions.Instances)
							{
								AppendClassFunction(builder, classObj, classFunction);
							}

							using UhtCodeBlockComment blockComment = new(builder, field);
							AppendClass(builder, classObj, functions);
							classes.Add(classObj);
						}
					}
				}

				if (!enums.IsEmpty || !scriptStructs.IsEmpty || !classes.IsEmpty)
				{
					string name = $"Z_CompiledInDeferFile_{headerInfo.FileId}";
					string staticsName = $"{name}_Statics";

					uint combinedHash = UInt32.MaxValue;

					using UhtCodeBlockComment blockComment = new(builder, "Registration");
					builder.Append("struct ").Append(staticsName).Append("\r\n");
					builder.Append("{\r\n");

					builder.AppendInstances(enums, UhtDefineScopeNames.Standard, 
						builder => builder.Append("\tstatic constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {\r\n"),
						(builder, enumObj) =>
						{
							uint hash = ObjectInfos[enumObj.ObjectTypeIndex].Hash;
							builder
								.Append("\t\t{ ")
								.Append(enumObj.SourceName)
								.Append("_StaticEnum, TEXT(\"")
								.Append(enumObj.EngineName)
								.Append("\"), &Z_Registration_Info_UEnum_")
								.Append(enumObj.EngineName)
								.Append($", CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, {hash}U) }},\r\n");
							combinedHash = HashCombine(combinedHash, hash);
						},
						builder => builder.Append("\t};\r\n"));

					builder.AppendInstances(scriptStructs, UhtDefineScopeNames.Standard,
						builder => builder.Append("\tstatic constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {\r\n"),
						(builder, scriptStruct) =>
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
						},
						builder => builder.Append("\t};\r\n"));

					builder.AppendInstances(classes, UhtDefineScopeNames.Standard,
						builder => builder.Append("\tstatic constexpr FClassRegisterCompiledInInfo ClassInfo[] = {\r\n"),
						(builder, classObj) =>
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
						},
						builder => builder.Append("\t};\r\n"));

					builder.Append("};\r\n");

					builder
						.Append("static FRegisterCompiledInInfo ")
						.Append(name)
						.Append($"_{combinedHash}(TEXT(\"")
						.Append(Package.EngineName)
						.Append("\"),\r\n");

					builder.AppendArrayPtrAndCountLine(classes, UhtDefineScopeNames.Standard, staticsName, "ClassInfo", 1, ",\r\n");
					builder.AppendArrayPtrAndCountLine(scriptStructs, UhtDefineScopeNames.Standard, staticsName, "ScriptStructInfo", 1, ",\r\n");
					builder.AppendArrayPtrAndCountLine(enums, UhtDefineScopeNames.Standard, staticsName, "EnumInfo", 1, ");\r\n");
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
					using UhtRentedPoolBuffer<char> borrowBuffer = builder.RentPoolBuffer();
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

		private void AddIncludeForType(UhtProperty uhtProperty, bool requireIncludeForClasses, HashSet<UhtHeaderFile> addedIncludes, IList<string> includesToAdd)
		{
			if (uhtProperty is UhtStructProperty structProperty)
			{
				UhtScriptStruct scriptStruct = structProperty.ScriptStruct;
				if (!scriptStruct.HeaderFile.IsNoExportTypes && addedIncludes.Add(scriptStruct.HeaderFile))
				{
					includesToAdd.Add(HeaderInfos[scriptStruct.HeaderFile.HeaderFileTypeIndex].IncludePath);
				}
			}
			else if (requireIncludeForClasses && uhtProperty is UhtClassProperty classProperty)
			{
				UhtClass uhtClass = classProperty.Class;
				if (!uhtClass.HeaderFile.IsNoExportTypes && addedIncludes.Add(uhtClass.HeaderFile))
				{
					includesToAdd.Add(HeaderInfos[uhtClass.HeaderFile.HeaderFileTypeIndex].IncludePath);
				}
			}
		}

		private void AddIncludeForProperty(UhtProperty property, bool requireIncludeForClasses, HashSet<UhtHeaderFile> addedIncludes, IList<string> includesToAdd)
		{
			AddIncludeForType(property, requireIncludeForClasses, addedIncludes, includesToAdd);

			if (property is UhtContainerBaseProperty containerProperty)
			{
				AddIncludeForType(containerProperty.ValueProperty, false, addedIncludes, includesToAdd);
			}

			if (property is UhtMapProperty mapProperty)
			{
				AddIncludeForType(mapProperty.KeyProperty, false, addedIncludes, includesToAdd);
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

			using (UhtMacroBlockEmitter macroBlockEmitter = new(builder, UhtDefineScopeNames.Standard, enumObj.DefineScope))
			{

				// If we don't have a zero 0 then we emit a static assert to verify we have one
				if (!enumObj.IsValidEnumValue(0) && enumObj.MetaData.ContainsKey(UhtNames.BlueprintType))
				{
					bool hasUnparsedValue = enumObj.EnumValues.Exists(x => x.Value == -1);
					if (hasUnparsedValue)
					{
						builder.Append("static_assert(");
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

				builder.Append("static FEnumRegistrationInfo ").Append(registrationName).Append(";\r\n");
				builder.Append("static UEnum* ").Append(enumObj.SourceName).Append("_StaticEnum()\r\n");
				builder.Append("{\r\n");

				builder.Append("\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\t").Append(registrationName).Append(".OuterSingleton = GetStaticEnum(").Append(singletonName).Append(", (UObject*)")
					.Append(PackageSingletonName).Append("(), TEXT(\"").Append(enumObj.SourceName).Append("\"));\r\n");
				builder.Append("\t}\r\n");
				builder.Append("\treturn ").Append(registrationName).Append(".OuterSingleton;\r\n");

				builder.Append("}\r\n");

				builder.Append("template<> ").Append(PackageApi).Append("UEnum* StaticEnum<").Append(enumObj.CppType).Append(">()\r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn ").Append(enumObj.SourceName).Append("_StaticEnum();\r\n");
				builder.Append("}\r\n");

				// Everything from this point on will be part of the definition hash
				int hashCodeBlockStart = builder.Length;

				// Statics declaration
				{
					builder.Append("struct ").Append(staticsName).Append("\r\n");
					builder.Append("{\r\n");
					builder.AppendMetaDataDecl(enumObj, null, null, MetaDataParamsName, 1);

					// Enumerators
					builder.Append("\tstatic constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {\r\n");
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
					builder.Append("\tstatic const UECodeGen_Private::FEnumParams EnumParams;\r\n");
					builder.Append("};\r\n");
				}

				// Statics definition
				{
					builder.Append("const UECodeGen_Private::FEnumParams ").Append(staticsName).Append("::EnumParams = {\r\n");
					builder.Append("\t(UObject*(*)())").Append(PackageSingletonName).Append(",\r\n");
					builder.Append('\t').Append(enumDisplayNameFn).Append(",\r\n");
					builder.Append('\t').AppendUTF8LiteralString(enumObj.SourceName).Append(",\r\n");
					builder.Append('\t').AppendUTF8LiteralString(enumObj.CppType).Append(",\r\n");
					builder.Append('\t').Append(staticsName).Append("::Enumerators,\r\n");
					builder.Append('\t').Append(ObjectFlags).Append(",\r\n");
					builder.Append("\tUE_ARRAY_COUNT(").Append(staticsName).Append("::Enumerators),\r\n");
					builder.Append('\t').Append(enumObj.EnumFlags.HasAnyFlags(EEnumFlags.Flags) ? "EEnumFlags::Flags" : "EEnumFlags::None").Append(",\r\n");
					builder.Append("\t(uint8)UEnum::ECppForm::").Append(enumObj.CppForm.ToString()).Append(",\r\n");
					builder.Append('\t').AppendMetaDataParams(enumObj, staticsName, MetaDataParamsName).Append("\r\n");
					builder.Append("};\r\n");
				}

				// Registration singleton
				builder.Append("UEnum* ").Append(singletonName).Append("()\r\n");
				builder.Append("{\r\n");
				builder.Append("\tif (!").Append(registrationName).Append(".InnerSingleton)\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tUECodeGen_Private::ConstructUEnum(").Append(registrationName).Append(".InnerSingleton, ").Append(staticsName).Append("::EnumParams);\r\n");
				builder.Append("\t}\r\n");
				builder.Append("\treturn ").Append(registrationName).Append(".InnerSingleton;\r\n");
				builder.Append("}\r\n");

				{
					using UhtRentedPoolBuffer<char> borrowBuffer = builder.RentPoolBuffer(hashCodeBlockStart, builder.Length - hashCodeBlockStart);
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
					builder.Append("static_assert(std::is_polymorphic<")
						.Append(scriptStruct.SourceName)
						.Append(">() == std::is_polymorphic<")
						.Append(scriptStruct.SuperScriptStruct.SourceName)
						.Append(">(), \"USTRUCT ")
						.Append(scriptStruct.SourceName)
						.Append(" cannot be polymorphic unless super ")
						.Append(scriptStruct.SuperScriptStruct.SourceName)
						.Append(" is polymorphic\");\r\n");
				}

				// Outer singleton
				builder.Append("static FStructRegistrationInfo ").Append(registrationName).Append(";\r\n");
				builder.Append("class UScriptStruct* ").Append(scriptStruct.SourceName).Append("::StaticStruct()\r\n");
				builder.Append("{\r\n");
				builder.Append("\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\t")
					.Append(registrationName)
					.Append(".OuterSingleton = GetStaticStruct(")
					.Append(singletonName)
					.Append(", (UObject*)")
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
						if (methodInfo.IsPredicate)
						{
							builder.Append("\t\tTArray<FRigVMFunctionArgument> ").AppendArgumentsName(scriptStruct, methodInfo).Append(";\r\n");
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
								.Append("\t\t")
								.AppendArgumentsName(scriptStruct, methodInfo)
								.Append(".Emplace(TEXT(\"Return\"), TEXT(\"")
								.Append(methodInfo.ReturnType)
								.Append("\"), ERigVMFunctionArgumentDirection::Output);\r\n");
							
							builder
								.Append("\t\tFRigVMRegistry::Get().RegisterPredicate(")
								.Append(registrationName)
								.Append(".OuterSingleton, ")
								.Append("TEXT(\"")
								.Append(methodInfo.Name)
								.Append("\"), ")
								.AppendArgumentsName(scriptStruct, methodInfo)
								.Append(");\r\n");
						}
						else
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
					// The preprocessor conditional is written here instead of in FastArraySerializerImplementation.h
					// since it may evaluate differently in different modules, triggering warnings in IncludeTool.
					builder.Append("#if defined(UE_NET_HAS_IRIS_FASTARRAY_BINDING) && UE_NET_HAS_IRIS_FASTARRAY_BINDING\r\n");
					builder.Append("UE_NET_IMPLEMENT_FASTARRAY(").Append(scriptStruct.SourceName).Append(");\r\n");
					builder.Append("#else\r\n");
					builder.Append("UE_NET_IMPLEMENT_FASTARRAY_STUB(").Append(scriptStruct.SourceName).Append(");\r\n");
					builder.Append("#endif\r\n");
				}
			}

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			// Collect the properties
			PropertyMemberContextImpl context = new(CodeGenerator, scriptStruct, scriptStruct.SourceName, staticsName);
			UhtUsedDefineScopes<UhtProperty> properties = new(scriptStruct.Properties);

			// Declare the statics structure
			{
				builder.Append("struct ").Append(staticsName).Append("\r\n");
				builder.Append("{\r\n");

				foreach (UhtScriptStruct noExportStruct in noExportStructs)
				{
					AppendMirrorsForNoexportStruct(builder, noExportStruct, 1);
					builder.Append("\tstatic_assert(sizeof(").Append(noExportStruct.SourceName).Append(") < MAX_uint16);\r\n");
					builder.Append("\tstatic_assert(alignof(").Append(noExportStruct.SourceName).Append(") < MAX_uint8);\r\n");
				}

				// Meta data
				builder.AppendMetaDataDecl(scriptStruct, context, properties, MetaDataParamsName, 1);

				// Properties
				AppendPropertiesDecl(builder, context, properties, 1);

				// New struct ops
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					builder.Append("\tstatic void* NewStructOps()\r\n");
					builder.Append("\t{\r\n");
					builder.Append("\t\treturn (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<").Append(scriptStruct.SourceName).Append(">();\r\n");
					builder.Append("\t}\r\n");
				}
				builder.Append("\tstatic const UECodeGen_Private::FStructParams StructParams;\r\n");
				builder.Append("};\r\n");
			}

			// Populate the elements of the static structure
			{
				AppendPropertiesDefs(builder, context, properties, 0);

				builder.Append("const UECodeGen_Private::FStructParams ").Append(staticsName).Append("::StructParams = {\r\n");
				builder.Append("\t(UObject* (*)())").Append(PackageSingletonName).Append(",\r\n");
				builder.Append('\t').Append(GetSingletonName(scriptStruct.SuperScriptStruct, true)).Append(",\r\n");
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					builder.Append("\t&NewStructOps,\r\n");
				}
				else
				{
					builder.Append("\tnullptr,\r\n");
				}
				builder.Append('\t').AppendUTF8LiteralString(scriptStruct.EngineName).Append(",\r\n");
				builder.AppendArrayPtrLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 1, ",\r\n");
				builder.AppendArrayCountLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 1, ",\r\n");
				builder.Append("\tsizeof(").Append(scriptStruct.SourceName).Append("),\r\n");
				builder.Append("\talignof(").Append(scriptStruct.SourceName).Append("),\r\n");
				builder.Append("\tRF_Public|RF_Transient|RF_MarkAsNative,\r\n");
				builder.Append($"\tEStructFlags(0x{(uint)(scriptStruct.ScriptStructFlags & ~EStructFlags.ComputedFlags):X8}),\r\n");
				builder.Append('\t').AppendMetaDataParams(scriptStruct, staticsName, MetaDataParamsName).Append("\r\n");
				builder.Append("};\r\n");
			}

			// Generate the registration function
			{
				builder.Append("UScriptStruct* ").Append(singletonName).Append("()\r\n");
				builder.Append("{\r\n");
				string innerSingletonName;
				if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
				{
					innerSingletonName = $"{registrationName}.InnerSingleton";
				}
				else
				{
					builder.Append("\tstatic UScriptStruct* ReturnStruct = nullptr;\r\n");
					innerSingletonName = "ReturnStruct";
				}
				builder.Append("\tif (!").Append(innerSingletonName).Append(")\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tUECodeGen_Private::ConstructUScriptStruct(").Append(innerSingletonName).Append(", ").Append(staticsName).Append("::StructParams);\r\n");
				builder.Append("\t}\r\n");
				builder.Append("\treturn ").Append(innerSingletonName).Append(";\r\n");
				builder.Append("}\r\n");
			}

			using (UhtRentedPoolBuffer<char> borrowBuffer = builder.RentPoolBuffer(hashCodeBlockStart, builder.Length - hashCodeBlockStart))
			{
				ObjectInfos[scriptStruct.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
			}

			// if this struct has RigVM methods we need to implement both the 
			// virtual function as well as the stub method here.
			// The static method is implemented by the user using a macro.
			if (scriptStruct.RigVMStructInfo != null)
			{
				string constPrefix = "";
				if (String.IsNullOrEmpty(scriptStruct.RigVMStructInfo.ExecuteContextMember))
				{
					constPrefix = "const ";
				}
				
				foreach (UhtRigVMMethodInfo methodInfo in scriptStruct.RigVMStructInfo.Methods)
				{
					if (methodInfo.IsPredicate)
					{
						continue;
					}
					
					builder
						.Append(methodInfo.ReturnType)
						.Append(' ')
						.Append(scriptStruct.SourceName)
						.Append("::")
						.Append(methodInfo.Name)
						.Append("()\r\n");
					builder.Append("{\r\n");
					if (String.IsNullOrEmpty(scriptStruct.RigVMStructInfo.ExecuteContextMember))
					{
						builder.Append('\t').Append(scriptStruct.RigVMStructInfo.ExecuteContextType).Append(" TemporaryExecuteContext;\r\n");
					}
					else
					{
						builder.Append('\t').Append(scriptStruct.RigVMStructInfo.ExecuteContextType).Append("& TemporaryExecuteContext = ").Append(scriptStruct.RigVMStructInfo.ExecuteContextMember).Append(";\r\n");
					}

					builder.Append("\tTemporaryExecuteContext.Initialize();\r\n");
					builder.Append('\t');
					if (methodInfo.ReturnType != "void")
					{
						builder.Append("return ");
					}
					builder
						.Append(methodInfo.Name)
						.Append("(TemporaryExecuteContext);\r\n")
						.Append("}\r\n");

					builder
						.Append(methodInfo.ReturnType)
						.Append(' ')
						.Append(scriptStruct.SourceName)
						.Append("::")
						.Append(methodInfo.Name)
						.Append('(')
						.Append(constPrefix)
						.Append(scriptStruct.RigVMStructInfo.ExecuteContextType)
						.Append("& InExecuteContext)\r\n");
					builder.Append("{\r\n");

					foreach (UhtRigVMParameter parameter in scriptStruct.RigVMStructInfo.Members)
					{
						if (!parameter.RequiresCast())
						{
							continue;
						}
						builder.Append('\t').Append(parameter.CastType).Append(' ').Append(parameter.CastName).Append('(').Append(parameter.Name).Append(");\r\n");
					}
					
					foreach (UhtRigVMMethodInfo predicateInfo in scriptStruct.RigVMStructInfo.Methods)
					{
						if (predicateInfo.IsPredicate)
						{
							builder.Append('\t').Append(predicateInfo.Name).Append("Struct ").Append(predicateInfo.Name).Append("Predicate; \r\n");
						}
					}

					builder.Append('\t').Append(methodInfo.ReturnPrefix()).Append("Static").Append(methodInfo.Name).Append("(\r\n");
					builder.Append("\t\tInExecuteContext");
					builder.AppendParameterNames(scriptStruct.RigVMStructInfo.Members, true, ",\r\n\t\t", true);
					
					foreach (UhtRigVMMethodInfo predicateInfo in scriptStruct.RigVMStructInfo.Methods)
					{
						if (predicateInfo.IsPredicate)
						{
							builder.Append(", \r\n\t\t").Append(predicateInfo.Name).Append("Predicate");
						}
					}
					
					builder.Append("\r\n");
					builder.Append("\t);\r\n");
					builder.Append("}\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendDelegate(StringBuilder builder, UhtFunction function)
		{
			AppendFunction(builder, function, false);

			int tabs = 0;
			string strippedFunctionName = function.StrippedFunctionName;
			string exportFunctionName = GetDelegateFunctionExportName(function);
			string extraParameter = GetDelegateFunctionExtraParameter(function);

			AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.EventFunctionArgOrRetVal, false, exportFunctionName, extraParameter, UhtFunctionExportFlags.None, 0, "\r\n");
			AppendEventFunctionPrologue(builder, function, strippedFunctionName, tabs, "\r\n", true);
			builder
				.Append('\t')
				.Append(strippedFunctionName)
				.Append('.')
				.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.MulticastDelegate) ? "ProcessMulticastDelegate" : "ProcessDelegate")
				.Append("<UObject>(")
				.Append(function.Children.Count > 0 ? "&Parms" : "NULL")
				.Append(");\r\n");
			AppendEventFunctionEpilogue(builder, function, tabs, "\r\n");

			return builder;
		}

		private StringBuilder AppendFunction(StringBuilder builder, UhtFunction function, bool isNoExport)
		{
			const string MetaDataParamsName = "Function_MetaDataParams";
			string singletonName = GetSingletonName(function, true);
			string staticsName = singletonName + "_Statics";
			bool paramsInStatic = isNoExport || !IsCallbackFunction(function);
			bool isNet = function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetRequest | EFunctionFlags.NetResponse);

			string strippedFunctionName = function.StrippedFunctionName;
			string eventParameters = GetEventStructParametersName(function.Outer, strippedFunctionName);

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			// Note: This is fairly agressive 
			using (UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.WithEditor, function.DefineScope))
			{

				// Collect the properties
				PropertyMemberContextImpl context = new(CodeGenerator, function, eventParameters, staticsName);
				UhtUsedDefineScopes<UhtProperty> properties = new(function.Properties);

				// Statics declaration
				{
					builder.Append("struct ").Append(staticsName).Append("\r\n");
					builder.Append("{\r\n");

					if (paramsInStatic)
					{
						List<UhtScriptStruct> noExportStructs = FindNoExportStructs(function);
						foreach (UhtScriptStruct noExportStruct in noExportStructs)
						{
							AppendMirrorsForNoexportStruct(builder, noExportStruct, 1);
						}
						AppendEventParameter(builder, function, strippedFunctionName, UhtPropertyTextType.EventParameterFunctionMember, false, 1, "\r\n");
					}

					builder.AppendMetaDataDecl(function, context, properties, MetaDataParamsName, 1);

					AppendPropertiesDecl(builder, context, properties, 1);

					builder.Append("\tstatic const UECodeGen_Private::FFunctionParams FuncParams;\r\n");

					builder.Append("};\r\n");
				}

				// Statics definition
				{
					AppendPropertiesDefs(builder, context, properties, 0);

					builder
						.Append("const UECodeGen_Private::FFunctionParams ")
						.Append(staticsName).Append("::FuncParams = { ")
						.Append("(UObject*(*)())").Append(GetFunctionOuterFunc(function)).Append(", ")
						.Append(GetSingletonName(function.SuperFunction, true)).Append(", ")
						.AppendUTF8LiteralString(function.EngineName).Append(", ")
						.AppendUTF8LiteralString(function.FunctionType == UhtFunctionType.SparseDelegate, function.SparseOwningClassName).Append(", ")
						.AppendUTF8LiteralString(function.FunctionType == UhtFunctionType.SparseDelegate, function.SparseDelegateName).Append(", ");

					builder.AppendArrayPtrLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 0, ", ");
					builder.AppendArrayCountLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 0, ", ");

					string sizeOfStatic = "";
					if (function.Children.Count > 0)
					{
						UhtFunction tempFunction = function;
						while (tempFunction.SuperFunction != null)
						{
							tempFunction = tempFunction.SuperFunction;
						}

						if (paramsInStatic)
						{
							sizeOfStatic = "sizeof(" + staticsName + "::" + GetEventStructParametersName(tempFunction.Outer, tempFunction.StrippedFunctionName) + ")";
						}
						else
						{
							sizeOfStatic = "sizeof(" + GetEventStructParametersName(tempFunction.Outer, tempFunction.StrippedFunctionName) + ")";
						}

						builder.Append(sizeOfStatic);
					}
					else
					{
						builder.Append('0');
					}
					builder.Append(", ");

					builder
						.Append("RF_Public|RF_Transient|RF_MarkAsNative, ")
						.Append($"(EFunctionFlags)0x{(uint)function.FunctionFlags:X8}, ")
						.Append(isNet ? function.RPCId : 0).Append(", ")
						.Append(isNet ? function.RPCResponseId : 0).Append(", ")
						.AppendMetaDataParams(function, staticsName, MetaDataParamsName)
						.Append(" };\r\n");

					if (!String.IsNullOrEmpty(sizeOfStatic))
					{
						builder.Append("static_assert(").Append(sizeOfStatic).Append(" < MAX_uint16);\r\n");
					}
				}

				// Registration function
				{
					builder.Append("UFunction* ").Append(singletonName).Append("()\r\n");
					builder.Append("{\r\n");
					builder.Append("\tstatic UFunction* ReturnFunction = nullptr;\r\n");
					builder.Append("\tif (!ReturnFunction)\r\n");
					builder.Append("\t{\r\n");
					builder.Append("\t\tUECodeGen_Private::ConstructUFunction(&ReturnFunction, ").Append(staticsName).Append("::FuncParams);\r\n");
					builder.Append("\t}\r\n");
					builder.Append("\treturn ReturnFunction;\r\n");
					builder.Append("}\r\n");
				}
			}

			using (UhtRentedPoolBuffer<char> borrowBuffer = builder.RentPoolBuffer(hashCodeBlockStart, builder.Length - hashCodeBlockStart))
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
			using (UhtMacroBlockEmitter emitter = new(builder, UhtDefineScopeNames.Standard, UhtDefineScope.None))
			{
				foreach (UhtProperty property in scriptStruct.Properties)
				{
					emitter.Set(property.DefineScope);
					builder.AppendTabs(tabs).AppendFullDecl(property, UhtPropertyTextType.ExportMember, false).Append(";\r\n");
				}
			}
			return builder;
		}

		private static StringBuilder AppendPropertiesDecl(StringBuilder builder, IUhtPropertyMemberContext context, UhtUsedDefineScopes<UhtProperty> properties, int tabs)
		{
			if (properties.IsEmpty)
			{
				return builder;
			}
			builder.AppendInstances(properties, UhtDefineScopeNames.Standard,
				builder => { },
				(builder, property) => builder.AppendMemberDecl(property, context, property.EngineName, "", tabs),
				builder => builder.AppendTabs(tabs).Append("static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];\r\n"));
			return builder;
		}

		private static StringBuilder AppendPropertiesDefs(StringBuilder builder, IUhtPropertyMemberContext context, UhtUsedDefineScopes<UhtProperty> properties, int tabs)
		{
			if (properties.IsEmpty)
			{
				return builder;
			}
			builder.AppendInstances(properties, UhtDefineScopeNames.Standard,
				(builder, property) => builder.AppendMemberDef(property, context, property.EngineName, "", null, tabs));

			builder.AppendInstances(properties, UhtDefineScopeNames.Standard,
				builder => builder.AppendTabs(tabs).Append("const UECodeGen_Private::FPropertyParamsBase* const ").Append(context.StaticsName).Append("::PropPointers[] = {\r\n"),
				(builder, property) => builder.AppendMemberPtr(property, context, property.EngineName, "", tabs + 1),
				builder =>
				{
					builder.AppendTabs(tabs).Append("};\r\n");
					builder.AppendTabs(tabs).Append("static_assert(UE_ARRAY_COUNT(").Append(context.StaticsName).Append("::PropPointers) < 2048);\r\n");
				});
			return builder;
		}

		private StringBuilder AppendClassFunction(StringBuilder builder, UhtClass classObj, UhtFunction function)
		{
			bool isNotDelegate = !function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate);
			bool isRpc = IsRpcFunction(function) && ShouldExportFunction(function);
			bool isCallback = IsCallbackFunction(function);
			if (isNotDelegate || isRpc || isCallback)
			{
				using UhtCodeBlockComment blockCommand = new(builder, classObj, function);
				if (isCallback)
				{
					AppendEventParameter(builder, function, function.StrippedFunctionName, UhtPropertyTextType.EventParameterMember, true, 0, "\r\n");
					AppendClassFunctionCallback(builder, classObj, function);
					if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
					{
						AppendInterfaceCallFunction(builder, classObj, function);
					}
				}
				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate))
				{
					AppendFunction(builder, function, classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport));
				}
				if (isRpc)
				{
					using UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.WithEditor, function.DefineScope);
					builder.Append("DEFINE_FUNCTION(").AppendClassSourceNameOrInterfaceName(classObj).Append("::").Append(function.UnMarshalAndCallName).Append(")\r\n");
					builder.Append("{\r\n");
					AppendFunctionThunk(builder, function);
					builder.Append("}\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendClassFunctionCallback(StringBuilder builder, UhtClass classObj, UhtFunction function)
		{
			// Net response functions don't go into the VM
			if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport) || function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetResponse))
			{
				return builder;

			}

			bool isInterfaceClass = classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface);
			using UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.WithEditor, function.DefineScope);

			if (!isInterfaceClass)
			{
				builder.Append("static FName NAME_").Append(classObj.SourceName).Append('_').Append(function.EngineName).Append(" = FName(TEXT(\"").Append(function.EngineName).Append("\"));\r\n");
			}

			AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.EventFunctionArgOrRetVal, false, null, null, UhtFunctionExportFlags.None, 0, "\r\n");

			if (isInterfaceClass)
			{
				builder.Append("{\r\n");

				// assert if this is ever called directly
				builder
					.Append("\tcheck(0 && \"Do not directly call Event functions in Interfaces. Call Execute_")
					.Append(function.EngineName)
					.Append(" instead.\");\r\n");

				// satisfy compiler if it's expecting a return value
				if (function.ReturnProperty != null)
				{
					string eventParmStructName = GetEventStructParametersName(classObj, function.EngineName);
					builder.Append('\t').Append(eventParmStructName).Append(" Parms;\r\n");
					builder.Append("\treturn Parms.ReturnValue;\r\n");
				}
				builder.Append("}\r\n");
			}
			else
			{
				AppendEventFunctionPrologue(builder, function, function.EngineName, 0, "\r\n", false);

				// Cast away const just in case, because ProcessEvent isn't const
				builder.Append('\t');
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

				AppendEventFunctionEpilogue(builder, function, 0, "\r\n");
			}
			return builder;
		}

		private StringBuilder AppendInterfaceCallFunction(StringBuilder builder, UhtClass classObj, UhtFunction function)
		{
			builder.Append("static FName NAME_").Append(function.Outer?.SourceName).Append('_').Append(function.SourceName).Append(" = FName(TEXT(\"").Append(function.EngineName).Append("\"));\r\n");
			string extraParameter = function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const UObject* O" : "UObject* O";
			AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.InterfaceFunctionArgOrRetVal, false, null, extraParameter, UhtFunctionExportFlags.None, 0, "\r\n");
			builder.Append("{\r\n");
			builder.Append("\tcheck(O != NULL);\r\n");
			builder.Append("\tcheck(O->GetClass()->ImplementsInterface(").Append(classObj.SourceName).Append("::StaticClass()));\r\n");
			if (function.Children.Count > 0)
			{
				builder.Append('\t').Append(GetEventStructParametersName(classObj, function.StrippedFunctionName)).Append(" Parms;\r\n");
			}
			builder.Append("\tUFunction* const Func = O->FindFunction(NAME_").Append(function.Outer?.SourceName).Append('_').Append(function.SourceName).Append(");\r\n");
			builder.Append("\tif (Func)\r\n");
			builder.Append("\t{\r\n");
			foreach (UhtType parameter in function.ParameterProperties.Span)
			{
				if (parameter is UhtProperty property)
				{
					builder.Append("\t\tParms.").Append(property.SourceName).Append('=').Append(property.SourceName).Append(";\r\n");
				}
			}
			builder
				.Append("\t\t")
				.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const_cast<UObject*>(O)" : "O")
				.Append("->ProcessEvent(Func, ")
				.Append(function.Children.Count > 0 ? "&Parms" : "NULL")
				.Append(");\r\n");
			foreach (UhtType parameter in function.ParameterProperties.Span)
			{
				if (parameter is UhtProperty property)
				{
					if (property.PropertyFlags.HasExactFlags(EPropertyFlags.OutParm | EPropertyFlags.ConstParm, EPropertyFlags.OutParm))
					{
						builder.Append("\t\t").Append(property.SourceName).Append("=Parms.").Append(property.SourceName).Append(";\r\n");
					}
				}
			}
			builder.Append("\t}\r\n");

			// else clause to call back into native if it's a BlueprintNativeEvent
			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Native))
			{
				builder
					.Append("\telse if (auto I = (")
					.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? "const I" : "I")
					.Append(classObj.EngineName)
					.Append("*)(O->GetNativeInterfaceAddress(U")
					.Append(classObj.EngineName)
					.Append("::StaticClass())))\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\t");
				if (function.HasReturnProperty)
				{
					builder.Append("Parms.ReturnValue = ");
				}
				builder.Append("I->").Append(function.SourceName).Append("_Implementation(");

				bool first = true;
				foreach (UhtType parameter in function.ParameterProperties.Span)
				{
					if (parameter is UhtProperty property)
					{
						if (!first)
						{
							builder.Append(',');
						}
						first = false;
						builder.Append(property.SourceName);
					}
				}
				builder.Append(");\r\n");
				builder.Append("\t}\r\n");
			}

			if (function.HasReturnProperty)
			{
				builder.Append("\treturn Parms.ReturnValue;\r\n");
			}
			builder.Append("}\r\n");
			return builder;
		}

		private StringBuilder AppendClass(StringBuilder builder, UhtClass classObj, UhtUsedDefineScopes<UhtFunction> functions)
		{
			// Add the auto getters/setters
			AppendAutoGettersSetters(builder, classObj);

			// Add the accessors
			AppendPropertyAccessors(builder, classObj);

			// Add sparse accessors
			AppendSparseAccessors(builder, classObj);

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
			{
				AppendNatives(builder, classObj);
			}

			AppendNativeGeneratedInitCode(builder, classObj, functions);

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasConstructor))
			{
				if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.UsesGeneratedBodyLegacy))
				{
					switch (GetConstructorType(classObj))
					{
						case ConstructorType.ObjectInitializer:
							builder.Append(classObj.SourceName).Append("::").Append(classObj.SourceName).Append("(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}\r\n");
							break;

						case ConstructorType.Default:
							builder.Append(classObj.SourceName).Append("::").Append(classObj.SourceName).Append("() {}\r\n");
							break;
					}
				}
			}

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasCustomVTableHelperConstructor))
			{
				builder.Append("DEFINE_VTABLE_PTR_HELPER_CTOR(").Append(classObj.SourceName).Append(");\r\n");
			}

			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasDestructor))
			{
				builder.Append(classObj.SourceName).Append("::~").Append(classObj.SourceName).Append("() {}\r\n");
			}

			AppendFieldNotify(builder, classObj);

			// Only write out adapters if the user has provided one or the other of the Serialize overloads
			if (classObj.SerializerArchiveType != UhtSerializerArchiveType.None && classObj.SerializerArchiveType != UhtSerializerArchiveType.All)
			{
				AppendSerializer(builder, classObj, UhtSerializerArchiveType.Archive, "IMPLEMENT_FARCHIVE_SERIALIZER");
				AppendSerializer(builder, classObj, UhtSerializerArchiveType.StructuredArchiveRecord, "IMPLEMENT_FSTRUCTUREDARCHIVE_SERIALIZER");
			}
			return builder;
		}

		private static StringBuilder AppendFieldNotify(StringBuilder builder, UhtClass classObj)
		{
			if (!NeedFieldNotifyCodeGen(classObj))
			{
				return builder;
			}

			UhtUsedDefineScopes<UhtType> notifyTypes = GetFieldNotifyTypes(classObj);

			//UE_FIELD_NOTIFICATION_DECLARE_FIELD
			builder.AppendInstances(notifyTypes, UhtDefineScopeNames.Standard,
				(builder, notifyType) =>
				{
					builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD({classObj.SourceName}, {GetNotifyTypeName(notifyType)})\r\n");
				});

			//UE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD
			builder.AppendInstances(notifyTypes, UhtDefineScopeNames.Standard,
				builder => builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_BEGIN({classObj.SourceName})\r\n"),
				(builder, notifyType) =>
				{
					builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENT_ENUM_FIELD({classObj.SourceName}, {GetNotifyTypeName(notifyType)})\r\n");
				}, 
				builder => builder.Append($"\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_END({classObj.SourceName});\r\n"));

			return builder;
		}

		private static StringBuilder AppendAutoGettersSetters(StringBuilder builder, UhtClass classObj)
		{
			UhtUsedDefineScopes<UhtProperty> autoGetterSetterProperties = GetAutoGetterSetterProperties(classObj);
			if (autoGetterSetterProperties.IsEmpty)
			{
				return builder;
			}

			return builder.AppendInstances(autoGetterSetterProperties, UhtDefineScopeNames.Standard,
				(builder, property) =>
				{
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.GetterSpecifiedAuto))
					{
						string getterCallText = property.Getter ?? "Get" + property.SourceName;
						builder.AppendPropertyText(property, UhtPropertyTextType.GetterRetVal).Append(classObj.SourceName).Append("::").Append(getterCallText).Append("() const\r\n");
						builder.Append("{\r\n");
						builder.Append("\treturn ").Append(property.SourceName).Append(";\r\n");
						builder.Append("}\r\n");
					}
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.SetterSpecifiedAuto))
					{
						string setterCallText = property.Setter ?? "Set" + property.SourceName;
						builder.Append("void ").Append(classObj.SourceName).Append("::").Append(setterCallText).Append('(').AppendPropertyText(property, UhtPropertyTextType.SetterParameterArgType).Append("InValue").Append(")\r\n");
						builder.Append("{\r\n");
						// @todo: setter defn
						builder.Append("}\r\n");
					}
				});
		}
		
		private static StringBuilder AppendPropertyAccessors(StringBuilder builder, UhtClass classObj)
		{
			foreach (UhtType type in classObj.Children)
			{
				if (type is UhtProperty property)
				{
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.GetterFound))
					{
						builder.Append("void ").Append(classObj.SourceName).Append("::").AppendPropertyGetterWrapperName(property).Append("(const void* Object, void* OutValue)\r\n");
						builder.Append("{\r\n");
						using (UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.Standard, property.DefineScope))
						{
							builder.Append("\tconst ").Append(classObj.SourceName).Append("* Obj = (const ").Append(classObj.SourceName).Append("*)Object;\r\n");
							if (property.IsStaticArray)
							{
								builder
									.Append('\t')
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("* Source = (")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("*)Obj->")
									.Append(property.Getter!)
									.Append("();\r\n");
								builder
									.Append('\t')
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("* Result = (")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("*)OutValue;\r\n");
								builder
									.Append("\tCopyAssignItems(Result, Source, ")
									.Append(property.ArrayDimensions)
									.Append(");\r\n");
							}
							else if (property is UhtByteProperty byteProperty && byteProperty.Enum != null)
							{
								// If someone passed in a TEnumAsByte instead of the actual enum value, the cast in the else clause would cause an issue.
								// Since this is known to be a TEnumAsByte, we just fetch the first byte.  *HOWEVER* on MSB machines where 
								// the actual enum value is passed in this will fail and return zero if the native size of the enum > 1 byte.
								builder
									.Append('\t')
									.Append("uint8")
									.Append("& Result = *(")
									.Append("uint8")
									.Append("*)OutValue;\r\n");
								builder
									.Append("\tResult = (")
									.Append("uint8")
									.Append(")Obj->")
									.Append(property.Getter!)
									.Append("();\r\n");
							}
							else
							{
								builder
									.Append('\t')
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("& Result = *(")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("*)OutValue;\r\n");
								builder
									.Append("\tResult = (")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append(")Obj->")
									.Append(property.Getter!)
									.Append("();\r\n");
							}
						}
						builder.Append("}\r\n");
					}
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.SetterFound))
					{
						builder.Append("void ").Append(classObj.SourceName).Append("::").AppendPropertySetterWrapperName(property).Append("(void* Object, const void* InValue)\r\n");
						builder.Append("{\r\n");
						using (UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.Standard, property.DefineScope))
						{
							builder.Append('\t').Append(classObj.SourceName).Append("* Obj = (").Append(classObj.SourceName).Append("*)Object;\r\n");
							if (property.IsStaticArray)
							{
								builder
									.Append('\t')
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
									.Append('\t')
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append(" Value = (")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append(")*(uint8*)InValue;\r\n");
							}
							else
							{
								builder
									.Append('\t')
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("& Value = *(")
									.AppendPropertyText(property, UhtPropertyTextType.GetterSetterArg)
									.Append("*)InValue;\r\n");
							}
							builder
								.Append("\tObj->")
								.Append(property.Setter!)
								.Append("(Value);\r\n");
						}
						builder.Append("}\r\n");
					}
				}
			}
			return builder;
		}

		private static StringBuilder AppendSparseAccessors(StringBuilder builder, UhtClass classObj)
		{
			foreach (UhtScriptStruct sparseScriptStruct in GetSparseDataStructsToExport(classObj))
			{
				string sparseDataType = sparseScriptStruct.EngineName;

				builder.Append('F').Append(sparseDataType).Append("* ").Append(classObj.SourceName).Append("::Get").Append(sparseDataType).Append("() const \r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn static_cast<F").Append(sparseDataType).Append("*>(GetClass()->GetOrCreateSparseClassData());\r\n");
				builder.Append("}\r\n");

				builder.Append("const F").Append(sparseDataType).Append("* ").Append(classObj.SourceName).Append("::Get").Append(sparseDataType).Append("(EGetSparseClassDataMethod GetMethod) const\r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn static_cast<const F").Append(sparseDataType).Append("*>(GetClass()->GetSparseClassData(GetMethod));\r\n");
				builder.Append("}\r\n");

				builder.Append("UScriptStruct* ").Append(classObj.SourceName).Append("::StaticGet").Append(sparseDataType).Append("ScriptStruct()\r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn F").Append(sparseDataType).Append("::StaticStruct();\r\n");
				builder.Append("}\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendSerializer(StringBuilder builder, UhtClass classObj, UhtSerializerArchiveType serializerType, string macroText)
		{
			if (!classObj.SerializerArchiveType.HasAnyFlags(serializerType))
			{
				builder.AppendScoped(UhtDefineScopeNames.Standard, classObj.SerializerDefineScope,
					builder => builder.Append(macroText).Append('(').Append(classObj.SourceName).Append(")\r\n"));
			}
			return builder;
		}

		private StringBuilder AppendNativeGeneratedInitCode(StringBuilder builder, UhtClass classObj, UhtUsedDefineScopes<UhtFunction> functions)
		{
			const string MetaDataParamsName = "Class_MetaDataParams";
			string singletonName = GetSingletonName(classObj, true);
			string staticsName = singletonName + "_Statics";
			string registrationName = $"Z_Registration_Info_UClass_{classObj.SourceName}";
			string[]? sparseDataTypes = classObj.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);

			PropertyMemberContextImpl context = new(CodeGenerator, classObj, classObj.SourceName, staticsName);

			bool hasInterfaces = classObj.Bases.Any(x => x is UhtClass baseClass && baseClass.ClassFlags.HasAnyFlags(EClassFlags.Interface));

			builder.Append("IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(").Append(classObj.SourceName).Append(");\r\n");

			// Everything from this point on will be part of the definition hash
			int hashCodeBlockStart = builder.Length;

			// simple ::StaticClass wrapper to avoid header, link and DLL hell
			{
				builder.Append("UClass* ").Append(GetSingletonName(classObj, false)).Append("()\r\n");
				builder.Append("{\r\n");
				builder.Append("\treturn ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
				builder.Append("}\r\n");
			}

			// Collect the properties
			UhtUsedDefineScopes<UhtProperty> properties = new(classObj.Properties);

			// Declare the statics object
			{
				builder.Append("struct ").Append(staticsName).Append("\r\n");
				builder.Append("{\r\n");

				builder.AppendMetaDataDecl(classObj, context, properties, MetaDataParamsName, 1);

				AppendPropertiesDecl(builder, context, properties, 1);

				builder.Append("\tstatic UObject* (*const DependentSingletons[])();\r\n");

				//builder.AppendIfInstances(functions, UhtDefineScopeNames.WithEditor, builder => builder.Append("\t\tstatic const FClassFunctionLinkInfo FuncInfo[];\r\n"));
				// Functions
				builder.AppendInstances(functions, UhtDefineScopeNames.WithEditor,
					builder =>
					{
						builder.Append("\tstatic constexpr FClassFunctionLinkInfo ").Append("FuncInfo[] = {\r\n");
					},
					(builder, function) =>
					{
						builder
							.Append("\t\t{ &")
							.Append(GetSingletonName(function, true))
							.Append(", ")
							.AppendUTF8LiteralString(function.EngineName)
							.Append(" },")
							.AppendObjectHash(classObj, context, function)
							.Append("\r\n");
					},
					builder =>
					{
						builder.Append("\t};\r\n");
						builder.Append("\tstatic_assert(UE_ARRAY_COUNT(").Append("FuncInfo) < 2048);\r\n");
					});


				if (hasInterfaces)
				{
					builder.Append("\tstatic const UECodeGen_Private::FImplementedInterfaceParams InterfaceParams[];\r\n");
				}

				builder.Append("\tstatic constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {\r\n");
				if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
				{
					builder.Append("\t\tTCppClassTypeTraits<I").Append(classObj.EngineName).Append(">::IsAbstract,\r\n");
				}
				else
				{
					builder.Append("\t\tTCppClassTypeTraits<").Append(classObj.SourceName).Append(">::IsAbstract,\r\n");
				}
				builder.Append("\t};\r\n");

				builder.Append("\tstatic const UECodeGen_Private::FClassParams ClassParams;\r\n");

				builder.Append("};\r\n");
			}

			// Define the statics object
			{
				AppendPropertiesDefs(builder, context, properties, 0);

				// Dependent singletons
				builder.Append("UObject* (*const ").Append(staticsName).Append("::DependentSingletons[])() = {\r\n");
				if (classObj.SuperClass != null && classObj.SuperClass != classObj)
				{
					builder.Append("\t(UObject* (*)())").Append(GetSingletonName(classObj.SuperClass, true)).Append(",\r\n");
				}
				builder.Append("\t(UObject* (*)())").Append(GetSingletonName(Package, true)).Append(",\r\n");
				builder.Append("};\r\n");
				builder.Append("static_assert(UE_ARRAY_COUNT(").Append(staticsName).Append("::DependentSingletons) < 16);\r\n");

				// Implemented interfaces
				if (hasInterfaces)
				{
					builder.Append("const UECodeGen_Private::FImplementedInterfaceParams ").Append(staticsName).Append("::InterfaceParams[] = {\r\n");

					foreach (UhtStruct structObj in classObj.Bases)
					{
						if (structObj is UhtClass baseClass)
						{
							if (baseClass.ClassFlags.HasAnyFlags(EClassFlags.Interface))
							{
								builder
									.Append("\t{ ")
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

					builder.Append("};\r\n");
				}

				// Class parameters
				EClassFlags classFlags = classObj.ClassFlags & EClassFlags.SaveInCompiledInClasses;
				builder.Append("const UECodeGen_Private::FClassParams ").Append(staticsName).Append("::ClassParams = {\r\n");
				builder.Append("\t&").Append(classObj.SourceName).Append("::StaticClass,\r\n");
				if (classObj.Config.Length > 0)
				{
					builder.Append('\t').AppendUTF8LiteralString(classObj.Config).Append(",\r\n");
				}
				else
				{
					builder.Append("\tnullptr,\r\n");
				}
				builder.Append("\t&StaticCppClassTypeInfo,\r\n");
				builder.Append("\tDependentSingletons,\r\n");
				builder.AppendArrayPtrLine(functions, UhtDefineScopeNames.WithEditor, null, "FuncInfo", 1, ",\r\n");
				builder.AppendArrayPtrLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 1, ",\r\n");
				builder.Append('\t').Append(hasInterfaces ? "InterfaceParams" : "nullptr").Append(",\r\n");
				builder.Append("\tUE_ARRAY_COUNT(DependentSingletons),\r\n");
				builder.AppendArrayCountLine(functions, UhtDefineScopeNames.WithEditor, null, "FuncInfo", 1, ",\r\n");
				builder.AppendArrayCountLine(properties, UhtDefineScopeNames.Standard, staticsName, "PropPointers", 1, ",\r\n");
				builder.Append('\t').Append(hasInterfaces ? "UE_ARRAY_COUNT(InterfaceParams)" : "0").Append(",\r\n");
				builder.Append($"\t0x{(uint)classFlags:X8}u,\r\n");
				builder.Append('\t').AppendMetaDataParams(classObj, staticsName, MetaDataParamsName).Append("\r\n");
				builder.Append("};\r\n");
			}

			// Class registration
			{
				builder.Append("UClass* ").Append(singletonName).Append("()\r\n");
				builder.Append("{\r\n");
				builder.Append("\tif (!").Append(registrationName).Append(".OuterSingleton)\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tUECodeGen_Private::ConstructUClass(").Append(registrationName).Append(".OuterSingleton, ").Append(staticsName).Append("::ClassParams);\r\n");
				if (sparseDataTypes != null)
				{
					foreach (string sparseClass in sparseDataTypes)
					{
						builder.Append("\t\t").Append(registrationName).Append(".OuterSingleton->SetSparseClassDataStruct(").Append(classObj.SourceName).Append("::StaticGet").Append(sparseClass).Append("ScriptStruct());\r\n");
					}
				}
				builder.Append("\t}\r\n");
				builder.Append("\treturn ").Append(registrationName).Append(".OuterSingleton;\r\n");
				builder.Append("}\r\n");
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
					builder.Append("/* friend declarations for pasting into noexport class ").Append(classObj.SourceName).Append("\r\n");
					builder.Append("friend struct ").Append(staticsName).Append(";\r\n");
					builder.Append("*/\r\n");
				}

				if (Session.IncludeDebugOutput)
				{
					using UhtRentedPoolBuffer<char> borrowBuffer = hashBuilder.RentPoolBuffer(saveLength, hashBuilder.Length - saveLength);
					builder.Append("#if 0\r\n");
					builder.Append(borrowBuffer.Buffer.Memory);
					builder.Append("#endif\r\n");
				}

				// Calculate generated class initialization code hash so that we know when it changes after hot-reload
				{
					using UhtRentedPoolBuffer<char> borrowBuffer = hashBuilder.RentPoolBuffer();
					ObjectInfos[classObj.ObjectTypeIndex].Hash = UhtHash.GenenerateTextHash(borrowBuffer.Buffer.Memory.Span);
				}
			}

			builder.Append("template<> ").Append(PackageApi).Append("UClass* StaticClass<").Append(classObj.SourceName).Append(">()\r\n");
			builder.Append("{\r\n");
			builder.Append("\treturn ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
			builder.Append("}\r\n");

			if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.SelfHasReplicatedProperties))
			{
				builder.Append("void ").Append(classObj.SourceName).Append("::ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const\r\n");
				builder.Append("{\r\n");

				foreach (UhtProperty property in classObj.Properties)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
					{
						builder.Append("\tstatic const FName Name_").Append(property.SourceName).Append("(TEXT(\"").Append(property.SourceName).Append("\"));\r\n");
					}
				}
				builder.Append("\tconst bool bIsValid = true");
				foreach (UhtProperty property in classObj.Properties)
				{
					if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
					{
						if (!property.IsStaticArray)
						{
							builder.Append("\r\n\t\t&& Name_").Append(property.SourceName).Append(" == ClassReps[(int32)ENetFields_Private::").Append(property.SourceName).Append("].Property->GetFName()");
						}
						else
						{
							builder.Append("\r\n\t\t&& Name_").Append(property.SourceName).Append(" == ClassReps[(int32)ENetFields_Private::").Append(property.SourceName).Append("_STATIC_ARRAY].Property->GetFName()");
						}
					}
				}
				builder.Append(";\r\n");
				builder.Append("\tcheckf(bIsValid, TEXT(\"UHT Generated Rep Indices do not match runtime populated Rep Indices for properties in ").Append(classObj.SourceName).Append("\"));\r\n");
				builder.Append("}\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendNatives(StringBuilder builder, UhtClass classObj)
		{
			builder.Append("void ").Append(classObj.SourceName).Append("::StaticRegisterNatives").Append(classObj.SourceName).Append("()\r\n");
			builder.Append("{\r\n");

			UhtUsedDefineScopes<UhtFunction> functions = new(classObj.Functions.Where(x => x.FunctionFlags.HasExactFlags(EFunctionFlags.Native | EFunctionFlags.NetRequest, EFunctionFlags.Native)));
			functions.Instances.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

			if (!functions.IsEmpty)
			{
				{
					using UhtMacroBlockEmitter blockEmitter = new(builder, UhtDefineScopeNames.WithEditor, functions.SoleScope);
					builder.Append("\tUClass* Class = ").Append(classObj.SourceName).Append("::StaticClass();\r\n");
					builder.Append("\tstatic const FNameNativePtrPair Funcs[] = {\r\n");

					foreach (UhtFunction function in functions.Instances)
					{
						blockEmitter.Set(function.DefineScope);
						builder
							.Append("\t\t{ ")
							.AppendUTF8LiteralString(function.EngineName)
							.Append(", &")
							.AppendClassSourceNameOrInterfaceName(classObj)
							.Append("::exec")
							.Append(function.EngineName)
							.Append(" },\r\n");
					}

					// This will close the block if we have one that isn't editor only
					blockEmitter.Set(functions.SoleScope);

					builder.Append("\t};\r\n");
					builder.Append("\tFNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));\r\n");
				}
			}

			builder.Append("}\r\n");
			return builder;
		}

		private static StringBuilder AppendFunctionThunk(StringBuilder builder, UhtFunction function)
		{
			// Export the GET macro for the parameters
			foreach (UhtType parameter in function.ParameterProperties.Span)
			{
				if (parameter is UhtProperty property)
				{
					builder.Append('\t').AppendFunctionThunkParameterGet(property).Append(";\r\n");
				}
			}

			builder.Append("\tP_FINISH;\r\n");
			builder.Append("\tP_NATIVE_BEGIN;\r\n");

			// Call the validate function if there is one
			if (!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CppStatic) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetValidate))
			{
				builder.Append("\tif (!P_THIS->").Append(function.CppValidationImplName).Append('(').AppendFunctionThunkParameterNames(function).Append("))\r\n");
				builder.Append("\t{\r\n");
				builder.Append("\t\tRPC_ValidateFailed(TEXT(\"").Append(function.CppValidationImplName).Append("\"));\r\n");
				builder.Append("\t\treturn;\r\n");   // If we got here, the validation function check failed
				builder.Append("\t}\r\n");
			}

			// Write out the return value
			builder.Append('\t');
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
			builder.Append("\tP_NATIVE_END;\r\n");
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
			foreach (UhtType parameter in function.ParameterProperties.Span)
			{
				if (parameter is UhtProperty property)
				{
					if (first)
					{
						first = false;
					}
					else
					{
						builder.Append(',');
					}

					bool needsGCBarrier = property.NeedsGCBarrierWhenPassedToFunction(function);
					if (needsGCBarrier)
					{
						builder.Append("P_ARG_GC_BARRIER(");
					}
					builder.AppendFunctionThunkParameterArg(property);
					if (needsGCBarrier)
					{
						builder.Append(')');
					}
				}
			}
			return builder;
		}
	}
}
