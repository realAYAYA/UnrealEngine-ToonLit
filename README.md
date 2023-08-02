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
