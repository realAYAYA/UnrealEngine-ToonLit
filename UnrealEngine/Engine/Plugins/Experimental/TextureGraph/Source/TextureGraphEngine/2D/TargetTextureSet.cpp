// Copyright Epic Games, Inc. All Rights Reserved.
#include "TargetTextureSet.h"
#include "TextureHelper.h"
#include "2D/TextureHelper.h"
#include "Helper/ColorUtil.h"
#include "Device/FX/Device_FX.h"
#include "TextureGraphEngineGameInstance.h"
#include "Data/Blobber.h"
#include "2D/Tex.h"
#include "Model/Mix/MixSettings.h"

TargetTextureSet::TargetTextureSet(int32 InId, const FString& InName, RenderMeshPtr InMesh, int32 InWidth, int32 InHeight)
	: TextureSet(InWidth, InHeight)
	, Id(InId)
	, Name(InName)
	, Mesh(InMesh)
{
	
}

TargetTextureSet::~TargetTextureSet()
{
	Callbacks.Empty();
}

void TargetTextureSet::InitTex(int32 InTypeIndex)
{
	// This is being used in multiple places in old Texture Graph code base.
	// For now, just removing the code.
	// We will do the refactor later.
	// TODO: We need to remove this function
	check(false);
}

AsyncBufferResultPtr TargetTextureSet::BindTo(RenderMaterial_BPPtr Material, TArray<FMaterialMappingInfo> MaterialMappingInfo)
{
	RenderCount++;

	for(const FMaterialMappingInfo MappingInfo : MaterialMappingInfo)
	{
		BindOnTextureUpdate(Material, MappingInfo);
	}
			
	return cti::make_ready_continuable(std::make_shared<BufferResult>());
}

AsyncBufferResultPtr TargetTextureSet::BindTo(RenderMaterial_BPPtr Material, FMaterialMappingInfo MaterialMappingInfo)
{
	RenderCount++;

	BindOnTextureUpdate(Material, MaterialMappingInfo);
	
	return cti::make_ready_continuable(std::make_shared<BufferResult>());
}

AsyncBufferResultPtr TargetTextureSet::UnbindFrom(RenderMaterial_BPPtr Material)
{
	ResourceBindInfo BindInfo;
	BindInfo.Dev = Device_FX::Get();

	for(auto TextureEntry : TexturesMap)
	{	
		if (TexturesMap.Contains(TextureEntry.Key) && !TexturesMap[TextureEntry.Key].expired())
		{
			
			BindInfo.Target = TextureEntry.Key.ToString();
			TexturesMap[TextureEntry.Key].lock()->Unbind(Material.get(), BindInfo);
		}
	}

	return cti::make_ready_continuable(std::make_shared<BufferResult>());
}

void TargetTextureSet::SetMesh(RenderMeshPtr InMesh)
{
	Mesh = InMesh;
}

void TargetTextureSet::SetTexture(FName TextureName, TiledBlobRef InTexture)
{
	TextureSet::SetTexture(TextureName, InTexture);

	if(BoundTexturesMap.Contains(TextureName))
	{
		BoundTexturesMap[TextureName] = InTexture;
	}
	else
	{
		BoundTexturesMap.Add(TextureName, InTexture);
	}

	//if there exists a callback, now is the time to update it 
	if (Callbacks.Contains(TextureName) && Callbacks[TextureName])
	{
		Callbacks[TextureName](InTexture);
	}
}

void TargetTextureSet::FreeAt(FName TextureName)
{
	UnRegisterCallback(TextureName);
	
	if(BoundTexturesMap.Contains(TextureName))
	{
		BoundTexturesMap.Remove(TextureName);
	}
	
	TextureSet::FreeAt(TextureName);
}

void TargetTextureSet::Free()
{
	for(auto TextureEntry : BoundTexturesMap)
	{
		BoundTexturesMap[TextureEntry.Key] = TiledBlobRef();
	}

	BoundTexturesMap.Empty();
	
	TextureSet::Free();
}

void TargetTextureSet::BindOnTextureUpdate(RenderMaterial_BPPtr InMaterial, FMaterialMappingInfo MaterialMappingInfo) const
{
	TextureReadyCallback bindCallback = [this, InMaterial, MaterialMappingInfo](TiledBlobRef texture) mutable
	{
		Util::OnGameThread([this, texture, InMaterial, MaterialMappingInfo]()
			{
				ResourceBindInfo BindInfo;
				BindInfo.Dev = Device_FX::Get();
				if (texture)
				{
					BindInfo.Target = MaterialMappingInfo.MaterialInput.ToString();
					//TODO: Need to move it into the Resource Bind Info

					// Linear textures are now being displayed in linear gamma.
					// So turning off the conversion to match UE's convention.
					// InMaterial->SetInt(TEXT("sRGB"), texture.get()->GetDescriptor().bIsSRGB ? 1 : 0);

					InMaterial->SetInt(TEXT("IsGrayscale"), texture.get()->GetDescriptor().ItemsPerPoint == 1 ? 1 : 0);

					texture->OnFinalise().then([=]()
					{
						texture->Bind(InMaterial.get(), BindInfo);
					});
					
				}
			});
	};

	if(MaterialMappingInfo.HasTarget())
	{
		RegisterCallback(bindCallback, MaterialMappingInfo);
	}
}

void TargetTextureSet::RegisterCallback(TextureReadyCallback Callback, FMaterialMappingInfo MaterialMappingInfo) const
{
	//If we have something in bound, let the callback know now
	if (BoundTexturesMap.Contains(MaterialMappingInfo.Target))
	{
		Callback(BoundTexturesMap[MaterialMappingInfo.Target]);
	}

	/// And also put it in an array incase it gets updated

	if(Callbacks.Contains(MaterialMappingInfo.Target))
	{
		Callbacks[MaterialMappingInfo.Target] = Callback;
	}
	else
	{
		Callbacks.Add(MaterialMappingInfo.Target, Callback);
	}
}

void TargetTextureSet::UnRegisterCallback(FName TextureName) const
{
	if(Callbacks.Contains(TextureName))
	{
		Callbacks.Remove(TextureName);
	}
}

