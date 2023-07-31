void main(
    in  float4 inPosition : SV_POSITION,
    in  float3 inColor    : COLOR,
    out float4 outColor   : SV_TARGET0)
{
    outColor = float4(inColor.xyz, 1.0);
}
