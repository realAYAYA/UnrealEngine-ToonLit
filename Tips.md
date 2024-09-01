# Tips
Postprocess Mat
分支重命名后 版本控制让我感到很迷惑  

## GBuffer
Name            Format          R           G           B           A  
GBufferA        R10G10B10A2     Normal                              PerObjectGBufferData(bit1:CastContactShadow, bit2:HasDynamicIndirectShadowCasterRepresentation)  
GBufferB        RGBA8888        Metallic    Specular    Roughness   ShadingModel+SelectiveOutputMask(4bit + 4bit)  
GBufferC        RGBA8888        BaseColor                           AmbientOcclusion  
GBufferD                        CustomData  
GBufferE                        DynamicShadow  
GBufferF                        Tangent                             Anistoropy  


SelectiveOutputMask记录了绘制时以下宏的开启结果:  

MATERIAL_USES_ANISOTROPY 禁止计算各向异性  
!GBUFFER_HAS_PRECSHADOWFACTOR 禁止读取GBufferE数据作为预计算阴影  
GBUFFER_HAS_PRECSHADOWFACTOR && WRITES_PRECSHADOWFACTOR_ZERO 当不读取GBufferE时，若此值为1时，预计算阴影设为0，否则为1。  
WRITES_VELOCITY_TO_GBUFFER 禁止从Gbuffer中读取速度值。  

AmbientOcclusion在有静态光照时候储存随机抖动过的IndirectIrradiance*Material AO  

除开必要的光照计算，开发者可以自定义拓展的Buffer: Metalic, CustomData, AO  
而如果开启各项异性通道, 则可以额外使用: Tangent, Anistoropy  

Now, what does ToonLit need to save in buffer?  
Necessary: Outline Width, Outline Id, RampId, HairShadowMask, CustomShadowMask;  
Additional: Outline Color, Outline Normal  