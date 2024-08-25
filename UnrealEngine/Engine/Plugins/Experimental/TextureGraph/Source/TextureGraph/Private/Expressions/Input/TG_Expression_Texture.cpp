// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Input/TG_Expression_Texture.h"

#include "TG_Graph.h"
#include "2D/TextureHelper.h"
#include "Model/StaticImageResource.h"



FTG_SignaturePtr UTG_Expression_Texture::BuildInputParameterSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	return MakeShared<FTG_Signature>(SignatureInit);
};

FTG_SignaturePtr UTG_Expression_Texture::BuildInputConstantSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	for (auto& arg : SignatureInit.Arguments)
	{	
		// Turn the Input parameter TG_Texture entry into private so it s not exposed or used
		// in the constant case, the expression only rely on the UTexture asset
		if (arg.IsInput() && arg.IsParam())
		{
			arg.ArgumentType = FTG_ArgumentType(ETG_Access::Private);
		}
	}
	return MakeShared<FTG_Signature>(SignatureInit);
};

void UTG_Expression_Texture::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);
	if (Texture)
	{
		Output = Texture;
	}
	else if (Source)
	{
		const FString Path = Source->GetPathName();
		UStaticImageResource* StaticImageResource = UStaticImageResource::CreateNew<UStaticImageResource>();
		StaticImageResource->SetAssetUUID(Path);

		//Until we have srgb value exposed in the UI we need to set the Srgb of the Output Descriptor here from the source
		//This gets updated for the late bond case but since we do not have the UI to specify the override in other nodes 
		// the override value will always be set to false while combining the buffers
		auto DesiredDesc = Output.GetBufferDescriptor();
		DesiredDesc.bIsSRGB = Source->SRGB;

		Output = StaticImageResource->GetBlob(InContext->Cycle, DesiredDesc, 0);
	}
	else
	{
		Output = FTG_Texture::GetBlack();
	}
}

bool UTG_Expression_Texture::Validate(MixUpdateCyclePtr Cycle)
{
	UMixInterface* ParentMix = Cast<UMixInterface>(GetOutermostObject());
	
	//Check here if Source is VT
	if (Source && !CanHandleAsset(Source))
	{
		auto ErrorType = static_cast<int32>(ETextureGraphErrorType::UNSUPPORTED_TYPE);

		UClass* Type = Source->GetClass();
		FString TypeName = Source->VirtualTextureStreaming ? "Virtual Texture" : Type->GetName();

		TextureGraphEngine::GetErrorReporter(ParentMix)->ReportError(ErrorType, FString::Printf(TEXT("%s not supported at the moment"), *TypeName), GetParentNode());
		return false;
	}
	
	return true;
}
void UTG_Expression_Texture::SetTitleName(FName NewName)
{
	GetParentNode()->GetPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Texture, Texture))->SetAliasName(NewName);
}

FName UTG_Expression_Texture::GetTitleName() const
{
	return GetParentNode()->GetPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Texture, Texture))->GetAliasName();
}

void UTG_Expression_Texture::SetAsset(UObject* Asset)
{
	if(CanHandleAsset(Asset))
	{
		Modify();

		Source = Cast<UTexture>(Asset);

#if WITH_EDITOR
		// We need to find its property and trigger property change event manually.
		const auto SourcePin = GetParentNode()->GetInputPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Texture, Source));

		check(SourcePin);
	
		if(SourcePin)
		{
			auto Property = SourcePin->GetExpressionProperty();
			PropertyChangeTriggered(Property, EPropertyChangeType::ValueSet);
		}
#endif
	}
}

bool UTG_Expression_Texture::CanHandleAsset(UObject* Asset)
{
	return TextureHelper::CanSupportTexture(Cast<UTexture>(Asset));
}
