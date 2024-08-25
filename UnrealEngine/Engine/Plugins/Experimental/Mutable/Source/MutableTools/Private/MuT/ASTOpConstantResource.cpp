// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantResource.h"

#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Hash/CityHash.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Serialisation.h"
#include "MuR/Skeleton.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConstantResource::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpConstantResource* Other = static_cast<const ASTOpConstantResource*>(&OtherUntyped);
			return Type == Other->Type && ValueHash == Other->ValueHash &&
				LoadedValue == Other->LoadedValue && Proxy == Other->Proxy;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpConstantResource::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantResource> n = new ASTOpConstantResource();
		n->Type = Type;
		n->Proxy = Proxy;
		n->LoadedValue = LoadedValue;
		n->ValueHash = ValueHash;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpConstantResource::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(Type));
		hash_combine(res, ValueHash);
		return res;
	}


	namespace
	{
		/** Adds a constant image data to a program and returns its constant index. */
		int32 AddConstantImage(FProgram& Program, const Ptr<const Image>& pImage, FLinkerOptions& Options)
		{
			MUTABLE_CPUPROFILER_SCOPE(AddConstantImage);

			check(pImage->GetSizeX() * pImage->GetSizeY() > 0);
			
			// Mips to store
			int32 MipsToStore = 1;

			int32 FirstLODIndexIndex = Program.m_constantImageLODIndices.Num();

			FImageOperator& ImOp = Options.ImageOperator;
			Ptr<const Image> pMip;

			if (!Options.bSeparateImageMips)
			{
				pMip = pImage;
			}
			else
			{
				// We may want the full mipmaps for fragments of images, regardless of the resident mip size, for intermediate operations.
				// \TODO: Calculate the mip ranges that makes sense to store.
				int32 MaxMipmaps = Image::GetMipmapCount(pImage->GetSizeX(), pImage->GetSizeY());
				MipsToStore = MaxMipmaps;

				// Some images cannot be resized or mipmaped
				bool bCannotBeScaled = pImage->m_flags & Image::IF_CANNOT_BE_SCALED;
				if (bCannotBeScaled)
				{
					// Store only the mips that we have already calculated. We assume we have calculated them correctly.
					MipsToStore = pImage->GetLODCount();
				}

				// TODO: If the image already has mips, we will be duplicating them...
				if (pImage->GetLODCount() == 1)
				{
					pMip = pImage;
				}
				else
				{
					pMip = ImOp.ExtractMip(pImage.get(), 0);
				}
			}

			for (int Mip = 0; Mip < MipsToStore; ++Mip)
			{
				check(pMip->GetFormat() == pImage->GetFormat());

				// Ensure unique at mip level
				int32 MipIndex = -1;

				// Use a map-based deduplication only if we are splitting mips.
				if (Options.bSeparateImageMips)
				{
					MUTABLE_CPUPROFILER_SCOPE(Deduplicate);

					const int32* IndexPtr = Options.ImageConstantMipMap.Find(pMip);
					if (IndexPtr)
					{
						MipIndex = *IndexPtr;
					}
				}

				if (MipIndex<0)
				{
					MipIndex = Program.ConstantImageLODs.Add(TPair<int32, Ptr<const Image>>(-1, pMip));
					Options.ImageConstantMipMap.Add(pMip, MipIndex);
				}

				Program.m_constantImageLODIndices.Add(uint32(MipIndex));

				// Generate next mip if necessary
				if (Mip + 1 < MipsToStore)
				{
					Ptr<Image> NewMip;
					if (Mip > pImage->GetLODCount())
					{
						// Generate from the last mip.
						NewMip = ImOp.ExtractMip(pMip.get(), 1);
					}
					else
					{
						NewMip = ImOp.ExtractMip(pImage.get(), Mip + 1);
					}
					check(NewMip);

					pMip = NewMip;
				}
			}

			FImageLODRange LODRange;
			LODRange.FirstIndex = FirstLODIndexIndex;
			LODRange.LODCount = MipsToStore;
			LODRange.ImageFormat = pImage->GetFormat();
			LODRange.ImageSizeX = pImage->GetSizeX();
			LODRange.ImageSizeY = pImage->GetSizeY();
			int32 ImageIndex = Program.m_constantImages.Add(LODRange);
			return ImageIndex;
		}
	}

	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::Link(FProgram& program, FLinkerOptions* Options)
	{
		MUTABLE_CPUPROFILER_SCOPE(ASTOpConstantResource_Link);

		if (!linkedAddress && !bLinkedAndNull)
		{
			if (Type == OP_TYPE::ME_CONSTANT)
			{
				OP::MeshConstantArgs args;
				FMemory::Memset(&args, 0, sizeof(args));

				Ptr<Mesh> MeshData = static_cast<const Mesh*>(GetValue().get())->Clone();
				check(MeshData);

				args.skeleton = -1;
				if (Ptr<const Skeleton> pSkeleton = MeshData->GetSkeleton())
				{
					args.skeleton = program.AddConstant(pSkeleton.get());
					MeshData->SetSkeleton(nullptr);
				}

				args.physicsBody = -1;
				if (Ptr<const PhysicsBody> pPhysicsBody = MeshData->GetPhysicsBody())
				{
					args.physicsBody = program.AddConstant(pPhysicsBody.get());
					MeshData->SetPhysicsBody(nullptr);
				}

				// Use a map-based deduplication
				mu::Ptr<const mu::Mesh> Key = MeshData;
				const int32* IndexPtr = Options->MeshConstantMap.Find(Key);
				if (!IndexPtr)
				{
					args.value = program.AddConstant(MeshData.get());
					Options->MeshConstantMap.Add(MeshData, int32(args.value));
				}
				else
				{
					args.value = *IndexPtr;
				}

				linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
				program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
				AppendCode(program.m_byteCode, Type);
				AppendCode(program.m_byteCode, args);
			}
			else
			{
				OP::ResourceConstantArgs args;
				FMemory::Memset(&args, 0, sizeof(args));

				bool bValidData = true;

				switch (Type)
				{
				case OP_TYPE::IM_CONSTANT:
				{
					Ptr<const Image> pTyped = static_cast<const Image*>(GetValue().get());
					check(pTyped);

					if (pTyped->GetSizeX() * pTyped->GetSizeY() == 0)
					{
						// It's an empty or degenerated image, return a null operation.
						bValidData = false;
					}
					else
					{
						args.value = AddConstantImage( program, pTyped, *Options);
					}

					break;
				}
				case OP_TYPE::LA_CONSTANT:
				{
					Ptr<const Layout> pTyped = static_cast<const Layout*>(GetValue().get());
					check(pTyped);
					args.value = program.AddConstant(pTyped);
					break;
				}
				default:
					check(false);
				}

				if (bValidData)
				{
					linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
					program.m_opAddress.Add((uint32)program.m_byteCode.Num());
					AppendCode(program.m_byteCode, Type);
					AppendCode(program.m_byteCode, args);
				}
				else
				{
					// Null op
					linkedAddress = 0;
					bLinkedAndNull = true;
				}
			}

			// Clear stored value to reduce memory usage.
			LoadedValue = nullptr;
			Proxy = nullptr;
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpConstantResource::GetImageDesc(bool, class FGetImageDescContext*) const
	{
		FImageDesc Result;

		if (Type == OP_TYPE::IM_CONSTANT)
		{
			// TODO: cache to avoid disk loading
			Ptr<const Image> ConstImage = static_cast<const Image*>(GetValue().get());
			Result.m_format = ConstImage->GetFormat();
			Result.m_lods = ConstImage->GetLODCount();
			Result.m_size = ConstImage->GetSize();
		}
		else
		{
			check(false);
		}

		return Result;
	}

	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache*)
	{
		switch (Type)
		{
		case OP_TYPE::LA_CONSTANT:
		{
			Ptr<const Layout> pLayout = static_cast<const Layout*>(GetValue().get());
			check(pLayout);

			if (pLayout)
			{
				int relId = pLayout->FindBlock(blockIndex);
				if (relId >= 0)
				{
					*pBlockX = pLayout->m_blocks[relId].m_size[0];
					*pBlockY = pLayout->m_blocks[relId].m_size[1];
				}
				else
				{
					*pBlockX = 0;
					*pBlockY = 0;
				}
			}

			break;
		}
		default:
			check(false);
		}
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		switch (Type)
		{

		case OP_TYPE::IM_CONSTANT:
		{
			// We didn't find any layout.
			*pBlockX = 0;
			*pBlockY = 0;
			break;
		}

		default:
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConstantResource::GetNonBlackRect(FImageRect& maskUsage) const
	{
		if (Type == OP_TYPE::IM_CONSTANT)
		{
			// TODO: cache
			Ptr<const Image> pMask = static_cast<const Image*>(GetValue().get());
			pMask->GetNonBlackRect(maskUsage);
			return true;
		}

		return false;
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConstantResource::IsImagePlainConstant(FVector4f& colour) const
	{
		bool res = false;
		switch (Type)
		{

		case OP_TYPE::IM_CONSTANT:
		{
			Ptr<const Image> pImage = static_cast<const Image*>(GetValue().get());
			if (pImage->GetSizeX() <= 0 || pImage->GetSizeY() <= 0)
			{
				res = true;
				colour = FVector4f(0.0f,0.0f,0.0f,1.0f);
			}
			else if (pImage->m_flags & Image::IF_IS_PLAIN_COLOUR_VALID)
			{
				if (pImage->m_flags & Image::IF_IS_PLAIN_COLOUR)
				{
					res = true;
					colour = pImage->Sample(FVector2f(0, 0));
				}
				else
				{
					res = false;
				}
			}
			else
			{
				if (pImage->IsPlainColour(colour))
				{
					res = true;
					pImage->m_flags |= Image::IF_IS_PLAIN_COLOUR;
				}

				pImage->m_flags |= Image::IF_IS_PLAIN_COLOUR_VALID;
			}
			break;
		}

		default:
			break;
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpConstantResource::~ASTOpConstantResource()
	{
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpConstantResource::GetValueHash() const
	{
		return ValueHash;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<const RefCounted> ASTOpConstantResource::GetValue() const
	{
		if (LoadedValue)
		{
			return LoadedValue;
		}
		else
		{
			switch (Type)
			{

			case OP_TYPE::IM_CONSTANT:
			{
				Ptr<ResourceProxy<Image>> typedProxy = static_cast<ResourceProxy<Image>*>(Proxy.get());
				Ptr<const Image> r = typedProxy->Get();
				return r;
			}

			default:
				check(false);
				break;
			}
		}

		return nullptr;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::SetValue(const Ptr<const RefCounted>& v, FProxyFileContext* DiskCacheContext)
	{
		switch (Type)
		{
		case OP_TYPE::IM_CONSTANT:
		{
			Ptr<const Image> r = static_cast<const Image*>(v.get());

			OutputMemoryStream stream(r->GetDataSize() + 1024);
			OutputArchive arch(&stream);
			Image::Serialise(r.get(), arch);

			ValueHash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			if (DiskCacheContext)
			{
				Proxy = new ResourceProxyTempFile<Image>(r.get(), *DiskCacheContext);
			}
			else
			{
				LoadedValue = r;
			}
			break;
		}

		case OP_TYPE::ME_CONSTANT:
		{
			OutputMemoryStream stream;
			OutputArchive arch(&stream);

			Ptr<const Mesh> r = static_cast<const Mesh*>(v.get());
			Mesh::Serialise(r.get(), arch);

			ValueHash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			LoadedValue = v;
			break;
		}

		case OP_TYPE::LA_CONSTANT:
		{
			OutputMemoryStream stream;
			OutputArchive arch(&stream);

			Ptr<const Layout> r = static_cast<const Layout*>(v.get());
			Layout::Serialise(r.get(), arch);

			ValueHash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			LoadedValue = v;
			break;
		}

		default:
			LoadedValue = v;
			break;
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpConstantResource::GetImageSizeExpression() const
	{
		if (Type==OP_TYPE::IM_CONSTANT)
		{
			Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
			pRes->type = ImageSizeExpression::ISET_CONSTANT;
			Ptr<const Image> pConst = static_cast<const Image*>(GetValue().get());
			pRes->size = pConst->GetSize();
			return pRes;
		}

		return nullptr;
	}

}
