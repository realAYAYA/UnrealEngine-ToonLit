# ToonLit render pipeline based on UnrealEngine5.2

#### 介绍
魔改UE5.1引擎，基于此实现一套ToonLit流程  
效果不重要，重要的是借此提升对引擎代码的理解  


#### 特性
 
1.  添加自定义Outline Pass，基于Backface绘制
    1.  描边控制，在材质球中添加自定义参数，用于在Renderer模块中过滤
    2.  顶点色控制描边颜色和权重
    3.  平滑法线，引擎中生成平滑法线，存储在SkeletonMesh资产中的UV1中
2.  新加ShadingModel，实现ToonLit
    1.  为ToonLit的ShadingModel作额外的间接光照处理（天光，Lumen）
    2.  延迟着色下的Toon Diffuse的Ramp控制
3.  ToonLit后处理
4.  角色眼睛渲染，头发渲染（Kajiya-Kay）

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
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
