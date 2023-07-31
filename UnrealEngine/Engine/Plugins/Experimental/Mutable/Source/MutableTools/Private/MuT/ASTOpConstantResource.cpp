// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantResource.h"

#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Hash/CityHash.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Serialisation.h"
#include "MuR/Skeleton.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConstantResource::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpConstantResource*>(&otherUntyped))
		{
			return type == other->type && hash == other->hash &&
				loadedValue == other->loadedValue && proxy == other->proxy;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpConstantResource::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantResource> n = new ASTOpConstantResource();
		n->type = type;
		n->proxy = proxy;
		n->loadedValue = loadedValue;
		n->hash = hash;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpConstantResource::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(type));
		hash_combine(res, hash);
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::Link(PROGRAM& program, const FLinkerOptions* Options)
	{
		if (!linkedAddress && !bLinkedAndNull)
		{
			if (type == OP_TYPE::ME_CONSTANT)
			{
				OP::MeshConstantArgs args;
				memset(&args, 0, sizeof(args));

				Ptr<Mesh> pTyped = static_cast<const Mesh*>(GetValue().get())->Clone();
				check(pTyped);

				args.skeleton = -1;
				if (Ptr<const Skeleton> pSkeleton = pTyped->GetSkeleton())
				{
					// See if it can be merged into another skeleton
	//                for (const auto& pOtherSkeleton : program.m_constantSkeletons)
	//                {
	//                    if (true)
	//                    //if (AreSkeletonsRelated(pOtherSkeleton, pSkeleton))
	//                    {
	//                        // \todo : ugly const hack
	//                        ExtendSkeleton( const_cast<Skeleton*>(pOtherSkeleton.get()),
	//                                        pSkeleton.get() );
	//                        pSkeleton = pOtherSkeleton;

	//                        Ptr<Mesh> pRemapped = MeshRemapSkeleton( pTyped.get(),
	//                                                                 pOtherSkeleton.get() );
	//                        if (pRemapped)
	//                        {
	//                            pTyped = pRemapped;
	//                        }
	//                        break;
	//                    }
	//                }

					args.skeleton = program.AddConstant(pSkeleton.get());
					pTyped->SetSkeleton(nullptr);
				}

				args.physicsBody = -1;
				if (Ptr<const PhysicsBody> pPhysicsBody = pTyped->GetPhysicsBody())
				{
					args.physicsBody = program.AddConstant(pPhysicsBody.get());
					pTyped->SetPhysicsBody(nullptr);
				}

				args.value = program.AddConstant(pTyped.get());

				linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
				program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
				AppendCode(program.m_byteCode, type);
				AppendCode(program.m_byteCode, args);
			}
			else
			{
				OP::ResourceConstantArgs args;
				memset(&args, 0, sizeof(args));

				bool bValidData = true;

				switch (type)
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
						// TODO: Compilation options?
						int32 MinTextureResidentMipCount = Options->MinTextureResidentMipCount;
						args.value = program.AddConstant(pTyped, MinTextureResidentMipCount);
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
					program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
					AppendCode(program.m_byteCode, type);
					AppendCode(program.m_byteCode, args);
				}
				else
				{
					// Null op
					linkedAddress = 0;
					bLinkedAndNull = true;
				}
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpConstantResource::GetImageDesc(bool, class GetImageDescContext*)
	{
		FImageDesc res;

		if (type == OP_TYPE::IM_CONSTANT)
		{
			// TODO: cache to avoid disk loading
			Ptr<const Image> pConst = static_cast<const Image*>(GetValue().get());
			res.m_format = pConst->m_format;
			res.m_lods = pConst->m_lods;
			res.m_size = pConst->m_size;
		}
		else
		{
			check(false);
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantResource::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY,
		BLOCK_LAYOUT_SIZE_CACHE*)
	{
		switch (type)
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
		switch (type)
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
		if (type == OP_TYPE::IM_CONSTANT)
		{
			// TODO: cache
			Ptr<const Image> pMask = static_cast<const Image*>(GetValue().get());
			pMask->GetNonBlackRect(maskUsage);
			return true;
		}

		return false;
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpConstantResource::IsImagePlainConstant(vec4<float>& colour) const
	{
		bool res = false;
		switch (type)
		{

		case OP_TYPE::IM_CONSTANT:
		{
			Ptr<const Image> pImage = static_cast<const Image*>(GetValue().get());
			check(pImage->m_size[0] > 0);
			check(pImage->m_size[1] > 0);

			if (pImage->m_flags & Image::IF_IS_PLAIN_COLOUR_VALID)
			{
				if (pImage->m_flags & Image::IF_IS_PLAIN_COLOUR)
				{
					res = true;
					colour = pImage->Sample(vec2<float>(0, 0));
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
		return hash;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<const RefCounted> ASTOpConstantResource::GetValue() const
	{
		if (loadedValue)
		{
			return loadedValue;
		}
		else
		{
			switch (type)
			{

			case OP_TYPE::IM_CONSTANT:
			{
				Ptr<ResourceProxy<Image>> typedProxy = dynamic_cast<ResourceProxy<Image>*>(proxy.get());
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
	void ASTOpConstantResource::SetValue(const Ptr<const RefCounted>& v,
		bool useDiskCache)
	{
		switch (type)
		{
		case OP_TYPE::IM_CONSTANT:
		{
			Ptr<const Image> r = static_cast<const Image*>(v.get());

			OutputMemoryStream stream(r->GetDataSize() + 1024);
			OutputArchive arch(&stream);
			Image::Serialise(r.get(), arch);

			hash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			if (useDiskCache)
			{
				proxy = new ResourceProxyTempFile<Image>(r.get());
			}
			else
			{
				loadedValue = r;
			}
			break;
		}

		case OP_TYPE::ME_CONSTANT:
		{
			OutputMemoryStream stream;
			OutputArchive arch(&stream);

			Ptr<const Mesh> r = static_cast<const Mesh*>(v.get());
			Mesh::Serialise(r.get(), arch);

			hash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			loadedValue = v;
			break;
		}

		case OP_TYPE::LA_CONSTANT:
		{
			OutputMemoryStream stream;
			OutputArchive arch(&stream);

			Ptr<const Layout> r = static_cast<const Layout*>(v.get());
			Layout::Serialise(r.get(), arch);

			hash = CityHash64(static_cast<const char*>(stream.GetBuffer()), stream.GetBufferSize());

			loadedValue = v;
			break;
		}

		default:
			loadedValue = v;
			break;
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpConstantResource::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
		pRes->type = ImageSizeExpression::ISET_CONSTANT;

		switch (type)
		{
		case OP_TYPE::IM_CONSTANT:
		{
			Ptr<const Image> pConst = static_cast<const Image*>(GetValue().get());
			pRes->size = pConst->m_size;
			break;
		}

		default:
			check(false);
			return nullptr;
		}

		return pRes;
	}

}