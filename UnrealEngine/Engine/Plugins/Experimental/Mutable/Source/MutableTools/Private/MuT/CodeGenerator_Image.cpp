// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/ImagePrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpImageProject.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpReferenceResource.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpImageNormalComposite.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageTransform.h"
#include "MuT/ASTOpImageMakeGrowMap.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpMeshExtractLayoutBlocks.h"
#include "MuT/ASTOpMeshFormat.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/Compiler.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/ImageDescGenerator.h"
#include "MuT/Node.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/NodeImageBinarisePrivate.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/NodeImageColourMapPrivate.h"
#include "MuT/NodeImageConditional.h"
#include "MuT/NodeImageConditionalPrivate.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeImageConstantPrivate.h"
#include "MuT/NodeImageReferencePrivate.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageFormatPrivate.h"
#include "MuT/NodeImageGradient.h"
#include "MuT/NodeImageGradientPrivate.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInterpolatePrivate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageInvertPrivate.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageLayerColourPrivate.h"
#include "MuT/NodeImageLayerPrivate.h"
#include "MuT/NodeImageLuminance.h"
#include "MuT/NodeImageLuminancePrivate.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageMipmapPrivate.h"
#include "MuT/NodeImageMultiLayer.h"
#include "MuT/NodeImageMultiLayerPrivate.h"
#include "MuT/NodeImageNormalComposite.h"
#include "MuT/NodeImageNormalCompositePrivate.h"
#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImageParameterPrivate.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImagePlainColourPrivate.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageProjectPrivate.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageResizePrivate.h"
#include "MuT/NodeImageSaturate.h"
#include "MuT/NodeImageSaturatePrivate.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwitchPrivate.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageSwizzlePrivate.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeImageTablePrivate.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/NodeImageTransformPrivate.h"
#include "MuT/NodeImageVariation.h"
#include "MuT/NodeImageVariationPrivate.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImagePtrConst& Untyped)
	{
		if (!Untyped)
		{
			Result = FImageGenerationResult();
			return;
		}

		// See if it was already generated
		FGeneratedImageCacheKey key;
		key.Options = Options;
		key.Node = Untyped;
		GeneratedImagesMap::ValueType* CachedPtr = m_generatedImages.Find(key);
		if (CachedPtr)
		{
			Result = *CachedPtr;
		}
		else
		{ 
			const NodeImage* Node = Untyped.get();

			// Generate for each different type of node
			switch (Untyped->GetImageNodeType())
			{
			case NodeImage::EType::Constant: GenerateImage_Constant(Options, Result, static_cast<const NodeImageConstant*>(Node)); break;
			case NodeImage::EType::Difference_Deprecated: check(false); break;
			case NodeImage::EType::Interpolate: GenerateImage_Interpolate(Options, Result, static_cast<const NodeImageInterpolate*>(Node)); break;
			case NodeImage::EType::Saturate: GenerateImage_Saturate(Options, Result, static_cast<const NodeImageSaturate*>(Node)); break;
			case NodeImage::EType::Table: GenerateImage_Table(Options, Result, static_cast<const NodeImageTable*>(Node)); break;
			case NodeImage::EType::Swizzle: GenerateImage_Swizzle(Options, Result, static_cast<const NodeImageSwizzle*>(Node)); break;
			case NodeImage::EType::ColourMap: GenerateImage_ColourMap(Options, Result, static_cast<const NodeImageColourMap*>(Node)); break;
			case NodeImage::EType::Gradient: GenerateImage_Gradient(Options, Result, static_cast<const NodeImageGradient*>(Node)); break;
			case NodeImage::EType::Binarise: GenerateImage_Binarise(Options, Result, static_cast<const NodeImageBinarise*>(Node)); break;
			case NodeImage::EType::Luminance: GenerateImage_Luminance(Options, Result, static_cast<const NodeImageLuminance*>(Node)); break;
			case NodeImage::EType::Layer: GenerateImage_Layer(Options, Result, static_cast<const NodeImageLayer*>(Node)); break;
			case NodeImage::EType::LayerColour: GenerateImage_LayerColour(Options, Result, static_cast<const NodeImageLayerColour*>(Node)); break;
			case NodeImage::EType::Resize: GenerateImage_Resize(Options, Result, static_cast<const NodeImageResize*>(Node)); break;
			case NodeImage::EType::PlainColour: GenerateImage_PlainColour(Options, Result, static_cast<const NodeImagePlainColour*>(Node)); break;
			case NodeImage::EType::Project: GenerateImage_Project(Options, Result, static_cast<const NodeImageProject*>(Node)); break;
			case NodeImage::EType::Mipmap: GenerateImage_Mipmap(Options, Result, static_cast<const NodeImageMipmap*>(Node)); break;
			case NodeImage::EType::Switch: GenerateImage_Switch(Options, Result, static_cast<const NodeImageSwitch*>(Node)); break;
			case NodeImage::EType::Conditional: GenerateImage_Conditional(Options, Result, static_cast<const NodeImageConditional*>(Node)); break;
			case NodeImage::EType::Format: GenerateImage_Format(Options, Result, static_cast<const NodeImageFormat*>(Node)); break;
			case NodeImage::EType::Parameter: GenerateImage_Parameter(Options, Result, static_cast<const NodeImageParameter*>(Node)); break;
			case NodeImage::EType::MultiLayer: GenerateImage_MultiLayer(Options, Result, static_cast<const NodeImageMultiLayer*>(Node)); break;
			case NodeImage::EType::Invert: GenerateImage_Invert(Options, Result, static_cast<const NodeImageInvert*>(Node)); break;
			case NodeImage::EType::Variation: GenerateImage_Variation(Options, Result, static_cast<const NodeImageVariation*>(Node)); break;
			case NodeImage::EType::NormalComposite: GenerateImage_NormalComposite(Options, Result, static_cast<const NodeImageNormalComposite*>(Node)); break;
			case NodeImage::EType::Transform: GenerateImage_Transform(Options, Result, static_cast<const NodeImageTransform*>(Node)); break;
			case NodeImage::EType::Reference: GenerateImage_Reference(Options, Result, static_cast<const NodeImageReference*>(Node)); break;
			case NodeImage::EType::None: check(false);
			}

			// Cache the Result
			m_generatedImages.Add(key, Result);
		}
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateImage_Constant(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageConstant* InNode)
    {
		const NodeImageConstant::Private& node = *InNode->GetPrivate();
		
        Ptr<ASTOpConstantResource> op = new ASTOpConstantResource();
        op->type = OP_TYPE::IM_CONSTANT;

        // TODO: check duplicates
        Ptr<const Image> pImage;
		if (node.m_pProxy)
		{
			pImage = node.m_pProxy->Get();
		}

        if (!pImage)
        {
            // This data is required
            pImage = GenerateMissingImage(EImageFormat::IF_RGB_UBYTE );

            // Log an error message
            m_pErrorLog->GetPrivate()->Add( "Constant image not set.", ELMT_WARNING, node.m_errorContext );
        }

		if (Options.ImageLayoutStrategy!= CompilerOptions::TextureLayoutStrategy::None && Options.LayoutToApply)
		{
			// We want to generate only a block from the image.

			FIntVector2 SourceImageSize(pImage->GetSizeX(), pImage->GetSizeY());

			int32 BlockIndex = Options.LayoutToApply->FindBlock(Options.LayoutBlockId);
			check( BlockIndex>=0 );

			// Block in layout grid units
			box< UE::Math::TIntVector2<uint16> > RectInCells;
			Options.LayoutToApply->GetBlock
			(
				BlockIndex,
				&RectInCells.min[0], &RectInCells.min[1],
				&RectInCells.size[0], &RectInCells.size[1]
			);

			FIntPoint grid = Options.LayoutToApply->GetGridSize();
			grid[0] = FMath::Max(1, grid[0]);
			grid[1] = FMath::Max(1, grid[1]);

			// Transform to pixels
			box< UE::Math::TIntVector2<int32> > rect;
			rect.min[0]  = (RectInCells.min[0]  * SourceImageSize[0]) / grid[0];
			rect.min[1]  = (RectInCells.min[1]  * SourceImageSize[1]) / grid[1];
			rect.size[0] = (RectInCells.size[0] * SourceImageSize[0]) / grid[0];
			rect.size[1] = (RectInCells.size[1] * SourceImageSize[1]) / grid[1];

			// Do we need to crop?
			if (rect.min[0]!=0 || rect.min[1]!=0 || pImage->GetSizeX() != rect.size[0] || pImage->GetSizeY() != rect.size[1])
			{
				FImageOperator ImOp
				(
					[](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i) { return new Image(x, y, m, f, i); },
					[](Ptr<Image>& i) {i = nullptr; },
					[](const Image* i) { return i->Clone(); }
				);


				Ptr<Image> pCropped = new Image(rect.size[0], rect.size[1], 1, pImage->GetFormat(), EInitializationType::NotInitialized);
				ImOp.ImageCrop(pCropped.get(), m_compilerOptions->ImageCompressionQuality, pImage.get(), rect);
				op->SetValue(pCropped, m_compilerOptions->OptimisationOptions.bUseDiskCache);
			}
			else
			{
				// No need to crop.
				op->SetValue(pImage, m_compilerOptions->OptimisationOptions.bUseDiskCache);
			}
		}
        else
        {
            op->SetValue( pImage, m_compilerOptions->OptimisationOptions.bUseDiskCache );
        }

		Result.op = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Reference(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageReference* InNode)
	{
		const NodeImageReference::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpReferenceResource> op = new ASTOpReferenceResource();
		op->type = OP_TYPE::IM_REFERENCE;
		op->ID = node.ImageReferenceID;

		// TODO: check no crop
		Result.op = op;
	}


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Parameter(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageParameter* InNode )
    {
		const NodeImageParameter::Private& node = *InNode->GetPrivate();

        Ptr<ASTOpParameter> op;

        auto it = m_nodeVariables.find( node.m_pNode );
        if ( it == m_nodeVariables.end() )
        {
            op = new ASTOpParameter();
            op->type = OP_TYPE::IM_PARAMETER;

			op->parameter.m_name = node.m_name;
			op->parameter.m_uid = node.m_uid;
            op->parameter.m_type = PARAMETER_TYPE::T_IMAGE;
        	op->parameter.m_defaultValue.Set<ParamImageType>(FName());

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				FRangeGenerationResult rangeResult;
				GenerateRange(rangeResult, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

            m_nodeVariables[node.m_pNode] = op;
        }
        else
        {
            op = it->second;
        }

		Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Layer(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageLayer* InNode)
	{
		const NodeImageLayer::Private& node = *InNode->GetPrivate();
		
		MUTABLE_CPUPROFILER_SCOPE(NodeImageLayer);

        Ptr<ASTOpImageLayer> op = new ASTOpImageLayer();

        op->blendType = node.m_type;

        // Base image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
        }
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image Layer base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

		UE::Math::TIntVector2<int32> TargetSize = Options.RectSize;

		EImageFormat baseFormat = base->GetImageDesc( true ).m_format;
        //base = GenerateImageFormat( base, EImageFormat::IF_RGB_UBYTE );
        base = GenerateImageSize( base, TargetSize);
        op->base = base;

        // Mask of the effect
        Ptr<ASTOp> mask;
        if ( node.m_pMask )
        {
			FImageGenerationResult MaskResult;
			GenerateImage(Options, MaskResult, node.m_pMask);
			mask = MaskResult.op;

            mask = GenerateImageFormat( mask, EImageFormat::IF_L_UBYTE );
            mask = GenerateImageSize( mask, TargetSize);
        }
        op->mask = mask;

        // Image to apply
        Ptr<ASTOp> blended = 0;
        if ( node.m_pBlended )
        {
			FImageGenerationResult BlendedResult;
			GenerateImage(Options, BlendedResult, node.m_pBlended);
			blended = BlendedResult.op;
        }
        else
        {
            // This argument is required
            blended = GeneratePlainImageCode(FVector4f( 1,1,0,1 ), Options );
        }
        //blended = GenerateImageFormat( blended, EImageFormat::IF_RGB_UBYTE );
        blended = GenerateImageFormat( blended, baseFormat );
        blended = GenerateImageSize( blended, TargetSize);
        op->blend = blended;

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_LayerColour(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageLayerColour* InNode)
	{
		const NodeImageLayerColour::Private& node = *InNode->GetPrivate();
		
		MUTABLE_CPUPROFILER_SCOPE(NodeImageLayerColour);

        Ptr<ASTOpImageLayerColor> op = new ASTOpImageLayerColor();
		op->blendType = node.m_type;

        // Base image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
        }
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Layer base image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options );
        }
        base = GenerateImageFormat( base, EImageFormat::IF_RGB_UBYTE );
        base = GenerateImageSize( base,Options.RectSize );
        op->base = base;

        // Mask of the effect
        Ptr<ASTOp> mask;
        if ( node.m_pMask )
        {
			FImageGenerationResult MaskResult;
			GenerateImage(Options, MaskResult, node.m_pMask);
			mask = MaskResult.op;
			
			mask = GenerateImageFormat( mask, EImageFormat::IF_L_UBYTE );
            mask = GenerateImageSize( mask, Options.RectSize);
        }
        op->mask = mask;

        // Colour to apply
        Ptr<ASTOp> colour = 0;
        if ( node.m_pColour )
        {
			FColorGenerationResult ColorResult;
            GenerateColor(ColorResult, node.m_pColour);
			colour = ColorResult.op;
        }
        else
        {
            // This argument is required
            colour = GenerateMissingColourCode(TEXT("Layer colour"), node.m_errorContext );
        }
        op->color = colour;

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_MultiLayer(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageMultiLayer* InNode)
	{
		const NodeImageMultiLayer::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageMultiLayer);

        Ptr<ASTOpImageMultiLayer> op = new ASTOpImageMultiLayer();

		op->blendType = node.m_type;

        // Base image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image MultiLayer base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

		EImageFormat baseFormat = base->GetImageDesc().m_format;
        base = GenerateImageSize( base, Options.RectSize);
        op->base = base;

        // Mask of the effect
        Ptr<ASTOp> mask;
        if ( node.m_pMask )
        {
			FImageGenerationResult MaskResult;
			GenerateImage(Options, MaskResult, node.m_pMask);
			mask = MaskResult.op;

			mask = GenerateImageFormat( mask, EImageFormat::IF_L_UBYTE );
            mask = GenerateImageSize( mask, Options.RectSize);
        }
        op->mask = mask;

        // Image to apply
        Ptr<ASTOp> blended;
        if (node.m_pBlended)
        {
			FImageGenerationResult MaskResult;
			GenerateImage(Options, MaskResult, node.m_pBlended);
			blended = MaskResult.op;
        }
        else
        {
            // This argument is required
            blended = GeneratePlainImageCode(FVector4f( 1,1,0,1 ), Options);
        }
        blended = GenerateImageFormat( blended, baseFormat );
        blended = GenerateImageSize( blended, Options.RectSize);
        op->blend = blended;

        // Range of iteration
        if ( node.m_pRange )
        {
            FRangeGenerationResult rangeResult;
            GenerateRange( rangeResult, node.m_pRange );

            op->range.rangeSize = rangeResult.sizeOp;
            op->range.rangeName = rangeResult.rangeName;
            op->range.rangeUID = rangeResult.rangeUID;
        }

        Result.op = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_NormalComposite(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageNormalComposite* InNode)
	{
		const NodeImageNormalComposite::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageNormalComposite);

        Ptr<ASTOpImageNormalComposite> op = new ASTOpImageNormalComposite();

		op->Mode = node.m_mode; 
		op->Power = node.m_power;

        // Base image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image Composite Base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

		EImageFormat baseFormat = base->GetImageDesc().m_format;
        base = GenerateImageSize( base, Options.RectSize);
        op->Base = base;

        Ptr<ASTOp> normal;
        if ( node.m_pNormal )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pNormal);
			normal = BaseResult.op;

            normal = GenerateImageFormat( normal, EImageFormat::IF_RGB_UBYTE );
            normal = GenerateImageSize( normal, Options.RectSize);
        }
		else
		{
            // This argument is required
            normal = GenerateMissingImageCode(TEXT("Image Composite Normal"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
		}

        op->Normal = normal;
        
		Result.op = op;
    }

	void CodeGenerator::GenerateImage_Transform(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageTransform* InNode)
    {
		const NodeImageTransform::Private& node = *InNode->GetPrivate();

        MUTABLE_CPUPROFILER_SCOPE(NodeImageTransform);

        Ptr<ASTOpImageTransform> op = new ASTOpImageTransform();

		Ptr<ASTOp> OffsetX;
		if (node.m_pOffsetX)
		{
			OffsetX = Generate(node.m_pOffsetX);
		}

		Ptr<ASTOp> OffsetY;
		if (node.m_pOffsetY)
		{
			OffsetY = Generate(node.m_pOffsetY);
		}
	
		Ptr<ASTOp> ScaleX;
		if (node.m_pScaleX)
		{
			ScaleX = Generate(node.m_pScaleX);
		}
	
		Ptr<ASTOp> ScaleY;
		if (node.m_pScaleY)
		{
			ScaleY = Generate(node.m_pScaleY);
		}

		Ptr<ASTOp> Rotation;
		if (node.m_pRotation)
		{
			Rotation = Generate(node.m_pRotation);
		}

		// If one of the inputs (offset or scale) is missig assume unifrom translation/scaling 
		op->offsetX = OffsetX ? OffsetX : OffsetY;
		op->offsetY = OffsetY ? OffsetY : OffsetX;
 		op->scaleX = ScaleX ? ScaleX : ScaleY;
		op->scaleY = ScaleY ? ScaleY : ScaleX;
		op->rotation = Rotation; 
		op->AddressMode = node.AddressMode;

        // Base image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image Transform Base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

		EImageFormat baseFormat = base->GetImageDesc().m_format;
        base = GenerateImageSize( base, Options.RectSize);
        op->base = base;

        Result.op = op; 
    }

    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Interpolate(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageInterpolate* InNode)
	{
		const NodeImageInterpolate::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageInterpolate);

        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_INTERPOLATE;

        // Factor
        if ( Node* pFactor = node.m_pFactor.get() )
        {
            op->SetChild( op->op.args.ImageInterpolate.factor, Generate( pFactor ));
        }
        else
        {
            // This argument is required
            op->SetChild( op->op.args.ImageInterpolate.factor,
                          GenerateMissingScalarCode(TEXT("Interpolation factor"), 0.5f, node.m_errorContext ));
        }

        // Target images
        int numTargets = 0;

        for ( std::size_t t=0
            ; t< node.m_targets.Num() && numTargets<MUTABLE_OP_MAX_INTERPOLATE_COUNT
            ; ++t )
        {
            if ( node.m_targets[t] )
            {
				FImageGenerationResult BaseResult;
				GenerateImage(Options, BaseResult, node.m_targets[t]);
				Ptr<ASTOp> target = BaseResult.op;

                // TODO: Support other formats
                target = GenerateImageFormat( target, EImageFormat::IF_RGB_UBYTE );
                target = GenerateImageSize( target, Options.RectSize);

                op->SetChild( op->op.args.ImageInterpolate.targets[numTargets], target);
                numTargets++;
            }
        }

        // At least one target is required
        if (!op->op.args.ImageInterpolate.targets[0])
        {
            Ptr<ASTOp> target = GenerateMissingImageCode(TEXT("First interpolation image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
            target = GenerateImageSize( target, Options.RectSize);
            op->SetChild( op->op.args.ImageInterpolate.targets[0], target);
        }

        Result.op = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Swizzle(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageSwizzle* InNode)
	{
		const NodeImageSwizzle::Private& node = *InNode->GetPrivate();

		//MUTABLE_CPUPROFILER_SCOPE(NodeImageSwizzle);

        // This node always produces a swizzle operation and sometimes it may produce a pixelformat
		// operation to compress the Result
        Ptr<ASTOpImageSwizzle> op = new ASTOpImageSwizzle();

		// Format
		EImageFormat compressedFormat = EImageFormat::IF_NONE;

		switch (node.m_format)
		{
        case EImageFormat::IF_BC1:
        case EImageFormat::IF_ASTC_4x4_RGB_LDR:
            compressedFormat = node.m_format;
            op->Format = node.m_sources[3] ? EImageFormat::IF_RGBA_UBYTE : EImageFormat::IF_RGB_UBYTE;
			break;

		case EImageFormat::IF_BC2:
		case EImageFormat::IF_BC3:
		case EImageFormat::IF_BC6:
        case EImageFormat::IF_BC7:
        case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
            compressedFormat = node.m_format;
             op->Format = EImageFormat::IF_RGBA_UBYTE;
			break;

		case EImageFormat::IF_BC4:
			compressedFormat = node.m_format;
            op->Format = EImageFormat::IF_L_UBYTE;
			break;

		case EImageFormat::IF_BC5:
        case EImageFormat::IF_ASTC_4x4_RG_LDR:
            compressedFormat = node.m_format;
			// TODO: Should be RG
            op->Format = EImageFormat::IF_RGB_UBYTE;
			break;

		default:
            op->Format = node.m_format;
			break;

		}

		check(node.m_format != EImageFormat::IF_NONE);

		// Source images and channels
		check(node.m_sources.Num() == node.m_sourceChannels.Num());

		// First source, for reference in the size
        Ptr<ASTOp> first;
		for (int32 t = 0; t<node.m_sources.Num(); ++t)
		{
			if (node.m_sources[t])
			{
				FImageGenerationResult BaseResult;
				GenerateImage(Options, BaseResult, node.m_sources[t]);
                Ptr<ASTOp> source = BaseResult.op;

				source = GenerateImageUncompressed(source);

				if (!source)
				{
					// TODO: Warn?
					source = GenerateMissingImageCode(TEXT("Swizzle channel"), EImageFormat::IF_L_UBYTE, node.m_errorContext, Options);
				}

                Ptr<ASTOp> sizedSource;
				if (first)
				{
                    Ptr<ASTOpFixed> sop = new ASTOpFixed();
                    sop->op.type = OP_TYPE::IM_RESIZELIKE;
                    sop->SetChild( sop->op.args.ImageResizeLike.source, source);
                    sop->SetChild( sop->op.args.ImageResizeLike.sizeSource, first);
                    sizedSource = sop;
				}
				else
				{
					first = source;
					sizedSource = source;
				}

                op->Sources[t] = sizedSource;
                op->SourceChannels[t] = (uint8)node.m_sourceChannels[t];
			}
		}

		// At least one source is required
        if (!op->Sources[0])
		{
            Ptr<ASTOp> source = GenerateMissingImageCode(TEXT("First swizzle image"), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
            op->Sources[0] = source;
		}

        Ptr<ASTOp> resultOp = op;

		if (compressedFormat != EImageFormat::IF_NONE)
		{
            Ptr<ASTOpImagePixelFormat> fop = new ASTOpImagePixelFormat();
            fop->Source = resultOp;
            fop->Format = compressedFormat;
			resultOp = fop;
		}

        Result.op = resultOp;
	}


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Format(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageFormat* InNode)
	{
		const NodeImageFormat::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageFormat);

        check(node.m_format != EImageFormat::IF_NONE);

        Ptr<ASTOpImagePixelFormat> fop = new ASTOpImagePixelFormat();
        fop->Format = node.m_format;
        fop->FormatIfAlpha = node.m_formatIfAlpha;

		// Source is required
		if (!node.m_source)
		{
            fop->Source = GenerateMissingImageCode(TEXT("Source image for format."), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
		}
		else
		{
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_source);
			fop->Source = BaseResult.op;
		}

        Result.op = fop;
	}


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Saturate(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageSaturate* InNode)
	{
		const NodeImageSaturate::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_SATURATE;


        // Source image
        Ptr<ASTOp> base = 0;
        if ( node.m_pSource )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pSource);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Saturate image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }
		
        base = GenerateImageFormat(base, GetRGBOrRGBAFormat(base->GetImageDesc().m_format));
        base = GenerateImageSize( base, Options.RectSize);
        op->SetChild( op->op.args.ImageSaturate.base, base);


        // Factor
        if ( Node* pFactor = node.m_pFactor.get() )
        {
            op->SetChild( op->op.args.ImageSaturate.factor, Generate( pFactor ));
        }
        else
        {
            // This argument is required
            op->SetChild( op->op.args.ImageSaturate.factor, GenerateMissingScalarCode(TEXT("Saturation factor"), 0.5f, node.m_errorContext ) );
        }

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Luminance(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageLuminance* InNode)
	{
		const NodeImageLuminance::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_LUMINANCE;

        // Source image
        Ptr<ASTOp> base;
        if ( node.m_pSource )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pSource);
			base = BaseResult.op;
        }
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image luminance"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }
        base = GenerateImageFormat( base, EImageFormat::IF_RGB_UBYTE );
        op->SetChild( op->op.args.ImageLuminance.base, base);

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_ColourMap(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageColourMap* InNode)
	{
		const NodeImageColourMap::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_COLOURMAP;

        // Base image
        Ptr<ASTOp> base ;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Colourmap base image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }
        base = GenerateImageSize( base, Options.RectSize);
        op->SetChild( op->op.args.ImageColourMap.base, base);

        // Mask of the effect
        Ptr<ASTOp> mask;
        if ( node.m_pMask )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pMask);
			mask = BaseResult.op;
        }
        else
        {
            // Set the argument default value: affect all pixels.
            // TODO: Special operation code without mask
            mask = GeneratePlainImageCode(FVector4f( 1,1,1,1 ), Options);
        }
        mask = GenerateImageFormat( mask, EImageFormat::IF_L_UBYTE );
        mask = GenerateImageSize( mask, Options.RectSize);
        op->SetChild( op->op.args.ImageColourMap.mask, mask);

        // Map image
        Ptr<ASTOp> MapImageOp;
        if ( node.m_pMap )
        {
			FImageGenerationOptions MapOptions = Options;
			MapOptions.ImageLayoutStrategy = CompilerOptions::TextureLayoutStrategy::None;
			MapOptions.LayoutToApply = nullptr;
			MapOptions.LayoutBlockId = -1;
			MapOptions.RectSize = {};

			FImageGenerationResult BaseResult;
			GenerateImage(MapOptions, BaseResult, node.m_pMap);
			MapImageOp = BaseResult.op;
			MapImageOp = GenerateImageFormat(MapImageOp, EImageFormat::IF_RGB_UBYTE);
		}
        else
        {
            // This argument is required
			MapImageOp = GenerateMissingImageCode(TEXT("Map image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }
        op->SetChild( op->op.args.ImageColourMap.map, MapImageOp);

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Gradient(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageGradient* InNode)
	{
		const NodeImageGradient::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_GRADIENT;

        // First colour
        Ptr<ASTOp> colour0 = 0;
        if ( Node* pColour0 = node.m_pColour0.get() )
        {
            colour0 = Generate( pColour0 );
        }
        else
        {
            // This argument is required
            colour0 = GenerateMissingColourCode(TEXT("Gradient colour 0"), node.m_errorContext );
        }
        op->SetChild( op->op.args.ImageGradient.colour0, colour0);

        // Second colour
        Ptr<ASTOp> colour1 = 0;
        if ( Node* pColour1 = node.m_pColour1.get() )
        {
            colour1 = Generate( pColour1 );
        }
        else
        {
            // This argument is required
            colour1 = GenerateMissingColourCode(TEXT("Gradient colour 1"), node.m_errorContext );
        }
        op->SetChild( op->op.args.ImageGradient.colour1, colour1);

        op->op.args.ImageGradient.size[0] = (uint16)FMath::Max( 2, FMath::Min( 1024, node.m_size[0] ) );
        op->op.args.ImageGradient.size[1] = (uint16)FMath::Max( 1, FMath::Min( 1024, node.m_size[1] ) );

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Binarise(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageBinarise* InNode)
	{
		const NodeImageBinarise::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_BINARISE;

        // A image
        Ptr<ASTOp> a;
        if ( node.m_pBase )
        {
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			a = BaseResult.op;
		}
        else
        {
            // This argument is required
            a = GenerateMissingImageCode(TEXT("Image Binarise Base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }
        a = GenerateImageFormat( a, EImageFormat::IF_RGB_UBYTE );
        a = GenerateImageSize( a, Options.RectSize);
        op->SetChild( op->op.args.ImageBinarise.base, a );

        // Threshold
        Ptr<ASTOp> b = 0;
        if ( Node* pScalar = node.m_pThreshold.get() )
        {
            b = Generate( pScalar );
        }
        else
        {
            // This argument is required
            b = GenerateMissingScalarCode(TEXT("Image Binarise Threshold"), 0.5f, node.m_errorContext );
        }
        op->SetChild( op->op.args.ImageBinarise.threshold, b );

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Resize(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageResize* InNode)
	{
		const NodeImageResize::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageResize);

        Ptr<ASTOp> at = 0;

        // Source image
        Ptr<ASTOp> base;
        if ( node.m_pBase )
        {
			FImageGenerationOptions NewOptions = Options;
			
			if (node.m_relative)
			{
				NewOptions.RectSize[0] = FMath::RoundToInt32(NewOptions.RectSize[0] / node.m_sizeX);
				NewOptions.RectSize[1] = FMath::RoundToInt32(NewOptions.RectSize[1] / node.m_sizeY);
			}

			FImageGenerationResult BaseResult;
			GenerateImage(NewOptions, BaseResult, node.m_pBase);
			base = BaseResult.op;
        }
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Image resize base"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

        // Size
        if ( node.m_relative )
        {
            Ptr<ASTOpFixed> op = new ASTOpFixed();
            op->op.type = OP_TYPE::IM_RESIZEREL;
            op->op.args.ImageResizeRel.factor[0] = node.m_sizeX;
            op->op.args.ImageResizeRel.factor[1] = node.m_sizeY;
            op->SetChild( op->op.args.ImageResizeRel.source, base);
            at = op;
        }
        else
        {
            Ptr<ASTOpFixed> op = new ASTOpFixed();
            op->op.type = OP_TYPE::IM_RESIZE;
            op->op.args.ImageResize.size[0] = (uint16)node.m_sizeX;
            op->op.args.ImageResize.size[1] = (uint16)node.m_sizeY;
            op->SetChild( op->op.args.ImageResize.source, base);
            at = op;
        }

        Result.op = at;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_PlainColour(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImagePlainColour* InNode)
	{
		const NodeImagePlainColour::Private& node = *InNode->GetPrivate();

		// Source colour
        Ptr<ASTOp> base = 0;
        if ( node.m_pColour )
        {
            base = Generate( node.m_pColour.get() );
        }
        else
        {
            // This argument is required
            base = GenerateMissingColourCode(TEXT("Image plain colour base"), node.m_errorContext );
        }

        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::IM_PLAINCOLOUR;
        op->SetChild( op->op.args.ImagePlainColour.colour, base);
		op->op.args.ImagePlainColour.format = node.Format;
        op->op.args.ImagePlainColour.size[0] = (Options.RectSize[0] > 0) ? Options.RectSize[0] : node.m_sizeX;
		op->op.args.ImagePlainColour.size[1] = (Options.RectSize[1] > 0) ? Options.RectSize[1] : node.m_sizeY;
		op->op.args.ImagePlainColour.LODs = 1;

        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Switch(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageSwitch* InNode)
	{
		const NodeImageSwitch::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageSwitch);

        if (node.m_options.Num() == 0)
		{
			// No options in the switch!
            Ptr<ASTOp> missingOp = GenerateMissingImageCode(TEXT("Switch option"), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
			Result.op = missingOp;
			return;
		}

        Ptr<ASTOpSwitch> op = new ASTOpSwitch();
        op->type = OP_TYPE::IM_SWITCH;

		// Variable value
		if ( node.m_pParameter )
		{
            op->variable = Generate( node.m_pParameter.get() );
		}
		else
		{
			// This argument is required
            op->variable = GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, node.m_errorContext );
		}

		// Options
        for ( std::size_t t=0; t< node.m_options.Num(); ++t )
        {
            Ptr<ASTOp> branch;

            if (node.m_options[t])
            {
				FImageGenerationResult BaseResult;
				GenerateImage(Options, BaseResult, node.m_options[t]);
				branch = BaseResult.op;
			}
            else
            {
                // This argument is required
                branch = GenerateMissingImageCode(TEXT("Switch option"), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
            }

            op->cases.Emplace((int16_t)t,op,branch);
        }

        Ptr<ASTOp> switchAt = op;

        // Make sure all options are the same format and size
		// Disabled: This is not always desirable. For example if the image is going to be used in a 
		// projector, the size doesn't need to be constrained.
        //auto desc = switchAt->GetImageDesc( true );
        //if ( desc.m_format == EImageFormat::IF_NONE )
        //{
        //    // TODO: Look for the most generic of the options?
        //    // For now force a decently generic one
        //    desc.m_format = EImageFormat::IF_RGBA_UBYTE;
        //}

        //if (desc.m_size[0]!=0 && desc.m_size[1]!=0)
        //{
        //    Ptr<ASTOpFixed> sop = new ASTOpFixed();
        //    sop->op.type = OP_TYPE::IM_RESIZE;
        //    sop->op.args.ImageResize.size[0] = desc.m_size[0];
        //    sop->op.args.ImageResize.size[1] = desc.m_size[1];
        //    sop->SetChild( sop->op.args.ImageResize.source, switchAt );
        //    switchAt = sop;
        //}

        //{
        //    Ptr<ASTOpFixed> fop = new ASTOpFixed();
        //    fop->op.type = OP_TYPE::IM_PIXELFORMAT;
        //    fop->op.args.ImagePixelFormat.format = desc.m_format;
        //    fop->SetChild( fop->op.args.ImagePixelFormat.source, switchAt );
        //    switchAt = fop;
        //}

        Result.op = switchAt;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Conditional(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageConditional* InNode)
	{
		const NodeImageConditional::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpConditional> op = new ASTOpConditional();
        op->type = OP_TYPE::IM_CONDITIONAL;

        // Condition
        if ( node.m_parameter )
        {
            op->condition = Generate( node.m_parameter.get() );
        }
        else
        {
            // This argument is required
            op->condition = GenerateMissingBoolCode(TEXT("Conditional condition"), true, node.m_errorContext );
        }

        // Options
		FImageGenerationResult YesResult;
		GenerateImage(Options, YesResult, node.m_true);
		op->yes = YesResult.op;

		FImageGenerationResult NoResult;
		GenerateImage(Options, NoResult, node.m_false);
		op->no = NoResult.op;
		
        Result.op = op;
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Project(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageProject* InNode)
	{
		const NodeImageProject::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageProject);

        // Mesh project operation
        //------------------------------
        Ptr<ASTOpFixed> ProjectOp = new ASTOpFixed();
		ProjectOp->op.type = OP_TYPE::ME_PROJECT;

        Ptr<ASTOp> LastMeshOp = ProjectOp;

        // Projector
        FProjectorGenerationResult projectorResult;
        if ( node.m_pProjector )
        {
            GenerateProjector( projectorResult, node.m_pProjector );
            //projectorAt = Generate( node.m_pProjector.get() );
        }
        else
        {
            // This argument is required
            GenerateMissingProjectorCode( projectorResult, node.m_errorContext );
        }

		ProjectOp->SetChild(ProjectOp->op.args.MeshProject.projector, projectorResult.op );

		int32 LayoutBlockIndex = -1;
		if (Options.LayoutToApply)
		{
			LayoutBlockIndex = Options.LayoutToApply->m_blocks.IndexOfByPredicate([&](const Layout::FBlock& Block) { return Block.m_id == Options.LayoutBlockId; });
		}
		int32 GeneratedLayoutBlockId = -1;

        // Mesh
        if ( node.m_pMesh )
        {
			// TODO: This will probably Result in a duplicated mesh subgraph, with the original mesh but new layout block ids.
			// See if it can be optimized and try to reuse the existing layout block ids instead of generating new ones.
			FMeshGenerationOptions MeshOptions;
			MeshOptions.State = Options.CurrentStateIndex;
			MeshOptions.ActiveTags = Options.ActiveTags;
			MeshOptions.bLayouts = true;			// We need the layout that we will use to render
			MeshOptions.bNormalizeUVs = true;		// We need normalized UVs for the projection

			// We may need IDs at this point if there are modifiers for the mesh.
			// \TODO: Detect this at mesh constant generation and enable there instead of here?
			MeshOptions.bUniqueVertexIDs = true;
			
			FMeshGenerationResult MeshResult;
			GenerateMesh( MeshOptions, MeshResult, node.m_pMesh );

			// Match the block id of the block we are generating with the id that resulted in the generated mesh
			GeneratedLayoutBlockId = -1;
			
			Ptr<const Layout> Layout = MeshResult.GeneratedLayouts.IsValidIndex(node.m_layout) ? MeshResult.GeneratedLayouts[node.m_layout] : nullptr;
			if (Layout && Layout->m_blocks.IsValidIndex(LayoutBlockIndex))
			{
				GeneratedLayoutBlockId = Layout->m_blocks[LayoutBlockIndex].m_id;
			}
			else if (Layout && Layout->m_blocks.Num() == 1)
			{
				// Layout management disabled, use the only block available
				GeneratedLayoutBlockId = Layout->m_blocks[0].m_id;
			}
			else
			{
				m_pErrorLog->GetPrivate()->Add("Layout or block index error.", ELMT_ERROR, node.m_errorContext);
			}


			Ptr<ASTOp> CurrentMeshToProjectOp = MeshResult.meshOp;

            if (projectorResult.type == PROJECTOR_TYPE::WRAPPING)
            {
                // For wrapping projector we need the entire mesh. The actual project operation
                // will remove the faces that are not in the layout block we are generating.
                Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
                cop->type = OP_TYPE::ME_CONSTANT;
				Ptr<Mesh> FormatMeshResult = new Mesh();
				CreateMeshOptimisedForWrappingProjection(FormatMeshResult.get(), node.m_layout);

                cop->SetValue(FormatMeshResult, m_compilerOptions->OptimisationOptions.bUseDiskCache);

                Ptr<ASTOpMeshFormat> FormatOp = new ASTOpMeshFormat();
				FormatOp->Buffers = OP::MeshFormatArgs::BT_VERTEX
                        | OP::MeshFormatArgs::BT_INDEX
                        | OP::MeshFormatArgs::BT_FACE
                        | OP::MeshFormatArgs::BT_RESETBUFFERINDICES;
				FormatOp->Format = cop;
				FormatOp->Source = CurrentMeshToProjectOp;
				CurrentMeshToProjectOp = FormatOp;
            }
            else
            {
                // Extract the mesh layout block
                if ( GeneratedLayoutBlockId>=0 )
                {
                    Ptr<ASTOpMeshExtractLayoutBlocks> eop = new ASTOpMeshExtractLayoutBlocks();
                    eop->source = CurrentMeshToProjectOp;
                    eop->layout = node.m_layout;

                    eop->blocks.Add(GeneratedLayoutBlockId);

					CurrentMeshToProjectOp = eop;
                }

                // Reformat the mesh to a more efficient format for this operation
                Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
                cop->type = OP_TYPE::ME_CONSTANT;

				Ptr<Mesh> FormatMeshResult = new Mesh();
                CreateMeshOptimisedForProjection(FormatMeshResult.get(), node.m_layout);

                cop->SetValue(FormatMeshResult, m_compilerOptions->OptimisationOptions.bUseDiskCache);

                Ptr<ASTOpMeshFormat> FormatOp = new ASTOpMeshFormat();
				FormatOp->Buffers = OP::MeshFormatArgs::BT_VERTEX
					| OP::MeshFormatArgs::BT_INDEX
					| OP::MeshFormatArgs::BT_FACE
					| OP::MeshFormatArgs::BT_RESETBUFFERINDICES;
				FormatOp->Format = cop;
				FormatOp->Source = CurrentMeshToProjectOp;
				CurrentMeshToProjectOp = FormatOp;
            }

			ProjectOp->SetChild(ProjectOp->op.args.MeshProject.mesh, CurrentMeshToProjectOp);

        }
        else
        {
            // This argument is required
            Ptr<const Mesh> TempMesh = new Mesh();
            Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
            cop->type = OP_TYPE::ME_CONSTANT;
            cop->SetValue(TempMesh, m_compilerOptions->OptimisationOptions.bUseDiskCache );
			ProjectOp->SetChild(ProjectOp->op.args.MeshProject.mesh, cop );
            m_pErrorLog->GetPrivate()->Add( "Projector mesh not set.", ELMT_ERROR, node.m_errorContext );
        }


        // Image raster operation
        //------------------------------
        Ptr<ASTOpImageRasterMesh> ImageRasterOp = new ASTOpImageRasterMesh();
		ImageRasterOp->mesh = LastMeshOp;
		ImageRasterOp->projector = projectorResult.op;

        // Image
        if ( node.m_pImage )
        {
            // Generate
			FImageGenerationOptions NewOptions = Options;
			NewOptions.ImageLayoutStrategy = CompilerOptions::TextureLayoutStrategy::None;
			NewOptions.LayoutToApply = nullptr;
			NewOptions.LayoutBlockId = -1;
			NewOptions.RectSize = {};

			FImageGenerationResult ImageResult;
			GenerateImage(NewOptions, ImageResult, node.m_pImage);
			ImageRasterOp->image = ImageResult.op;

			FImageDesc desc = CalculateImageDesc(*node.m_pImage->GetBasePrivate());
			ImageRasterOp->SourceSizeX = desc.m_size[0];
			ImageRasterOp->SourceSizeY = desc.m_size[1];
        }
        else
        {
            // This argument is required
			ImageRasterOp->image = GenerateMissingImageCode(TEXT("Projector image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

        // Image size, from the current block being generated
		ImageRasterOp->SizeX = Options.RectSize[0];
		ImageRasterOp->SizeY = Options.RectSize[1];
		ImageRasterOp->BlockId = GeneratedLayoutBlockId;
		ImageRasterOp->LayoutIndex = node.m_layout;

		ImageRasterOp->bIsRGBFadingEnabled = node.bIsRGBFadingEnabled;
		ImageRasterOp->bIsAlphaFadingEnabled = node.bIsAlphaFadingEnabled;
		ImageRasterOp->SamplingMethod = node.SamplingMethod;
		ImageRasterOp->MinFilterMethod = node.MinFilterMethod;

		// Fading angles are optional, and stored in a colour. If one exists, we generate both.
		if (node.m_pAngleFadeStart || node.m_pAngleFadeEnd)
		{
			Ptr<NodeScalarConstant> pDefaultFade = new NodeScalarConstant();
			pDefaultFade->SetValue(180.0f);

			Ptr<NodeColourFromScalars> pPropsNode = new NodeColourFromScalars();

			if (node.m_pAngleFadeStart) pPropsNode->SetX(node.m_pAngleFadeStart);
			else pPropsNode->SetX(pDefaultFade);

			if (node.m_pAngleFadeEnd) pPropsNode->SetY(node.m_pAngleFadeEnd);
			else pPropsNode->SetY(pDefaultFade);

			ImageRasterOp->angleFadeProperties = Generate(pPropsNode);
		}

        // Target mask
        if ( node.m_pMask )
        {
			FImageGenerationResult MaskResult;
			GenerateImage(Options, MaskResult, node.m_pMask);
			Ptr<ASTOp> mask = MaskResult.op;

            mask = GenerateImageFormat( mask, EImageFormat::IF_L_UBYTE );
			ImageRasterOp->mask = GenerateImageSize( mask, Options.RectSize);
        }

        // Seam correction operations
        //------------------------------
		if (node.bEnableTextureSeamCorrection)
		{
			Ptr<ASTOpImageRasterMesh> MaskRasterOp = new ASTOpImageRasterMesh();
			MaskRasterOp->mesh = ImageRasterOp->mesh.child();
			MaskRasterOp->image = 0;
			MaskRasterOp->mask = 0;
			MaskRasterOp->BlockId = ImageRasterOp->BlockId;
			MaskRasterOp->LayoutIndex = ImageRasterOp->LayoutIndex;
			MaskRasterOp->SizeX = ImageRasterOp->SizeX;
			MaskRasterOp->SizeY = ImageRasterOp->SizeY;
			MaskRasterOp->UncroppedSizeX = ImageRasterOp->UncroppedSizeX;
			MaskRasterOp->UncroppedSizeY = ImageRasterOp->UncroppedSizeY;
			MaskRasterOp->CropMinX = ImageRasterOp->CropMinX;
			MaskRasterOp->CropMinY = ImageRasterOp->CropMinY;
			MaskRasterOp->SamplingMethod = ESamplingMethod::Point;
			MaskRasterOp->MinFilterMethod = EMinFilterMethod::None;

			Ptr<ASTOpImageMakeGrowMap> MakeGrowMapOp = new ASTOpImageMakeGrowMap();
			MakeGrowMapOp->Mask = MaskRasterOp;
			MakeGrowMapOp->Border = MUTABLE_GROW_BORDER_VALUE;

			// If we want to be able to generate progressive mips efficiently, we need mipmaps for the "displacement map".
			if (m_compilerOptions->OptimisationOptions.bEnableProgressiveImages)
			{
				Ptr<ASTOpImageMipmap> MipMask = new ASTOpImageMipmap;
				MipMask->Source = MakeGrowMapOp->Mask.child();
				MipMask->bPreventSplitTail = true;
				MakeGrowMapOp->Mask = MipMask;
			}

			Ptr<ASTOpFixed> DisplaceOp = new ASTOpFixed();
			DisplaceOp->op.type = OP_TYPE::IM_DISPLACE;
			DisplaceOp->SetChild(DisplaceOp->op.args.ImageDisplace.displacementMap, MakeGrowMapOp);
			DisplaceOp->SetChild(DisplaceOp->op.args.ImageDisplace.source, ImageRasterOp);

			Result.op = DisplaceOp;
		}
		else
		{
			Result.op = ImageRasterOp;
		}		
    }


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Mipmap(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageMipmap* InNode)
	{
		const NodeImageMipmap::Private& node = *InNode->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeImageMipmap);

        Ptr<ASTOp> res;

        Ptr<ASTOpImageMipmap> op = new ASTOpImageMipmap();

        // At the end of the day, we want all the mipmaps. Maybe the code optimiser will split the process later.
        op->Levels = 0;

        // Source image
        Ptr<ASTOp> base;
        if ( node.m_pSource )
        {
            MUTABLE_CPUPROFILER_SCOPE(Base);
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pSource);
			base = BaseResult.op;
		}
        else
        {
            // This argument is required
            base = GenerateMissingImageCode(TEXT("Mipmap image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
        }

        op->Source = base;

        // The number of tail mipmaps depends on the cell size. We need to know it for some code
        // optimisation operations. Scan the source image code looking for this info
        int32 blockX = 0;
        int32 blockY = 0;
        if ( Options.ImageLayoutStrategy
             !=
             CompilerOptions::TextureLayoutStrategy::None )
        {
            MUTABLE_CPUPROFILER_SCOPE(GetLayoutBlockSize);
            op->Source->GetLayoutBlockSize( &blockX, &blockY );
        }

        if ( blockX && blockY )
        {
            int32 mipsX = (int)ceilf( logf( (float)blockX )/logf(2.0f) );
            int32 mipsY = (int)ceilf( logf( (float)blockY )/logf(2.0f) );
            op->BlockLevels = (uint8)FMath::Max( mipsX, mipsY );
        }
        else
        {
            // No layout. Mipmap all the way down.
            op->BlockLevels = 0;
        }

		op->AddressMode = node.m_settings.m_addressMode;
		op->FilterType = node.m_settings.m_filterType;
		op->SharpenFactor = node.m_settings.m_sharpenFactor;
		op->DitherMipmapAlpha = node.m_settings.m_ditherMipmapAlpha;

        res = op;

        Result.op = res;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Invert(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageInvert* InNode)
	{
		const NodeImageInvert::Private& node = *InNode->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::IM_INVERT;

		// A image
		Ptr<ASTOp> a;
		if (node.m_pBase)
		{
			FImageGenerationResult BaseResult;
			GenerateImage(Options, BaseResult, node.m_pBase);
			a = BaseResult.op;
		}
		else
		{
			// This argument is required
			a = GenerateMissingImageCode(TEXT("Image Invert Color"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, Options);
		}
		a = GenerateImageFormat(a, EImageFormat::IF_RGB_UBYTE);
		a = GenerateImageSize(a, Options.RectSize);
		op->SetChild(op->op.args.ImageInvert.base, a);

		Result.op = op;
	}


    //---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Variation(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageVariation* InNode)
	{
		const NodeImageVariation::Private& node = *InNode->GetPrivate();

		Ptr<ASTOp> currentOp;

        // Default case
        if ( node.m_defaultImage )
        {
            FImageGenerationResult BranchResults;
			GenerateImage(Options, BranchResults, node.m_defaultImage);
			currentOp = BranchResults.op;
        }
        else
        {
            // This argument is required
            currentOp = GenerateMissingImageCode(TEXT("Variation default"), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
        }

        // Process variations in reverse order, since conditionals are built bottom-up.
        for ( int t = int( node.m_variations.Num() ) - 1; t >= 0; --t )
        {
            int tagIndex = -1;
            const string& tag = node.m_variations[t].m_tag;
            for ( int i = 0; i < int( m_firstPass.m_tags.Num() ); ++i )
            {
                if ( m_firstPass.m_tags[i].tag == tag )
                {
                    tagIndex = i;
                }
            }

            if ( tagIndex < 0 )
            {
				FString Msg = FString::Printf(TEXT("Unknown tag found in image variation [%s]."), tag.c_str() );

                m_pErrorLog->GetPrivate()->Add( Msg, ELMT_WARNING, node.m_errorContext );
                continue;
            }

            Ptr<ASTOp> variationOp;
            if ( node.m_variations[t].m_image )
            {
				FImageGenerationResult VariationResult;
				GenerateImage(Options, VariationResult, node.m_variations[t].m_image);
				variationOp = VariationResult.op;
            }
            else
            {
                // This argument is required
                variationOp = GenerateMissingImageCode(TEXT("Variation option"), EImageFormat::IF_RGBA_UBYTE, node.m_errorContext, Options);
            }


            Ptr<ASTOpConditional> conditional = new ASTOpConditional;
            conditional->type = OP_TYPE::IM_CONDITIONAL;
            conditional->no = currentOp;
            conditional->yes = variationOp;
            conditional->condition = m_firstPass.m_tags[tagIndex].genericCondition;

            currentOp = conditional;
        }

        // Make sure all options are the same format and size
        auto desc = currentOp->GetImageDesc( true );
        if ( desc.m_format == EImageFormat::IF_NONE )
        {
            // TODO: Look for the most generic of the options?
            // For now force a decently generic one
            desc.m_format = EImageFormat::IF_RGBA_UBYTE;
        }

        if ( desc.m_size[0] != 0 && desc.m_size[1] != 0 )
        {
            Ptr<ASTOpFixed> sop = new ASTOpFixed();
            sop->op.type = OP_TYPE::IM_RESIZE;
            sop->op.args.ImageResize.size[0] = desc.m_size[0];
            sop->op.args.ImageResize.size[1] = desc.m_size[1];
            sop->SetChild( sop->op.args.ImageResize.source, currentOp );
            currentOp = sop;
        }

        {
            Ptr<ASTOpImagePixelFormat> fop = new ASTOpImagePixelFormat();
            fop->Format = desc.m_format;
            fop->Source = currentOp;
            currentOp = fop;
        }

        Result.op = currentOp;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateImage_Table(const FImageGenerationOptions& Options, FImageGenerationResult& Result, const NodeImageTable* InNode)
	{
		const NodeImageTable::Private& node = *InNode->GetPrivate();

		Result.op = GenerateTableSwitch<NodeImageTable::Private, TCT_IMAGE, OP_TYPE::IM_SWITCH>(node,
			[this,Options](const NodeImageTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				TABLE_VALUE CellData = node.m_pTable->GetPrivate()->m_rows[row].m_values[colIndex];
				ImagePtrConst pImage = nullptr;
				NodeImagePtr CellImage = nullptr;

				if (Ptr<ResourceProxy<Image>> pProxyImage = CellData.m_pProxyImage)
				{
					pImage = pProxyImage->Get();
				}

				if (!pImage)
				{
					FString Msg = FString::Printf(TEXT("Table has a missing image in column %d, row %d."), colIndex, row);
					pErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, node.m_errorContext);
				}
				else
				{
					if (pImage->IsReference())
					{
						Ptr<NodeImageReference> ImageRef = new NodeImageReference();
						ImageRef->SetImageReference(pImage->GetReferencedTexture());

						CellImage = ImageRef;
					}
					else
					{
						NodeImageConstantPtr ImageConst = new NodeImageConstant();
						ImageConst->SetValue(pImage);

						CellImage = ImageConst;
					}
				}

				FImageGenerationResult Result;
				GenerateImage(Options, Result, CellImage);
				return Result.op;
			});
	}


    //---------------------------------------------------------------------------------------------
    Ptr<Image> CodeGenerator::GenerateMissingImage( EImageFormat format )
    {
        // Create the image node if it hasn't been created yet.
        if (!m_missingImage[size_t(format)])
        {
            // Make a checkered debug image
            const FImageSize size = MUTABLE_MISSING_IMAGE_DESC.m_size;

            ImagePtr pImage = new Image( size[0], size[1], 1, format, EInitializationType::NotInitialized);

            switch (format)
            {
            case EImageFormat::IF_L_UBYTE:
                {
                    uint8_t* pData = pImage->GetData();
                    for ( int p=0; p<size[0]*size[1]; ++p )
                    {
                        if ((p+p/size[0])%2)
                        {
                            pData[0] = 255;
                        }
                        else
                        {
                            pData[0] = 64;
                        }

                        pData++;
                    }
                    break;
                }

            case EImageFormat::IF_RGB_UBYTE:
                {
                    uint8_t* pData = pImage->GetData();
                    for ( int p=0; p<size[0]*size[1]; ++p )
                    {
                        if ((p+p/size[0])%2)
                        {
                            pData[0] = 255;
                            pData[1] = 255;
                            pData[2] = 64;
                        }
                        else
                        {
                            pData[0] = 64;
                            pData[1] = 64;
                            pData[2] = 255;
                        }

                        pData += 3;
                    }
                    break;
                }

            case EImageFormat::IF_BGRA_UBYTE:
            case EImageFormat::IF_RGBA_UBYTE:
                {
                    uint8_t* pData = pImage->GetData();
                    for ( int p=0; p<size[0]*size[1]; ++p )
                    {
                        if ((p+p/size[0])%2)
                        {
                            pData[0] = 255;
                            pData[1] = 255;
                            pData[2] = 64;
                            pData[3] = 255;
                        }
                        else
                        {
                            pData[0] = 64;
                            pData[1] = 64;
                            pData[2] = 255;
                            pData[3] = 128;
                        }

                        pData += 4;
                    }
                    break;
                }

            default:
                check( false );
                break;

            }

            m_missingImage[(size_t)format] = pImage;
        }

        return m_missingImage[(size_t)format].get();
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GenerateMissingImageCode(const TCHAR* strWhere, EImageFormat format, const void* errorContext, const FImageGenerationOptions& Options )
    {
        // Log an error message
		FString Msg = FString::Printf(TEXT("Required connection not found: %s"), strWhere );
        m_pErrorLog->GetPrivate()->Add( Msg, ELMT_ERROR, errorContext );

        // Make a checkered debug image
        ImagePtr pImage = GenerateMissingImage( format );

        NodeImageConstantPtr pNode = new NodeImageConstant();
        pNode->SetValue( pImage.get() );

		FImageGenerationResult Result;
		GenerateImage(Options, Result, pNode);

        return Result.op;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GeneratePlainImageCode( const FVector4f& InColor, const FImageGenerationOptions& Options )
    {
		Ptr<NodeColourConstant> ConstantColor = new NodeColourConstant();
		ConstantColor->SetValue(InColor);

        Ptr<NodeImagePlainColour> PlainNode = new NodeImagePlainColour();
		PlainNode->SetColour(ConstantColor);

		FImageGenerationResult TempResult;
		GenerateImage(Options, TempResult, PlainNode);
        Ptr<ASTOp> Result = TempResult.op;

        return Result;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GenerateImageFormat( Ptr<ASTOp> Op, EImageFormat InFormat)
    {
        Ptr<ASTOp> Result = Op;

        if (InFormat!=EImageFormat::IF_NONE && Op && Op->GetImageDesc().m_format!=InFormat)
        {
            // Generate the format change code
            Ptr<ASTOpImagePixelFormat> op = new ASTOpImagePixelFormat();
            op->Source = Op;
            op->Format = InFormat;
			Result = op;
        }

        return Result;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GenerateImageUncompressed( Ptr<ASTOp> at )
    {
        Ptr<ASTOp> Result = at;

        if (at)
        {
            EImageFormat sourceFormat = at->GetImageDesc().m_format;
            EImageFormat targetFormat = GetUncompressedFormat( sourceFormat );

            if ( targetFormat != sourceFormat )
            {
                // Generate the format change code
                Ptr<ASTOpImagePixelFormat> op = new ASTOpImagePixelFormat();
                op->Source = at;
                op->Format = targetFormat;
                Result = op;
            }
        }

        return Result;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ASTOp> CodeGenerator::GenerateImageSize( Ptr<ASTOp> at, UE::Math::TIntVector2<int32> size )
    {
        Ptr<ASTOp> Result = at;

		if (size[0] > 0 && size[1] > 0)
		{
			if (UE::Math::TIntVector2<int32>(at->GetImageDesc().m_size) != size)
			{
				Ptr<ASTOpFixed> op = new ASTOpFixed();
				op->op.type = OP_TYPE::IM_RESIZE;
				op->SetChild(op->op.args.ImageResize.source, at);
				op->op.args.ImageResize.size[0] = size[0];
				op->op.args.ImageResize.size[1] = size[1];
				Result = op;
			}
		}

        return Result;
    }


    //---------------------------------------------------------------------------------------------
    FImageDesc CodeGenerator::CalculateImageDesc( const Node::Private& node )
    {
        ImageDescGenerator imageDescGenerator;
        imageDescGenerator.Generate( node );
        return imageDescGenerator.m_desc;
    }



    //---------------------------------------------------------------------------------------------
    //! This class contains the support data to accelerate the GetImageDesc recursive function.
    //! _If none is provided in the call, one will be created at that level and used from there on.
    class FGetImageDescContext
    {
    public:
        vector<bool> m_visited;
        vector<FImageDesc> m_results;
    };

 
}
