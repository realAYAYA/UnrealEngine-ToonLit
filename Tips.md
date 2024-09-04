# Tips
Postprocess Mat


卡通角色美术管线：  
1. 角色身上的透明部分必须在材质球层面单独分离出来, 可以Mask（不准因为一个透明件而将整个部位使用半透明贴图）  
    1. Buffer类型 BaseColor(3), RampId(1), Specular(1), Roughness(1), AO(1), Opacity(1)
2. Eye rendering:   
3. Hair rendering:  


真是 很多东西 心里觉得实现起来思路明确，只有实际做起来才真是各种踩坑  
实践太重要了  

我想说  

我们有很多想实现的东西，对于这些实现我们心里多少都有思路  
但如果只有思路却迟迟不做，自己会擅自的觉得这些实现难度也就那么回事  
从而耽误进步  
但另一方面，给自己找个机会去亲手落实想法，本身就很可贵了  



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

除开必要的光照计算，开发者可以自定义拓展的Buffer: Metalic(1), CustomData(2)  
而如果开启各项异性通道, 则可以额外使用: Tangent, Anistoropy  

Now, what does ToonLit need to save in buffer?  
Necessary:                  Outline Width, Outline Id, RampId  
Necessary but not GBuffer:  HairShadowMask, CustomShadowMask  
Additional:                 Outline Color, Outline Normal  