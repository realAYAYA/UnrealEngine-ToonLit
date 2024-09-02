// --------------------------------------------------------------------------------------
float3 GetSceneColor(float2 Uv);
float GetSceneTextureSize();
float2 GetCurrentUV();
float GetDepth(float2 Uv);
float3 GetNormal(float2 Uv);
float Fresnel(float ExponentIn, float BaseReflectFractionIn, float3 Normal);
// --------------------------------------------------------------------------------------

float2 GetKnernal(float2 Uv, float2 Offset, float InWidth)
{
    Offset *= InWidth;
    Offset *= GetSceneTextureSize();
    return Uv + Offset;
}

float ThicknessModulation(float MaxDepth, float MinThickness, float MaxThickness)
{
    float2 UVs = GetCurrentUV();

    float Center = GetDepth(GetKnernal(UVs, float2(0, 0), MaxThickness));
    float Left = GetDepth(GetKnernal(UVs, float2(-1, 0), MaxThickness));
    float Right = GetDepth(GetKnernal(UVs, float2(1, 0), MaxThickness));
    float Up = GetDepth(GetKnernal(UVs, float2(0, -1), MaxThickness));
    float Down = GetDepth(GetKnernal(UVs, float2(0, 1), MaxThickness));

    float MinDepth = min(MaxDepth, min(Center, min(Left, min(Right, min(Up, Down)))));

    return MinThickness - (MaxThickness - MinThickness) * (MinDepth / MaxDepth);
}

float GrazingAngle(float GrazingAnglePower, float FresnelPower, flaot GrazingAngleModulationFactor)
{
    float F = Fresnel(FresnelPower, 0.04, GetNormal(GetCurrentUV()));

    return saturate((F - 1) / GrazingAnglePower + 1) * GrazingAngleModulationFactor + 1;
}

float DetectEdge_Depth(float Threshold, float Thickness)
{
    float2 UVs = GetCurrentUV();

    float Center = GetDepth(GetKnernal(UVs, float2(0, 0), Thickness));
    float Left = GetDepth(GetKnernal(UVs, float2(-1, 0), Thickness));
    float Right = GetDepth(GetKnernal(UVs, float2(1, 0), Thickness));
    float Up = GetDepth(GetKnernal(UVs, float2(0, -1), Thickness));
    float Down = GetDepth(GetKnernal(UVs, float2(0, 1), Thickness));

    float Sum = Left + Right + Up + Down;
    float V = abs(Sum - Center * 4);

    return step(Threshold, V);
}

float DetectEdge_Normal(float Threshold, float Thickness)
{
    float3 Center = GetNormal(GetKnernal(UVs, float2(0, 0)));
    float3 Left = GetNormal(GetKnernal(UVs, float2(-1, 0)));
    float3 Right = GetNormal(GetKnernal(UVs, float2(1, 0)));
    float3 Up = GetNormal(GetKnernal(UVs, float2(0, -1)));
    float3 Down = GetNormal(GetKnernal(UVs, float2(0, 1)));

    float D1 = distance(Center, Left);
    float D2 = distance(Center, Right);
    float D3 = distance(Center, Up);
    float D4 = distance(Center, Down);

    return step(Threshold, D1 + D2 + D3 + D4);
}

float DepthMask(float MaxThickness, float MaxDepth)
{
    float Center = GetDepth(GetKnernal(UVs, float2(0, 0), MaxThickness));
    float Left = GetDepth(GetKnernal(UVs, float2(-1, 0), MaxThickness));
    float Right = GetDepth(GetKnernal(UVs, float2(1, 0), MaxThickness));
    float Up = GetDepth(GetKnernal(UVs, float2(0, -1), MaxThickness));
    float Down = GetDepth(GetKnernal(UVs, float2(0, 1), MaxThickness));

    float MinDepth = min(Center, min(Left, min(Right, min(Up, Down))));

    return 1 - step(MaxDepth, MinDepth);
}


float DepthOutline_GrazingAnglePower = 1;
float DepthOutline_FresnelPower = 5;
float DepthOutline_GrazingAngleModulationFactor = 10;

float4 DepthOutline_Color;
float DepthOutline_Threshold = 4;
float DepthOutline_MinThickness = 0.1;
float DepthOutline_Thickness = 1.5;
float DepthOutline_MaxDepth = 2000;

float4 NormalOutline_Color;
float NormalOutline_Threshold = 3;
float NormalOutline_MinThickness = 0.1;
float NormalOutline_Thickness = 1.5;
float NormalOutline_MaxDepth = 2000;

float MaxDrawDistance = 500000;

float4 Main()
{
    float3 FinalColor;
    float3 SceneColor = GetSceneColor(GetCurrentUV());

    // Calc depth outline
    {
        float FinalThickness = ThicknessModulation(DepthOutline_MaxDepth, DepthOutline_MinThickness, DepthOutline_Thickness);
        float FinalThreshold = DepthOutline_Threshold * GrazingAngle(DepthOutline_GrazingAnglePower, DepthOutline_FresnelPower, DepthOutline_GrazingAngleModulationFactor);
        float V = DetectEdge_Depth(FinalThreshold, FinalThickness);
        float Mask = DepthMask(FinalThickness, MaxDrawDistance);
        FinalColor = Lerp(SceneColor, DepthOutline_Color, V * Mask);
    }

    // Calc noraml outline
    {
        float FinalThickness = ThicknessModulation(NormalOutline_MaxDepth, NormalOutline_MinThickness, NormalOutline_Thickness);
        float V = DetectEdge_Depth(NormalOutline_Threshold, FinalThickness);
        float Mask = DepthMask(FinalThickness, MaxDrawDistance);
        FinalColor = Lerp(SceneColor, NormalOutline_Color, V * Mask);
    }

    return float4(FinalColor, 1.0);
}