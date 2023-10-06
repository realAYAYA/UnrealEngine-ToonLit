# ToonLit render pipeline based on UnrealEngine5

#### 介绍
魔改UE5引擎，基于此实现一套ToonLit流程  
效果不重要，重要的是借此提升对引擎代码的理解  


#### 特性
 
1.  添加自定义Outline Pass，基于Backface绘制
    1.  描边控制，在材质球中添加自定义参数，用于在Renderer模块中过滤
    2.  顶点色控制描边颜色和权重
    3.  平滑法线，引擎中生成平滑法线，存储在SkeletonMesh资产中的UV1中
2.  基于算子的后处理描边
    1.  Sobel
    2.  Normal | Depth | TextureID
3.  新加ShadingModel，实现ToonLit
    1.  为ToonLit的ShadingModel作额外的间接光照处理（天光，Lumen）
    2.  延迟着色下的Toon Diffuse的Ramp控制
4.  角色眼睛渲染，头发渲染（Kajiya-Kay），衣服coat
5.  ToonLit后处理

#### 使用说明

1.  git clone 该项目
2.  进入到UnrealEngine目录下生成引擎项目，编译（2小时起）
3.  使用编译过的引擎打开目录下的Demo.uproject

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  
3.  
4.  
5.  
6.  


## Log

#### Add Custom Variables in Material  
1.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInterface.h  
2.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\Material.h  
3.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstance.h  
4.UnrealEngine\Engine\Source\Runtime\Engine\Public\MaterialShared.h  
5.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  

6.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstanceBasePropertyOverrides.h  
7.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialShared.cpp  
8.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialInstance.cpp  
9.UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.h  
10.UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.cpp  

#### Add Custom MeshDrawPass  
0.新增UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\ToonRenderingSettings.h  
0.新增UnrealEngine\Engine\Source\Runtime\Engine\Private\ToonRenderingSettings.cpp  

1.新增UnrealEngine\Engine\Source\Runtime\Renderer\Private\ToonOutlinRendering.h  
2.新增UnrealEngine\Engine\Source\Runtime\Renderer\Private\ToonOutlinRendering.cpp  
3.新增UnrealEngine\Engine\Shaders\Private\ToonLit\ToonOutline.usf  
4.UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneRendering.h  
5.UnrealEngine\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.cpp  

#### Add custom PreIntegrated-Texture  
1.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\Engine.h  
2.UnrealEngine\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp  
3.UnrealEngine\Engine\Source\Runtime\Engine\Public\SceneView.h  
4.UnrealEngine\Engine\Source\Runtime\Engine\Private\SceneManagement.cpp  
5.UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneRendering.cpp  

#### Add Custom ShadingModel  
1.UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\EngineTypes.h  
2.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialShader.cpp  
3.UnrealEngine\Engine\Source\Editor\PixelInspector\Private\PixelInspectorResult.h  
4.UnrealEngine\Engine\Source\Editor\PixelInspector\Private\PixelInspectorResult.cpp  
5.UnrealEngine\Engine\Source\Runtime\RenderCore\Public\ShaderMaterial.h  
6.UnrealEngine\Engine\Source\Engine\Private\ShaderCompiler\ShaderGenerationUtil.cpp  
7.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\HLSLMaterialTranslator.cpp  
8.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  
9.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialAttributeDefinitionMap.cpp  
10.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialHLSLEmitter.cpp  
11.UnrealEngine\Engine\Source\Runtime\RenderCore\Public\ShaderMaterialDerivedHelpers  

12.UnrealEngine\Engine\Shaders\Private\ShadingCommon.ush  
13.UnrealEngine\Engine\Shaders\Private\Definitions.ush  
14.UnrealEngine\Engine\Shaders\Private\ShadingModelsMaterial.ush  
15.UnrealEngine\Engine\Shaders\Private\ClusteredDeferredShadingPixelShader.usf  
16.UnrealEngine\Engine\Shaders\Private\ShadingModels.ush  
17.UnrealEngine\Engine\Shaders\Private\BasePassPixelShader.usf  

#### Custom Toon IndirectLighting  
18.UnrealEngine\Engine\Shaders\Private\SkyLightingDiffuseShared.usf  
19.UnrealEngine\Engine\Shaders\Private\BasePassPixelShader.usf  
20.UnrealEngine\Engine\Shaders\Private\DeferredShadingCommon.ush  
21.UnrealEngine\Engine\Shaders\Private\GBufferHelpers.ush  
22.UnrealEngine\Engine\Shaders\Private\ReflectionEnvironment.usf  
23.UnrealEngine\Engine\Shaders\Private\DiffuseIndirectComposite.usf  

#### Enable Anisotropy for Shadingmodel  
1.UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  
2.UnrealEngine\Engine\Source\Runtime\Renderer\Private\AnisotropyRendering.cpp  
3.UnrealEngine\Engine\Source\Runtime\Renderer\Private\PrimitiveSceneInfo.cpp  