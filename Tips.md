# Tips
Postprocess Mat  
力求最小拓展, 不搞花里胡哨(没事不要乱加TBuffer! 不要把渲染计算复杂化)  


卡通角色美术管线：  
1. Texture data组织: BaseColor(3), Normal(3), RampId(1), Specular(1), Roughness(1), AO(1), Opacity(1), OutlineId(1)  
2. 什么时候必须单独拆出来材质球? (1)透明部分(不准因为一个透明件而将整个部位使用半透明贴图);(2)不希望走ToonLit渲染流程(PBR, 比如身上的金属和宝石, etc);(3)双面材质  
3. Eye rendering:   
4. Hair rendering:  
5. 设计角色时需要谨慎的地方，(1)特殊材质要求，比如特殊质感的衣服、带绒毛物件、等等  

制作纹理时不要使用原画光照信息，不要添加描边，描边控制有单独流程  
理论上一个角色基本需要'3'种材质球即可完成渲染（除开眼睛、头发），它们分别是ToonLit、半透明、非ToonLit，以及它们可能需要的双面  
如果必须有多个贴图，不透明（或Mask）和半透明的分开，然后建议皮肤和非皮肤分开  
贴图尺寸一定为2次幂，不接受反驳  


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

Final plan(最终安排):  
对GBuffer的安排, Metaillic用作RampId, CustomData.a用作ToonShadingModel参数, CustomData.rgb视ToonShadingModel不同作不同处理  
先说ToonShadingModel, 首先是UE自带的ShadingModel槽位不够用(且大可不必非要在C++中拓展)  
ToonShadingModel的作用在于Shader中可以依据此变量来决定GBuffer-CustomData.rgb的数据初始化, 以及光照计算放方法的选择  
ToonShadingModel目前暂定如下几种:  
Default(0):     默认卡通渲染  
Pbr(1):         属于ToonLit下但还是走Pbr流程, 这样可以吃到描边  
Stocking(2):    丝袜渲染, 带有此表面计算  
(3):  
Eye(4):         独有的光照和阴影计算, 不参与描边  
Face(5):        独有的光照和阴影计算, 自身不产生投影  
(6):  
Hair(7):        头发高光计算, 需要切线  
这是参数还影响了描边的处理: Eye不进行描边  