// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

template <typename ID3D1xShaderReflection, typename D3D1x_SHADER_DESC, typename D3D1x_SHADER_INPUT_BIND_DESC,
	typename ID3D1xShaderReflectionConstantBuffer, typename D3D1x_SHADER_BUFFER_DESC,
	typename ID3D1xShaderReflectionVariable, typename D3D1x_SHADER_VARIABLE_DESC>
	inline void ExtractParameterMapFromD3DShader(
		const FShaderCompilerInput& Input,
		const FShaderParameterParser& ShaderParameterParser,
		uint32 BindingSpace,
		ID3D1xShaderReflection* Reflector, const D3D1x_SHADER_DESC& ShaderDesc,
		bool& bGlobalUniformBufferUsed, bool& bDiagnosticBufferUsed, uint32& NumSamplers, uint32& NumSRVs, uint32& NumCBs, uint32& NumUAVs,
		FShaderCompilerOutput& Output, TArray<FString>& UniformBufferNames, TBitArray<>& UsedUniformBufferSlots, TArray<FShaderCodeVendorExtension>& VendorExtensions)
{
	const bool bBindlessEnabled = (Input.Environment.CompilerFlags.Contains(CFLAG_BindlessResources) || Input.Environment.CompilerFlags.Contains(CFLAG_BindlessSamplers));

	// Add parameters for shader resources (constant buffers, textures, samplers, etc. */
	for (uint32 ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ResourceIndex++)
	{
		D3D1x_SHADER_INPUT_BIND_DESC BindDesc;
		Reflector->GetResourceBindingDesc(ResourceIndex, &BindDesc);

		if (!IsCompatibleBinding(BindDesc, BindingSpace))
		{
			continue;
		}

		if (BindDesc.Type == D3D_SIT_CBUFFER || BindDesc.Type == D3D_SIT_TBUFFER)
		{
			const uint32 CBIndex = BindDesc.BindPoint;
			ID3D1xShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByName(BindDesc.Name);
			D3D1x_SHADER_BUFFER_DESC CBDesc;
			ConstantBuffer->GetDesc(&CBDesc);
			const bool bGlobalCB = (FCStringAnsi::Strcmp(CBDesc.Name, "$Globals") == 0);
			const bool bRootConstantsCB = (FCStringAnsi::Strcmp(CBDesc.Name, "UERootConstants") == 0);
			const bool bIsRootCB = FCString::Strcmp(ANSI_TO_TCHAR(CBDesc.Name), FShaderParametersMetadata::kRootUniformBufferBindingName) == 0;

			if (bGlobalCB)
			{
				if (Input.ShouldUseStableConstantBuffer())
				{
					// Each member found in the global constant buffer means it was not in RootParametersStructure or
					// it would have been moved by ShaderParameterParser.ParseAndModify().
					for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
					{
						ID3D1xShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
						D3D1x_SHADER_VARIABLE_DESC VariableDesc;
						Variable->GetDesc(&VariableDesc);
						if (VariableDesc.uFlags & D3D_SVF_USED)
						{
							AddUnboundShaderParameterError(
								Input,
								ShaderParameterParser,
								ANSI_TO_TCHAR(VariableDesc.Name),
								Output);
						}
					}
				}
				else
				{
					// Track all of the variables in this constant buffer.
					for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
					{
						ID3D1xShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
						D3D1x_SHADER_VARIABLE_DESC VariableDesc;
						Variable->GetDesc(&VariableDesc);
						if (VariableDesc.uFlags & D3D_SVF_USED)
						{
							bGlobalUniformBufferUsed = true;

							HandleReflectedGlobalConstantBufferMember(
								FString(VariableDesc.Name),
								CBIndex,
								VariableDesc.StartOffset,
								VariableDesc.Size,
								Output
							);

							UsedUniformBufferSlots[CBIndex] = true;
						}
					}
				}
			}
			else if (bRootConstantsCB)
			{
				// For the UERootConstants root constant CB, we want to fully skip adding it to the parameter map, or
				// updating the used slots or num CBs (all those assume space0).
			}
			else if (bIsRootCB && Input.ShouldUseStableConstantBuffer())
			{
				if (CBIndex == FShaderParametersMetadata::kRootCBufferBindingIndex)
				{
					int32 ConstantBufferSize = 0;

					// Track all of the variables in this constant buffer.
					for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
					{
						ID3D1xShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
						D3D1x_SHADER_VARIABLE_DESC VariableDesc;
						Variable->GetDesc(&VariableDesc);
						if (VariableDesc.uFlags & D3D_SVF_USED)
						{
							HandleReflectedRootConstantBufferMember(
								Input,
								ShaderParameterParser,
								FString(VariableDesc.Name),
								VariableDesc.StartOffset,
								VariableDesc.Size,
								Output
							);

							ConstantBufferSize = FMath::Max<int32>(ConstantBufferSize, VariableDesc.StartOffset + VariableDesc.Size);
						}
					}

					if (ConstantBufferSize > 0)
					{
						HandleReflectedRootConstantBuffer(ConstantBufferSize, Output);

						bGlobalUniformBufferUsed = true;
						UsedUniformBufferSlots[CBIndex] = true;
					}
				}
				else
				{
					FString ErrorMessage = FString::Printf(
						TEXT("Error: %s is expected to always be in the API slot %d, but is actually in slot %d."),
						FShaderParametersMetadata::kRootUniformBufferBindingName,
						FShaderParametersMetadata::kRootCBufferBindingIndex,
						CBIndex);
					Output.Errors.Add(FShaderCompilerError(*ErrorMessage));
					Output.bSucceeded = false;
				}
			}
			else
			{
				// Track just the constant buffer itself.
				const FString UniformBufferName(CBDesc.Name);

				AddShaderValidationUBSize(CBIndex, CBDesc.Size, Output);
				HandleReflectedUniformBuffer(UniformBufferName, CBIndex, Output);
				
				UsedUniformBufferSlots[CBIndex] = true;

				if (bBindlessEnabled)
				{
					for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
					{
						ID3D1xShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);

						D3D1x_SHADER_VARIABLE_DESC VariableDesc;
						Variable->GetDesc(&VariableDesc);

						if (VariableDesc.uFlags & D3D_SVF_USED)
						{
							HandleReflectedUniformBufferConstantBufferMember(
								CBIndex,
								FString(VariableDesc.Name),
								VariableDesc.StartOffset,
								VariableDesc.Size,
								Output
							);
						}
					}
				}
			}

			if (UniformBufferNames.Num() <= (int32)CBIndex)
			{
				UniformBufferNames.AddDefaulted(CBIndex - UniformBufferNames.Num() + 1);
			}
			UniformBufferNames[CBIndex] = UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(FString(CBDesc.Name));

			NumCBs = FMath::Max(NumCBs, BindDesc.BindPoint + BindDesc.BindCount);
		}
		else if (BindDesc.Type == D3D_SIT_TEXTURE || BindDesc.Type == D3D_SIT_SAMPLER)
		{
			check(BindDesc.BindCount == 1);

			// https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/blob/master/ags_lib/hlsl/ags_shader_intrinsics_dx11.hlsl
			const bool bIsAMDTexExtension = (FCStringAnsi::Strcmp(BindDesc.Name, "AmdDxExtShaderIntrinsicsResource") == 0);
			const bool bIsAMDSmpExtension = (FCStringAnsi::Strcmp(BindDesc.Name, "AmdDxExtShaderIntrinsicsSamplerState") == 0);
			const bool bIsVendorParameter = bIsAMDTexExtension || bIsAMDSmpExtension;

			const uint32 BindCount = 1;
			const EShaderParameterType ParameterType = (BindDesc.Type == D3D_SIT_SAMPLER) ? EShaderParameterType::Sampler : EShaderParameterType::SRV;

			if (bIsVendorParameter)
			{
				VendorExtensions.Emplace(EGpuVendorId::Amd, 0, BindDesc.BindPoint, BindCount, ParameterType);
			}
			else if (ParameterType == EShaderParameterType::Sampler)
			{
				HandleReflectedShaderSampler(FString(BindDesc.Name), BindDesc.BindPoint, Output);
				NumSamplers = FMath::Max(NumSamplers, BindDesc.BindPoint + BindCount);
			}
			else
			{
				const FShaderParameterParser::FParsedShaderParameter* ParsedParam = ShaderParameterParser.FindParameterInfosUnsafe(BindDesc.Name);
				AddShaderValidationSRVType(BindDesc.BindPoint, ParsedParam ? ParsedParam->ParsedTypeDecl : EShaderCodeResourceBindingType::Invalid, Output);

				HandleReflectedShaderResource(FString(BindDesc.Name), BindDesc.BindPoint, Output);
				NumSRVs = FMath::Max(NumSRVs, BindDesc.BindPoint + BindCount);
			}
		}
		else if (BindDesc.Type == D3D_SIT_UAV_RWTYPED || BindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED ||
			BindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS || BindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
			BindDesc.Type == D3D_SIT_UAV_APPEND_STRUCTURED)
		{
			check(BindDesc.BindCount == 1);

			// https://developer.nvidia.com/unlocking-gpu-intrinsics-hlsl
			const bool bIsNVExtension = (FCStringAnsi::Strcmp(BindDesc.Name, "g_NvidiaExt") == 0);

			// https://github.com/intel/intel-graphics-compiler/blob/master/inc/IntelExtensions.hlsl
			const bool bIsIntelExtension = (FCStringAnsi::Strcmp(BindDesc.Name, "g_IntelExt") == 0);

			// https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/blob/master/ags_lib/hlsl/ags_shader_intrinsics_dx11.hlsl
			const bool bIsAMDExtensionDX11 = (FCStringAnsi::Strcmp(BindDesc.Name, "AmdDxExtShaderIntrinsicsUAV") == 0);

			// https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/blob/master/ags_lib/hlsl/ags_shader_intrinsics_dx12.hlsl
			const bool bIsAMDExtensionDX12 = (FCStringAnsi::Strcmp(BindDesc.Name, "AmdExtD3DShaderIntrinsicsUAV") == 0);

			const bool bIsVendorParameter = bIsNVExtension || bIsIntelExtension || bIsAMDExtensionDX11 || bIsAMDExtensionDX12;

			// See D3DCommon.ush
			const bool bIsDiagnosticBufferParameter = (FCStringAnsi::Strcmp(BindDesc.Name, "UEDiagnosticBuffer") == 0);

			const uint32 BindCount = 1;
			if (bIsVendorParameter)
			{
				const EGpuVendorId VendorId =
					bIsNVExtension ? EGpuVendorId::Nvidia :
					(bIsAMDExtensionDX11 || bIsAMDExtensionDX12) ? EGpuVendorId::Amd :
					bIsIntelExtension ? EGpuVendorId::Intel :
					EGpuVendorId::Unknown;
				VendorExtensions.Emplace(VendorId, 0, BindDesc.BindPoint, BindCount, EShaderParameterType::UAV);
			}
			else if (bIsDiagnosticBufferParameter)
			{
				bDiagnosticBufferUsed = true;
			}
			else
			{
				const FShaderParameterParser::FParsedShaderParameter* ParsedParam = ShaderParameterParser.FindParameterInfosUnsafe(BindDesc.Name);
				AddShaderValidationUAVType(BindDesc.BindPoint, ParsedParam ? ParsedParam->ParsedTypeDecl : EShaderCodeResourceBindingType::Invalid, Output);

				HandleReflectedShaderUAV(FString(BindDesc.Name), BindDesc.BindPoint, Output);
				NumUAVs = FMath::Max(NumUAVs, BindDesc.BindPoint + BindCount);
			}
		}
		else if (BindDesc.Type == D3D_SIT_STRUCTURED || BindDesc.Type == D3D_SIT_BYTEADDRESS)
		{
			check(BindDesc.BindCount == 1);
			FString BindDescName(BindDesc.Name);
			const FShaderParameterParser::FParsedShaderParameter* ParsedParam = ShaderParameterParser.FindParameterInfosUnsafe(BindDesc.Name);
			AddShaderValidationSRVType(BindDesc.BindPoint, ParsedParam ? ParsedParam->ParsedTypeDecl : EShaderCodeResourceBindingType::Invalid, Output);

			HandleReflectedShaderResource(BindDescName, BindDesc.BindPoint, Output);

			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/ns-d3d12shader-d3d12_shader_input_bind_desc
			// If the shader resource is a structured buffer, the field contains the stride of the type in bytes
			if ( BindDesc.Type == D3D_SIT_STRUCTURED)
			{
				UpdateStructuredBufferStride(Input, BindDescName, BindDesc.BindPoint, BindDesc.NumSamples, Output);
			}

			NumSRVs = FMath::Max(NumSRVs, BindDesc.BindPoint + 1);
		}
		else if (BindDesc.Type == (D3D_SHADER_INPUT_TYPE)(D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER + 1)) // D3D_SIT_RTACCELERATIONSTRUCTURE (12)
		{
			// Acceleration structure resources are treated as SRVs.
			check(BindDesc.BindCount == 1);

			const FShaderParameterParser::FParsedShaderParameter* ParsedParam = ShaderParameterParser.FindParameterInfosUnsafe(BindDesc.Name);
			AddShaderValidationSRVType(BindDesc.BindPoint, ParsedParam ? ParsedParam->ParsedTypeDecl : EShaderCodeResourceBindingType::Invalid, Output);

			HandleReflectedShaderResource(FString(BindDesc.Name), BindDesc.BindPoint, Output);
			NumSRVs = FMath::Max(NumSRVs, BindDesc.BindPoint + 1);
		}
	}
}

template <typename TBlob>
inline void GenerateFinalOutput(TRefCountPtr<TBlob>& CompressedData,
	const FShaderCompilerInput& Input, TArray<FShaderCodeVendorExtension>& VendorExtensions, 
	TBitArray<>& UsedUniformBufferSlots, TArray<FString>& UniformBufferNames,
	bool bProcessingSecondTime, const TArray<FString>& ShaderInputs,
	FShaderCodePackedResourceCounts& PackedResourceCounts, uint32 NumInstructions,
	FShaderCompilerOutput& Output,
	TFunction<void(FMemoryWriter&)> PostSRTWriterCallback,
	TFunction<void(FShaderCode&)> AddOptionalDataCallback)
{
	// Build the SRT for this shader.
	FShaderResourceTable SRT;

	TArray<uint8> UniformBufferNameBytes;

	{	
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(Input.Environment.ResourceTableMap, Input.Environment.UniformBufferMap, UsedUniformBufferSlots, Output.ParameterMap, GenericSRT);

		// Ray generation shaders rely on a different binding model that aren't compatible with global uniform buffers.
		if (Input.Target.Frequency != SF_RayGen)
		{
			CullGlobalUniformBuffers(Input.Environment.UniformBufferMap, Output.ParameterMap);
		}

		if (UniformBufferNames.Num() < GenericSRT.ResourceTableLayoutHashes.Num())
		{
			UniformBufferNames.AddDefaulted(GenericSRT.ResourceTableLayoutHashes.Num() - UniformBufferNames.Num());
		}

		for (int32 Index = 0; Index < GenericSRT.ResourceTableLayoutHashes.Num(); ++Index)
		{
			if (GenericSRT.ResourceTableLayoutHashes[Index] != 0 && UniformBufferNames[Index].Len() == 0)
			{
				for (const auto& KeyValue : Input.Environment.UniformBufferMap)
				{
					const FUniformBufferEntry& UniformBufferEntry = KeyValue.Value;

					if (UniformBufferEntry.LayoutHash == GenericSRT.ResourceTableLayoutHashes[Index])
					{
						UniformBufferNames[Index] = KeyValue.Key;
						break;
					}
				}
			}
		}

		FMemoryWriter UniformBufferNameWriter(UniformBufferNameBytes);
		UniformBufferNameWriter << UniformBufferNames;

		// Copy over the bits indicating which resource tables are active.
		SRT.ResourceTableBits = GenericSRT.ResourceTableBits;

		SRT.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, SRT.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, SRT.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, SRT.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, SRT.UnorderedAccessViewMap);
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_ForceRemoveUnusedInterpolators) && Input.Target.Frequency == SF_Pixel && Input.bCompilingForShaderPipeline && bProcessingSecondTime)

	{
		Output.bSupportsQueryingUsedAttributes = true;
		Output.UsedAttributes = ShaderInputs;
	}

	// Generate the final Output
	FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
	Ar << SRT;

	PostSRTWriterCallback(Ar);

	Ar.Serialize(CompressedData->GetBufferPointer(), CompressedData->GetBufferSize());

	// Append data that is generate from the shader code and assist the usage, mostly needed for DX12 
	{
		Output.ShaderCode.AddOptionalData(PackedResourceCounts);
		Output.ShaderCode.AddOptionalData(FShaderCodeUniformBuffers::Key, UniformBufferNameBytes.GetData(), UniformBufferNameBytes.Num());
		AddOptionalDataCallback(Output.ShaderCode);
	}

	// Append information about optional hardware vendor extensions
	if (VendorExtensions.Num() > 0)
	{
		TArray<uint8> WriterBytes;
		FMemoryWriter Writer(WriterBytes);
		Writer << VendorExtensions;
		if (WriterBytes.Num() > 0)
		{
			Output.ShaderCode.AddOptionalData(FShaderCodeVendorExtension::Key, WriterBytes.GetData(), WriterBytes.Num());
		}
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		Output.ShaderCode.AddOptionalData(FShaderCodeName::Key, TCHAR_TO_UTF8(*Input.GenerateShaderName()));
	}

	Output.SerializeShaderCodeValidation();
	Output.SerializeShaderDiagnosticData();

	// Set the number of instructions.
	Output.NumInstructions = NumInstructions;

	Output.NumTextureSamplers = PackedResourceCounts.NumSamplers;

	// Pass the target through to the output.
	Output.Target = Input.Target;
}
