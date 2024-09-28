# How to start ToonLit

## Ramp
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp01.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp02.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp03.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp04.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp05.png)  

## ToonShadingModel
First, I try my best not to malloc additional gpu memory(ToonBuffer, etc) to implement ToonLit  
Everything should be implemented locally as much as possible, because it will introduce fewer problems  

ToonShadingModel is designed to complement the unreal shadingmodels  
For now, it includes the following types:  
Default(0), FaceNoSdf(1), Stocking(2), (3), Eye(4), Face(5), EyeWhite(6), Hair(7)  
When 'ToonShadingModel' > 3, it will not receive shadow  

Ensure your M_FileName.uasset has been set ShadingModel with ToonLit  
For example:  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel01.png)  
Above picture, we can see the ToonShadingModel(Face) was set on 'ToonData0', which is GBufferD.a  
Also, there are another 'Channel' follow the ToonShadingModel, not used yet, it can be used for something else in the future  
For some purposes, you can try extract ToonShadingModel in postprocess materials, look like:  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel02.png)  

### ToonLit materials output expression
I reassigned the GBuffer for ToonLit  
Depending on the ToonShadingModel, their definitions and purpose may vary  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel03.png)  
Todo: Introduce the purpose of material expressions for different ToonShadingModel separately  

## Shadow control
Todo  
### SelfShadow
* Face(SDF)
* Hair(SSHS)(VSM)
* Eye

## Outline
I supplied two solutions to render toon outline  

### Backface outline
I extented source code of Material, you can set a 'Outline Material' in material(include instance) property pannel  
then you can render outline for what model sections you want  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Outline02.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Outline03.png)  
Here it looks like:  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/SSHairShadow.png)  

* About how to make a outline material
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Outline05.png)  
Todo  

### Postprocess outline
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Outline01.png)  
Todo   
1.  Sobel
2.  Data: Normal | Depth | TextureID
3.  SmoothNormal

## Postprocess

## CustomDepthStencil in material property pannel