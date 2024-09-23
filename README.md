# ToonLit rendering pipeline based on UnrealEngine5

![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Advertising.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonLitShow.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/SSHairShadow.png)  

## Introduction
#### Copyright Epic Games, Inc. All Rights Reserved

改造UE5引擎，基于此实现一套ToonLit流程  
提升对引擎代码的理解  
此外包含了其它游戏相关的功能拓展，插件实现  

Remembering: The code that was exchanged with days and nights, years and youth, but is no longer useful  
缅怀：用日日夜夜岁月青春换来，却不再有用的代码  

感谢入行以来遇到的所以大佬与同事，一直向他们模仿与学习 standing on the shoulders of giants  

## Rendering Features

[HowToStartToonRendering](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/HowToStartToonRendering.md)
    
- [X]   ToonLit ShadingModel
- [X]   GI with ToonLit(SkyLighting，Lumen, etc)
- [X]   Toon diffuse(self-shadow) in deferred rendering
- [X]   Outline control
- [ ]   Shadow control
    - [X]   ScreenSpace Hair Shadow
    - [X]   Face SDF
    - [ ]   Eye
- [ ]   Toon Toonmapping
- [ ]   Toon Bloom

## Other Features
1.  MMO network frame, 基于Tcp连接的后端网络框架, details: Engine/Plugins/MyProject
    1. Websocket, current tcp lib, 当前使用的网tcp网络库
    2. protobuf, 接入了google的消息协议
2.  PuerTs, 接入脚本
3.  Excel support
4.  Redis | MySql
5.  GAS


## How to use

1.  git clone .  
2.  Compile UnrealEngine  
3.  Open project Projects/Demo/Demo.uproject  
4.	Get the demo project [ToonLitContent]https://github.com/realAYAYA/ToonLitContent

## Log

### Add ThirdParty
Python  
UnrealEngine\Engine\Binaries\ThirdParty\Python3\Linux | Mac | Win64\lib\site-packages\  
1. jinja2
2. markupsafe
3. xlrd
4. fnv1a

Other lib  
· UnrealEngine\Engine\Binaries\ThirdParty\Nodejs  
· UnrealEngine\Engine\Binaries\ThirdParty\Windows\msys  


### Add Custom Variables in Material  
1. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInterface.h  
2. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\Material.h  
3. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstance.h  
4. UnrealEngine\Engine\Source\Runtime\Engine\Public\MaterialShared.h  
5. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  

6. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstanceBasePropertyOverrides.h  
7. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialShared.cpp  
8. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialInstance.cpp  
9. UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.h  
10. UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.cpp  

### Add Custom MeshDrawPass  
0. New file: UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\ToonRenderingSettings.h  
0. New file: UnrealEngine\Engine\Source\Runtime\Engine\Private\ToonRenderingSettings.cpp  

1. New file: UnrealEngine\Engine\Source\Runtime\Renderer\Private\ToonOutlinRendering.h  
2. New file: UnrealEngine\Engine\Source\Runtime\Renderer\Private\ToonOutlinRendering.cpp  
3. New file: UnrealEngine\Engine\Shaders\Private\ToonLit\ToonOutline.usf  
4. UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneRendering.h  
5. UnrealEngine\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.cpp  

### Add custom PreIntegrated-Texture  
1. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\Engine.h  
2. UnrealEngine\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp  
3. UnrealEngine\Engine\Source\Runtime\Engine\Public\SceneView.h  
4. UnrealEngine\Engine\Source\Runtime\Engine\Private\SceneManagement.cpp  
5. UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneRendering.cpp  

### Add Custom ShadingModel  
1. UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\EngineTypes.h  
2. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialShader.cpp  
3. UnrealEngine\Engine\Source\Editor\PixelInspector\Private\PixelInspectorResult.h  
4. UnrealEngine\Engine\Source\Editor\PixelInspector\Private\PixelInspectorResult.cpp  
5. UnrealEngine\Engine\Source\Runtime\RenderCore\Public\ShaderMaterial.h  
6. UnrealEngine\Engine\Source\Engine\Private\ShaderCompiler\ShaderGenerationUtil.cpp  
7. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\HLSLMaterialTranslator.cpp  
8. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  
9. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialAttributeDefinitionMap.cpp  
10. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialHLSLEmitter.cpp  
11. UnrealEngine\Engine\Source\Runtime\RenderCore\Public\ShaderMaterialDerivedHelpers  

12. UnrealEngine\Engine\Shaders\Private\ShadingCommon.ush  
13. UnrealEngine\Engine\Shaders\Private\Definitions.ush  
14. UnrealEngine\Engine\Shaders\Private\ShadingModelsMaterial.ush  
15. UnrealEngine\Engine\Shaders\Private\ClusteredDeferredShadingPixelShader.usf  
16. UnrealEngine\Engine\Shaders\Private\ShadingModels.ush  
17. UnrealEngine\Engine\Shaders\Private\BasePassPixelShader.usf  

### Custom Toon IndirectLighting  
18. UnrealEngine\Engine\Shaders\Private\SkyLightingDiffuseShared.usf  
19. UnrealEngine\Engine\Shaders\Private\BasePassPixelShader.usf  
20. UnrealEngine\Engine\Shaders\Private\DeferredShadingCommon.ush  
21. UnrealEngine\Engine\Shaders\Private\GBufferHelpers.ush  
22. UnrealEngine\Engine\Shaders\Private\ReflectionEnvironment.usf  
23. UnrealEngine\Engine\Shaders\Private\DiffuseIndirectComposite.usf  

### Enable Anisotropy for Shadingmodel  
1. UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp  
2. UnrealEngine\Engine\Source\Runtime\Renderer\Private\AnisotropyRendering.cpp  
3. UnrealEngine\Engine\Source\Runtime\Renderer\Private\PrimitiveSceneInfo.cpp  
