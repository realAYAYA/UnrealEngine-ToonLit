# ToonLit rendering pipeline based on UnrealEngine5

![images](https://gitee.com/AYAYA2/LearnGI/raw/master/paper/VoxelizeDebug%20(2).png)  

#### 介绍
魔改UE5引擎，基于此实现一套ToonLit流程  
提升对引擎代码的理解  
此外包含了其它游戏相关的功能拓展，插件实现  


#### Rendering Features 渲染特性
    
1.  ToonLit ShadingModel 着色模型
    1.  GI, 为ToonLit的ShadingModel作额外的间接光照处理（天光，Lumen）
    2.  Toon diffuse(Ramp controling) in deferred rendering
2.  Outline 描边控制
    Extend custom material paramters，filted in rendering  
    Color and Weight control, stored in model's VertexColor  
    smoothed normal, generated in engine editor，stored in SkeletonMesh's uv1 slot  
    1.  Custom Pass, Backface
    2.  Post process based
        1.  Sobel
        2.  Data: Normal | Depth | TextureID
3.  Character eyes rendering，hair rendering（based on Kajiya-Kay）, coat rendering 角色各种部位渲染
4.  ToonLit postproces 后处理

#### Other Features 其它特性
1.  MMO frame, 基于Tcp连接的后端网络框架
    1. Websocket, current tcp lib, 当前使用的网tcp网络库
    2. protobuf, 使用了google的消息协议
2.  PuerTs, 接入脚本
3.  Excel support
4.  GAS


#### 使用说明

1.  git clone 该项目
2.  进入到UnrealEngine目录下生成引擎项目，编译（2小时起）
3.  使用编译过的引擎打开目录下的Demo.uproject

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


## Log

#### Add ThirdParty
· UnrealEngine\Engine\Binaries\ThirdParty\Python3\Linux | Mac | Win64\lib\site-packages\  
1. jinja2
2. markupsafe
3. xlrd
4. fnv1a
· UnrealEngine\Engine\Binaries\ThirdParty\Nodejs  
· UnrealEngine\Engine\Binaries\ThirdParty\Windows\msys  


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
