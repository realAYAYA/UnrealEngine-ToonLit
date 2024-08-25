// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessVisualizeNanite.h"
#include "NaniteVisualizationData.h"
#include "UnrealEngine.h"
#include "RHIStaticStates.h"
#include "SceneRendering.h"

class FVisualizeNanitePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisualizeNanitePS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeNanitePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FLinearColor, SelectionColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeNanitePS, "/Engine/Private/PostProcessVisualizeBuffer.usf", "MainPS", SF_Pixel);

void AddVisualizeNanitePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture Output, const Nanite::FRasterResults& RasterResults)
{
	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	if (VisualizationData.IsActive())
	{
		const bool bSingleVisualization = VisualizationData.GetActiveModeID() > 0;
		const bool bOverviewVisualization = VisualizationData.GetActiveModeID() == 0;

		// Any individual mode
		if (bSingleVisualization)
		{
			if (ensure(RasterResults.Visualizations.Num() == 1))
			{
				const Nanite::FVisualizeResult& Visualization = RasterResults.Visualizations[0];

				FRHIBlendState* BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_Zero>::GetRHI();

				FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
				Parameters->InputTexture = Visualization.ModeOutput;
				Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Parameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ELoad);

				const FScreenPassTextureViewport InputViewport(Visualization.ModeOutput->Desc.Extent, View.ViewRect);
				const FScreenPassTextureViewport OutputViewport(Output);

				TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);
				TShaderMapRef<FCopyRectPS> PixelShader(View.ShaderMap);

				AddDrawScreenPass(
					GraphBuilder,
					RDG_EVENT_NAME("DrawTexture"),
					View,
					OutputViewport,
					InputViewport,
					VertexShader,
					PixelShader,
					BlendState,
					Parameters,
					EScreenPassDrawFlags::None);
			}
		}
		// Overview mode
		else if (bOverviewVisualization)
		{
			struct FTileLabel
			{
				FString Label;
				FIntPoint Location;
			};

			TArray<FTileLabel> TileLabels;
			TileLabels.Reserve(RasterResults.Visualizations.Num());

			// Use the unscaled view so that dynamic resolution scaling doesn't scale the Nanite visualization tile(s).
			const FIntRect& UnscaledViewRect = View.UnscaledViewRect;

			const int32 MaxTilesX  = 4;
			const int32 MaxTilesY  = 4;
			const int32 TileWidth  = UnscaledViewRect.Width() / MaxTilesX;
			const int32 TileHeight = UnscaledViewRect.Height() / MaxTilesY;

			FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			FScreenPassRenderTarget OutputTarget(Output.Texture, UnscaledViewRect, ERenderTargetLoadAction::ELoad);

			for (int32 TileIndex = 0; TileIndex < RasterResults.Visualizations.Num(); ++TileIndex)
			{
				const Nanite::FVisualizeResult& Visualization = RasterResults.Visualizations[TileIndex];

				// The list can contain invalid entries to keep the indices static.
				if (Visualization.bSkippedTile)
				{
					continue;
				}

				const int32 TileX = TileIndex % MaxTilesX;
				const int32 TileY = TileIndex / MaxTilesX;

				FScreenPassTextureViewport OutputViewport(OutputTarget);
				OutputViewport.Rect.Min = UnscaledViewRect.Min + FIntPoint(TileX * TileWidth, TileY * TileHeight);
				OutputViewport.Rect.Max = OutputViewport.Rect.Min + FIntPoint(TileWidth, TileHeight);

				const FLinearColor SelectionColor = FLinearColor::Transparent;

				FVisualizeNanitePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeNanitePS::FParameters>();
				PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
				PassParameters->RenderTargets[0] = OutputTarget.GetRenderTargetBinding();
				PassParameters->InputTexture = Visualization.ModeOutput;
				PassParameters->InputSampler = BilinearClampSampler;
				PassParameters->SelectionColor = SelectionColor;

				FScreenPassTexture InputTexture(Visualization.ModeOutput);
				InputTexture.ViewRect = View.ViewRect;
				const FScreenPassTextureViewport InputViewport(InputTexture);

				TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);
				TShaderMapRef<FVisualizeNanitePS> PixelShader(View.ShaderMap);
				FRHIBlendState* BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();

				const FText DisplayText = VisualizationData.GetModeDisplayName(Visualization.ModeName);
				const FString& DisplayName = DisplayText.ToString();

				AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Tile: %s", *DisplayName), View, OutputViewport, InputViewport, VertexShader, PixelShader, BlendState, PassParameters);

				FTileLabel TileLabel;
				TileLabel.Label = DisplayName;
				TileLabel.Location.X = 8 + TileX * TileWidth;
				TileLabel.Location.Y = (TileY + 1) * TileHeight - 19;
				TileLabels.Add(TileLabel);
			}

			AddDrawCanvasPass(GraphBuilder, RDG_EVENT_NAME("Labels"), View, OutputTarget, [LocalTileLabels = MoveTemp(TileLabels)](FCanvas& Canvas)
			{
				const float DPIScale = Canvas.GetDPIScale();
				Canvas.SetBaseTransform(FMatrix(FScaleMatrix(DPIScale)* Canvas.CalcBaseTransform2D(Canvas.GetViewRect().Width(), Canvas.GetViewRect().Height())));

				const FLinearColor LabelColor(1, 1, 0);
				for (const FTileLabel& TileLabel : LocalTileLabels)
				{
					FIntPoint ScreenPos(TileLabel.Location.X, TileLabel.Location.Y);
					Canvas.DrawShadowedString(ScreenPos.X / DPIScale, ScreenPos.Y / DPIScale, *TileLabel.Label, GetStatsFont(), LabelColor);
				}
			});
		}
	}
}