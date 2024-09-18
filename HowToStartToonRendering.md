# How to start ToonLit

### Ramp
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp01.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp02.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp03.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp04.png)  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/Ramp05.png)  

### ToonShadingModel
First, I try my best not to malloc additional gpu memory(ToonBuffer, etc) to implement ToonLit  
Everything should be implemented locally as much as possible, because it will introduce fewer problems  

ToonShadingModel is designed to complement the unreal shadingmodels  
For now, it includes the following types: Default(0), (1), Stocking(2), (3), Eye(4), Face(5), (6), Hair(7)  

Ensure your M_FileName.uasset has been set ShadingModel with ToonLit  
For example:  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel01.png)  
Above picture, we can see the ToonShadingModel(Face) was set on 'ToonData0', which is GBufferD.a  
Also, there are another 'Channel' follow the ToonShadingModel, not used yet, it can be used for something else in future  
For some purposes, you can try extract ToonShadingModel in postprocess materials, look like:  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel02.png)  

#### ToonLit materials output expression
I reassigned the GBuffer for ToonLit  
Depending on the ToonShadingModel, their definitions and purpose may vary  
![images](https://github.com/realAYAYA/UnrealEngine-ToonLit/blob/5.4/Features/ToonShadingModel03.png)  
Todo: Introduce the purpose of material expressions for different ToonShadingModel separately  

### Outline
Todo  

### Postprocess
Todo  