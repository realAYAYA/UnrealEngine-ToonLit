// Copyright Epic Games, Inc. All Rights Reserved.
// Updated to SDK 1.1.82.1

#include "VulkanShaderFormat.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "hlslcc.h"

void PatchSpirvReflectionEntries(FVulkanSpirv& Spirv)
{
	TMap<uint32, FString> Names;
	struct FDecorations
	{
		uint32 BindingIndex = UINT32_MAX;
		uint32 WordBindingIndex = UINT32_MAX;
		uint32 DescriptorSet = UINT32_MAX;
		uint32 WordDescriptorSet = UINT32_MAX;
	};
	TMap<uint32, FDecorations> Decorations;
	TMap<uint32, uint32> TypePointerUniforms;
	TMap<uint32, uint32> VariableUniformTypes;

	for (FSpirvConstIterator Iter = Spirv.cbegin(); Iter != Spirv.cend(); ++Iter)
	{
		switch (Iter.Opcode())
		{
		case SpvOpName:
		{
			uint32 TargetId = Iter.Operand(1);
			FString Name = ANSI_TO_TCHAR(Iter.OperandAsString(2));
			Names.Add(TargetId, MoveTemp(Name));
		}
			break;
		case SpvOpDecorate:
		{
			uint32 TargetId = Iter.Operand(1);
			SpvDecoration Decoration = Iter.OperandAs<SpvDecoration>(2);
			switch (Decoration)
			{
			case SpvDecorationDescriptorSet:
			{
				uint32 Value = Iter.Operand(3);
				uint32 WordValueIndex = Spirv.GetWordOffset(Iter, 3);
				FDecorations& UBDecoration = Decorations.FindOrAdd(TargetId);
				UBDecoration.DescriptorSet = Value;
				UBDecoration.WordDescriptorSet = WordValueIndex;
				break;
			}
			case SpvDecorationBinding:
			{
				uint32 Value = Iter.Operand(3);
				uint32 WordValueIndex = Spirv.GetWordOffset(Iter, 3);
				FDecorations& UBDecoration = Decorations.FindOrAdd(TargetId);
				UBDecoration.BindingIndex = Value;
				UBDecoration.WordBindingIndex = WordValueIndex;
				break;
			}
			default:
				break;
			}
		}
			break;
		case SpvOpTypePointer:
		{
			uint32 Result = Iter.Operand(1);
			SpvStorageClass Storage = Iter.OperandAs<SpvStorageClass>(2);
			if (Storage == SpvStorageClassUniform || Storage == SpvStorageClassUniformConstant)
			{
				uint32 Type = Iter.Operand(3);
				TypePointerUniforms.Add(Result, Type);
			}
		}
			break;
		case SpvOpVariable:
		{
			uint32 Type = Iter.Operand(1);
			uint32 Id = Iter.Operand(2);
			SpvStorageClass Storage = Iter.OperandAs<SpvStorageClass>(3);
			if (Storage == SpvStorageClassUniform || 
				Storage == SpvStorageClassUniformConstant || 
				Storage == SpvStorageClassStorageBuffer)
			{
				VariableUniformTypes.Add(Id, Type);
			}
		}
			break;
		case SpvOpFunction:
			break;
		default:
			break;
		}
	}

	// Go through all found uniform variables and make sure we found the right info
	for (const auto& Pair : VariableUniformTypes)
	{
		const uint32 VariableId = Pair.Key;
		const FString* FoundVariableName = Names.Find(VariableId);
		if (FoundVariableName)
		{
			const uint32 VariableType = Pair.Value;
			if (FoundVariableName->Len() == 0)
			{
				// Uniform buffer
				uint32 TypePointer = TypePointerUniforms.FindChecked(VariableType);
				const FString* FoundTypeName = Names.Find(TypePointer);
				if (FoundTypeName && FoundTypeName->Len() > 0)
				{
					FVulkanSpirv::FEntry* FoundEntry = Spirv.GetEntry(*FoundTypeName);
					if (FoundEntry)
					{
						FDecorations& FoundDecorations = Decorations.FindChecked(VariableId);
						FoundEntry->Binding = FoundDecorations.BindingIndex;
						FoundEntry->WordBindingIndex = FoundDecorations.WordBindingIndex;
						FoundEntry->DescriptorSet = FoundDecorations.DescriptorSet;
						FoundEntry->WordDescriptorSetIndex = FoundDecorations.WordDescriptorSet;
					}
				}
			}
			else
			{
				// Standalone global var
				FVulkanSpirv::FEntry* FoundEntry = Spirv.GetEntry(*FoundVariableName);
				if (FoundEntry)
				{
					FDecorations& FoundDecorations = Decorations.FindChecked(VariableId);
					FoundEntry->Binding = FoundDecorations.BindingIndex;
					FoundEntry->WordBindingIndex = FoundDecorations.WordBindingIndex;
					FoundEntry->DescriptorSet = FoundDecorations.DescriptorSet;
					FoundEntry->WordDescriptorSetIndex = FoundDecorations.WordDescriptorSet;
				}
			}
		}
	}
}
