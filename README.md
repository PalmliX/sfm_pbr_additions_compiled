### This repo purely acts as a means to host a compiled version of @WhiteRedDragons fork of @ficool2 's SFM PBR Shader, as well my attempt at some documentation based on what I have been able to figure out so far *(also perhaps some AI assisted tweaks of my own.)* 

This version gets loaded over top of the existing workshop plugin. It doesn't replace or change it in any way (as long you don't copy it into the workshop folder of course) and it's easy to revert if needed.
## Fixes and new Features compared to the current workshop version
#### SSS *(Sub-Surface Scattering)*
#### Working POM *(Parallax Occlusion Mapping)*
#### Dual-Lobe Specular *(Also known as micro roughness, basically can make skin and similar materials look more realistic by blending two types of highlights together in the same material)*
#### SSAO now appears correctly
#### $nocull materials are now lit properly i.e. they don't become lit from behind.
#### Many additional material settings for things like normal map intensity, channel inverting, metallic/roughness bias, exponent and more!
## Fixes and new Features compared to @WhiteRedDragons fork
#### Restored cubemap rendering and added a feature whereby envmaps (cubemaps) no longer glow in the dark and are now masked by SFM dynamic lights. 
#### This feature is controllable (off, blend, or overdrive) via a material parameter ($envdlightfactor). See parameters list below for more details.
#### *These changes were written with the help of AI and as is fairly hacky as you'd imagine since it looks like based on @WhiteRedDragons code comments they're working on a better solution for all of it so this is only temporary until then.*

# Install
## *You MUST already be subscribed to the PBR Shader from the workshop and it must be working*
#### 1) Download the latest SFM-PBR-shader_modifiedthexapbr.zip from the releases page and extract it into your SFM usermod folder so you have usermod/addons and usermod/shaders when finished.
#### *Alternatively, you can create a new mod folder and make sure it's loaded ABOVE the workshop folder in usermod/gameinfo.txt* 
#### 2) Run SFM, it's that easy! Try one of the new shader features/parameters in your vmt file to see if it's working. If you want to revert back to the workshop version simply rename, delete, or move the addons and shaders folders that you extracted.

# VMT Parameters
***PLEASE NOTE:*** *I wasn't able to get a specular/glossiness setup working with this shader so for now I will only be focusing on metallic/roughnes.*
### Texture Maps *(path to VTF)*
#### $BaseTexture - *Albedo/color in RGB and transparency mask in alpha.*
#### $BumpMap - *Normal map in RGB and height map for POM in alpha. Channels can be inverted via VMT command so no need to worry about normal map format (i.e. Green+-)*
#### $MRAOTexture - *Metalness in R, Roughness in G and Oclussion in B. If you don't have an oclussion map you can either use the blue channel of the normal map or fill it with white.*
#### $EmissionTexture - *Emissive texture; is a color texture, not a mask*
#### $ThicknessTexture - *Thickness texture used for SSS. Grayscale, black is thin, i.e. most amount of SSS glow, white is thick, i.e. least amount of SSS.*
#### $LightWarpTexture - *Not tested, I assume it works the same as the standard light warp*
#### $EnvMap - *Path to cubemap, leave same as original shader unless you want to change it of course*
#### $Compress - *Compression wrinklemap*
#### $BumpCompress - *Stretch bumpmap*
#### $Stretch - *Stretch wrinklemap*
#### $BumpStretch - *Compression bumpmap*

## Adjustment Parameters

#### $EnvDlightFactor - *New envmap feature. Float, 0 is off, i.e. old behavior with glow in the dark envmaps. 1 is 100% masked by SFM lights, or any blend in-between. Any values larger than 1 act as an  overdrive to increase envmap brightness but still 100% masked by SFM lights. I've used values around 10 or 15 for void maps for a dim cubemap*
#### $Model - *Boolean, must be set to 1 for models*
#### $AlphaTest - *Boolean, same as VertexLitGeneric - Enable or disable hard edge alpha transparency. Transparency mask must be in the Alpha channel of the basetexture*
#### $NormalMap_FlipR - *Boolean, inverts Normal Map Red Channel*
#### $NormalMap_FlipG - *Boolean, inverts Normal Map Green Channel*
#### $NormalMap_FlipB - *Boolean, inverts Normal Map Blue Channel*
#### $NormalMapFactor - *Float, Normal Map Intensity (yay!)*

##### *What follows is more or less a code dump with a little bit of cleanup, haven't had time to go through all of it yet but figured I should at least make it available as a quick resource. Will update later*
#### AlphaTestReference,		SHADER_PARAM_TYPE_FLOAT, "0"
#### BumpFrame					SHADER_PARAM_TYPE_INTEGER, "0" "Frame number for $bumpmap")
#### UseEnvAmbient				SHADER_PARAM_TYPE_BOOL, "0" 
#### "Use the cubemaps to compute ambient light."
#### Parallax					SHADER_PARAM_TYPE_BOOL, "0" 
#### "Use Parallax Occlusion Mapping."
#### ParallaxDepth				SHADER_PARAM_TYPE_FLOAT, "0.0030", "Depth of the Parallax Map")
#### ParallaxCenter			SHADER_PARAM_TYPE_FLOAT, "0.5" 
#### "Center depth of the Parallax Map"
#### EmissiveFactor			SHADER_PARAM_TYPE_FLOAT, "1.0"
#### "Emissive factor"
#### SSSColor					SHADER_PARAM_TYPE_COLOR, "[1 1 1]" "Subsurface scattering color"
#### SSSIntensity				SHADER_PARAM_TYPE_FLOAT, "1.0" "SSS intensity")
#### SSSPowerScale				SHADER_PARAM_TYPE_FLOAT, "1.0"
#### "SSS power scale"
#### MRAOMultiplier			SHADER_PARAM_TYPE_VEC3 
#### (use this instead of old Metalness/Roughness/AO factor params)
#### MRAOBias					SHADER_PARAM_TYPE_VEC3
#### MRAOExponent				SHADER_PARAM_TYPE_VEC3
#### MicroShadowBias			SHADER_PARAM_TYPE_FLOAT 
#### (controls SSAO intensity)
#### DualLobe					SHADER_PARAM_TYPE_BOOL, ""
#### DualLobe_RoughnessBias	SHADER_PARAM_TYPE_FLOAT "-0.2"
#### DualLobe_LerpFactor		SHADER_PARAM_TYPE_FLOAT "0.5"

# SFM PBR

Plugin that adds a true PBR shader to Source Filmmaker, based on the PBR shader made by the [Zombie Master: Reborn](https://github.com/zm-reborn) team.

Check out the [Steam Workshop page](https://steamcommunity.com/sharedfiles/filedetails/?id=3671463307)!

Compared to ZMR's implementation, there is additional fixes for SFM compatibility, as well as new additions such as MRAO factor parameters.

This repository is a stripped version of the [Alien Swarm SDK](https://github.com/Nican/swarm-sdk).

## Building
	
To build the plugin, open the .sln in Visual Studio 2022 or newer and build. Place the compiled DLL into SFM's `addons` folder.

To build the shaders, run the `buildsfmshaders.bat` in `src/materialsystem/stdshaders`. Place the compiled FXC files into SFM's `shaders/fxc/` folder.
