// How does PP_ToonOutline.uasset work

// --------------------------------------------------------------------------------------
float3 GetSceneColor(float2 Uv);
float GetSceneTextureSize();
float2 GetCurrentUV();
float GetDepth(float2 Uv);
float3 GetNormal(float2 Uv);
float Fresnel(float ExponentIn, float BaseReflectFractionIn, float3 Normal);
// --------------------------------------------------------------------------------------

float2 IsHariMask(float2 Uv)
{
    return 1.0f;
}





float4 Main()
{
    float3 FinalColor = GetSceneColor(GetCurrentUV());

    // Calc depth outline
    {
        //计算该像素的Screen Position
        float2 scrPos = i.positionSS.xy / i.positionSS.w;
        
        //获取屏幕信息
        float4 scaledScreenParams = GetScaledScreenParams();
        
        //计算View Space的光照方向
        float3 viewLightDir = normalize(TransformWorldToViewDir(mainLight.direction));

        //计算采样点，其中_HairShadowDistace用于控制采样距离
        float2 samplingPoint = scrPos + _HairShadowDistace * viewLightDir.xy * float2(1 / scaledScreenParams.x, 1 / scaledScreenParams.y);

        //若采样点在阴影区内,则取得的value为1,作为阴影的话还得用1 - value;
        float hairShadow = 1 - SAMPLE_TEXTURE2D(_HairSoildColor, sampler_HairSoildColor, samplingPoint).r;
    }

    {
        float2 scrPos = i.positionSS.xy / i.positionSS.w;

        float4 scaledScreenParams = GetScaledScreenParams();

        //在Light Dir的基础上乘以NDC.w的倒数以修正摄像机距离所带来的变化
        float3 viewLightDir = normalize(TransformWorldToViewDir(mainLight.direction)) * (1 / i.posNDCw) ;

        float2 samplingPoint = scrPos + _HairShadowDistace * viewLightDir.xy * float2(1 / scaledScreenParams.x, 1 / scaledScreenParams.y);

        float hairShadow = 1 - SAMPLE_TEXTURE2D(_HairSoildColor, sampler_HairSoildColor, samplingPoint).r;
    }

    // 考虑深度，不过UE customdepth已经做好了
    {
        float2 scrPos = i.positionSS.xy / i.positionSS.w;
        float4 scaledScreenParams = GetScaledScreenParams();
        float3 viewLightDir = normalize(TransformWorldToViewDir(mainLight.direction)) * (1 / i.posNDCw) ;

        float2 samplingPoint = scrPos + _HairShadowDistace * viewLightDir.xy * float2(1 / scaledScreenParams.x, 1 / scaledScreenParams.y);
        //新增的“深度测试”
        float depth = (i.positionCS.z / i.positionCS.w) * 0.5 + 0.5;
        float hairDepth = SAMPLE_TEXTURE2D(_HairSoildColor, sampler_HairSoildColor, samplingPoint).g;
        //0.0001为bias，用于精度校正
        float depthCorrect = depth < hairDepth + 0.0001 ? 0 : 1;

        //float hairShadow = 1 - SAMPLE_TEXTURE2D(_HairSoildColor, sampler_HairSoildColor, samplingPoint).r;

        hairShadow = lerp(0, 1, depthCorrect);
    }

    return float4(FinalColor, 1.0);
}