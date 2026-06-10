//==================================================================================================
//
// Physically Based Rendering shader for brushes and models
// Adopted from Zombie Master: Reborn, modified for SFM compatibility
// https://github.com/zm-reborn/zmr-game/
//
//==================================================================================================

// Includes for all shaders
#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

#include "vtf/vtf.h"

// Includes for PS30
#include "pbr_vs30.inc"
#include "pbr_mrao_ps30.inc"
#include "pbr_mrao_projtex_ps30.inc"
#include "pbr_sg_ps30.inc"
#include "pbr_sg_projtex_ps30.inc"

//#define SFM_BLACKBOX_MODE

// M/R and S/G
const Sampler_t SAMPLER_BASECOLOR = SHADER_SAMPLER0;
const Sampler_t SAMPLER_DIFFUSE = SHADER_SAMPLER0;
const Sampler_t SAMPLER_SPECULAR = SHADER_SAMPLER1;
const Sampler_t SAMPLER_MRAO = SHADER_SAMPLER1;

const Sampler_t SAMPLER_NORMAL = SHADER_SAMPLER2;

// Wrinklemapping should follow the other 3
const Sampler_t SAMPLER_COMPRESS = SHADER_SAMPLER3;
const Sampler_t SAMPLER_STRETCH = SHADER_SAMPLER4;
const Sampler_t SAMPLER_BUMPCOMPRESS = SHADER_SAMPLER5;
const Sampler_t SAMPLER_BUMPSTRETCH = SHADER_SAMPLER6;

// We always have SSAO
const Sampler_t SAMPLER_SSAO = SHADER_SAMPLER7;

// 8-12 are now usable for other Things
const Sampler_t SAMPLER_EMISSIVE = SHADER_SAMPLER8;
const Sampler_t SAMPLER_LIGHTWARP = SHADER_SAMPLER9;
const Sampler_t SAMPLER_THICKNESS = SHADER_SAMPLER10;

// Lighting is split into two parts
const Sampler_t SAMPLER_LIGHTMAP = SHADER_SAMPLER13;
const Sampler_t SAMPLER_ENVMAP = SHADER_SAMPLER14;
const Sampler_t SAMPLER_PROJTEXCOOKIE = SHADER_SAMPLER12;
const Sampler_t SAMPLER_RANDOMROTATION = SHADER_SAMPLER13;
const Sampler_t SAMPLER_SHADOWDEPTH = SHADER_SAMPLER14;
const Sampler_t SAMPLER_ENVMAP_FLASHLIGHT = SHADER_SAMPLER15;

// Convars
static ConVar pbr_version("pbr_version", "1.05", FCVAR_CHEAT);
static ConVar mat_fullbright("mat_fullbright", "0", FCVAR_CHEAT);
static ConVar mat_specular("mat_specular", "1", FCVAR_NONE);
static ConVar mat_pbr_parallaxmap("mat_pbr_parallaxmap", "1");

static ConVar pbr_microshadows_globalstrength("pbr_microshadows_globalstrength", "0.50", FCVAR_NONE);

//==========================================================================//
// Shader Start
//==========================================================================//
BEGIN_VS_SHADER(PBR, "PBR shader")

// Setting up vmt parameters
BEGIN_SHADER_PARAMS;

// Metallic/Roughness
SHADER_PARAM(BaseColor, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(MRAOTexture, SHADER_PARAM_TYPE_TEXTURE, "", "")

// Specular/Glossiness
SHADER_PARAM(Diffuse, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(Specular, SHADER_PARAM_TYPE_TEXTURE, "", "")

SHADER_PARAM(SpecularGlossiness, SHADER_PARAM_TYPE_BOOL, "", "(Internal Parameter)")

// Proper Terminology
SHADER_PARAM(BumpMap, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(NormalMap, SHADER_PARAM_TYPE_TEXTURE, "", "")

SHADER_PARAM(NormalMap_FlipR, SHADER_PARAM_TYPE_BOOL, "", "")
SHADER_PARAM(NormalMap_FlipG, SHADER_PARAM_TYPE_BOOL, "", "")
SHADER_PARAM(NormalMap_FlipB, SHADER_PARAM_TYPE_BOOL, "", "")
SHADER_PARAM(NormalMapFactor, SHADER_PARAM_TYPE_FLOAT, "", "")

SHADER_PARAM(AlphaTestReference, SHADER_PARAM_TYPE_FLOAT, "0", "")
SHADER_PARAM(AllowAlphaToCoverage, SHADER_PARAM_TYPE_BOOL, "0", "Enable alpha-to-coverage") 
SHADER_PARAM(EnvMap, SHADER_PARAM_TYPE_ENVMAP, "", "Set the cubemap for this material.")
SHADER_PARAM(EmissionTexture, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture")
SHADER_PARAM(BumpFrame, SHADER_PARAM_TYPE_INTEGER, "0", "Frame number for $bumpmap")
SHADER_PARAM(UseEnvAmbient, SHADER_PARAM_TYPE_BOOL, "0", "Use the cubemaps to compute ambient light.")
SHADER_PARAM(LightWarpTexture, SHADER_PARAM_TYPE_TEXTURE, "", "Lightwarp Texture")
SHADER_PARAM(ThicknessTexture, SHADER_PARAM_TYPE_TEXTURE, "", "Thickness map for SSS")
SHADER_PARAM(Parallax, SHADER_PARAM_TYPE_BOOL, "0", "Use Parallax Occlusion Mapping.")
SHADER_PARAM(ParallaxDepth, SHADER_PARAM_TYPE_FLOAT, "0.0030", "Depth of the Parallax Map")
SHADER_PARAM(ParallaxCenter, SHADER_PARAM_TYPE_FLOAT, "0.5", "Center depth of the Parallax Map")
SHADER_PARAM(EmissiveFactor, SHADER_PARAM_TYPE_FLOAT, "1.0", "Emissive factor")
SHADER_PARAM(SpecularFactor, SHADER_PARAM_TYPE_FLOAT, "1.0", "Specular factor")
SHADER_PARAM(SSSColor, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Subsurface scattering color")
SHADER_PARAM(SSSIntensity, SHADER_PARAM_TYPE_FLOAT, "1.0", "SSS intensity")
SHADER_PARAM(SSSPowerScale, SHADER_PARAM_TYPE_FLOAT, "1.0", "SSS power scale")
SHADER_PARAM(Compress, SHADER_PARAM_TYPE_TEXTURE, "", "Compression wrinklemap")
SHADER_PARAM(BumpCompress, SHADER_PARAM_TYPE_TEXTURE, "", "Stretch bumpmap")
SHADER_PARAM(Stretch, SHADER_PARAM_TYPE_TEXTURE, "", "Stretch wrinklemap")
SHADER_PARAM(BumpStretch, SHADER_PARAM_TYPE_TEXTURE, "", "Compression bumpmap")

SHADER_PARAM(MRAOMultiplier, SHADER_PARAM_TYPE_VEC3, "", "")
SHADER_PARAM(MRAOBias, SHADER_PARAM_TYPE_VEC3, "", "")
SHADER_PARAM(MRAOExponent, SHADER_PARAM_TYPE_VEC3, "", "")
SHADER_PARAM(MicroShadowBias, SHADER_PARAM_TYPE_FLOAT, "", "")

SHADER_PARAM(DualLobe, SHADER_PARAM_TYPE_BOOL, "", "")
SHADER_PARAM(DualLobe_RoughnessBias, SHADER_PARAM_TYPE_FLOAT, "", "")
SHADER_PARAM(DualLobe_LerpFactor, SHADER_PARAM_TYPE_FLOAT, "", "")

SHADER_PARAM(EnvDlightFactor, SHADER_PARAM_TYPE_FLOAT, "1.0", "Controls dynamic light masking on the envmap")

// --- PLANAR REFLECTIONS ---
SHADER_PARAM(PlanarReflection, SHADER_PARAM_TYPE_BOOL, "0", "Enable Planar Reflections")
SHADER_PARAM(PlanarReflectionTexture, SHADER_PARAM_TYPE_TEXTURE, "_rt_camera", "Texture for planar reflection")
SHADER_PARAM(PlanarReflectionBlurScale, SHADER_PARAM_TYPE_VEC2, "[1.0 1.0]", "X and Y Blur Scale for Planar Reflections")

// --- ENVMAP SPOOFING ---
SHADER_PARAM(EnvmapOffsetX, SHADER_PARAM_TYPE_FLOAT, "0.0", "Offset X for Envmap translation")
SHADER_PARAM(EnvmapOffsetY, SHADER_PARAM_TYPE_FLOAT, "0.0", "Offset Y for Envmap translation")
SHADER_PARAM(EnvmapOffsetZ, SHADER_PARAM_TYPE_FLOAT, "0.0", "Offset Z for Envmap translation")

// --- CAR PAINT & PEARLESCENCE ---
SHADER_PARAM(CarPaint, SHADER_PARAM_TYPE_BOOL, "0", "Enable Car Paint Mode")
SHADER_PARAM(CarPaintGlossFactor, SHADER_PARAM_TYPE_FLOAT, "1.0", "Glossiness of the clearcoat")
SHADER_PARAM(CarPaintColor, SHADER_PARAM_TYPE_COLOR, "[0.5 0.5 0.5]", "Base color for Car Paint")
SHADER_PARAM(CarPaintFlakeTexture, SHADER_PARAM_TYPE_TEXTURE, "models/carpaint/shared_flakes_normal", "")
SHADER_PARAM(FlakeContrast, SHADER_PARAM_TYPE_FLOAT, "2.0", "Contrast curve for the metallic flakes")
SHADER_PARAM(FlakeScale, SHADER_PARAM_TYPE_FLOAT, "50.0", "Scale of the metallic flakes")
SHADER_PARAM(PearlColor, SHADER_PARAM_TYPE_COLOR, "[0 0 0]", "The grazing angle color for pearlescence")
SHADER_PARAM(PearlTransition, SHADER_PARAM_TYPE_FLOAT, "2.0", "How sharply the pearl color blends in (1.0 to 5.0)")
SHADER_PARAM(PearlBlendAmount, SHADER_PARAM_TYPE_FLOAT, "0.0", "Opacity of the pearl effect (0.0 to 1.0)")

END_SHADER_PARAMS;

// Initializing parameters
SHADER_INIT_PARAMS()
{
	if (params[BaseColor]->IsDefined())
	{
		params[BaseTexture]->SetStringValue(params[BaseColor]->GetStringValue());
	}
	else if (params[Diffuse]->IsDefined())
	{
		params[BaseTexture]->SetStringValue(params[Diffuse]->GetStringValue());
		params[SpecularGlossiness]->SetIntValue(1);
	}
	else if (params[BaseTexture]->IsDefined())
	{
		params[BaseColor]->SetStringValue(params[BaseTexture]->GetStringValue());
	}

	if (params[Specular]->IsDefined())
	{
		params[SpecularGlossiness]->SetIntValue(1);
	}

	if (params[BumpMap]->IsDefined())
	{
		params[NormalMap]->SetStringValue(params[BumpMap]->GetStringValue());
	}
	else if (params[NormalMap]->IsDefined())
	{
		params[BumpMap]->SetStringValue(params[NormalMap]->GetStringValue());
	}
	else
	{
		params[BumpMap]->SetStringValue("dev/flat_normal");
	}

	if (!params[EnvMap]->IsDefined())
		params[EnvMap]->SetStringValue("env_cubemap");

	if (params[Compress]->IsDefined() || params[BumpCompress]->IsDefined() ||
		params[Stretch]->IsDefined() || params[BumpStretch]->IsDefined())
	{
		if (!params[Compress]->IsDefined())
			params[Compress]->SetStringValue(params[BaseTexture]->GetStringValue());
		if (!params[BumpCompress]->IsDefined())
			params[BumpCompress]->SetStringValue(params[BumpMap]->GetStringValue());

		if (!params[Stretch]->IsDefined())
			params[Stretch]->SetStringValue(params[BaseTexture]->GetStringValue());
		if (!params[BumpStretch]->IsDefined())
			params[BumpStretch]->SetStringValue(params[BumpMap]->GetStringValue());
	}

	InitFloatParam(EnvmapOffsetX, params, 0.0f);
	InitFloatParam(EnvmapOffsetY, params, 0.0f);
	InitFloatParam(EnvmapOffsetZ, params, 0.0f);

	InitFloatParam(CarPaintGlossFactor, params, 1.0f); // NEW GLOSS INITIALIZATION
	InitVecParam(CarPaintColor, params, 0.5f, 0.5f, 0.5f);
	InitFloatParam(FlakeContrast, params, 2.0f); // NEW CONTRAST INITIALIZATION
	InitFloatParam(FlakeScale, params, 50.0f);
	InitVecParam(PearlColor, params, 0.0f, 0.0f, 0.0f);
	InitFloatParam(PearlTransition, params, 2.0f);
	InitFloatParam(PearlBlendAmount, params, 0.0f);

	// Force Dual Lobe ON and hardcode the Flake Texture
	if (params[CarPaint]->IsDefined() && params[CarPaint]->GetIntValue())
	{
		params[DualLobe]->SetIntValue(1);
		params[CarPaintFlakeTexture]->SetStringValue("models/carpaint/shared_flakes_normal");
	}

	InitIntParam(BumpFrame, params, 0);
	InitFloatParam(NormalMapFactor, params, 1.0f);
	InitFloatParam(EmissiveFactor, params, 1.0f);
	InitFloatParam(SpecularFactor, params, 1.0f);
	InitFloatParam(SSSIntensity, params, 1.0f);
	InitFloatParam(SSSPowerScale, params, 1.0f);
	InitVecParam(SSSColor, params, 1, 1, 1);
	InitFloatParam(DualLobe_RoughnessBias, params, -0.2f);
	InitFloatParam(DualLobe_LerpFactor, params, 0.5f);

	if (!mat_pbr_parallaxmap.GetBool() || params[Compress]->IsDefined())
	{
		params[Parallax]->SetIntValue(0);
	}

	InitVecParam(MRAOMultiplier, params, 1.0f, 1.0f, 1.0f);
	InitVecParam(MRAOExponent, params, 1.0f, 1.0f, 1.0f);
	InitFloatParam(MicroShadowBias, params, 0.0f);

	if (!params[MRAOTexture]->IsDefined() && params[SpecularGlossiness]->GetIntValue() == 0)
	{
		InitVecParam(MRAOBias, params, -1.0f, -0.2f, 0.0f, 0.0f);
	}
	else if (!params[Specular]->IsDefined() && params[SpecularGlossiness]->GetIntValue() != 0)
	{
		InitVecParam(MRAOBias, params, -1.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		InitVecParam(MRAOBias, params, 0.0f, 0.0f, 0.0f);
	}
};

SHADER_FALLBACK
{
	return 0;
};

SHADER_INIT
{
	LoadTexture(BaseTexture, TEXTUREFLAGS_SRGB);
	LoadTexture(BaseColor, TEXTUREFLAGS_SRGB);
	LoadTexture(Diffuse, TEXTUREFLAGS_SRGB);
	LoadTexture(MRAOTexture, NULL);
	LoadTexture(Specular, NULL);
	LoadBumpMap(BumpMap);
	LoadBumpMap(NormalMap);

	int nEnvMapFlags = g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0;
	nEnvMapFlags |= TEXTUREFLAGS_ALL_MIPS;
	LoadCubeMap(EnvMap, nEnvMapFlags);

	LoadTexture(EmissionTexture, TEXTUREFLAGS_SRGB);

	if (params[PlanarReflection]->GetIntValue())
	{
		LoadTexture(PlanarReflectionTexture);
	}

	LoadTexture(ThicknessTexture);

	if (params[CarPaint]->GetIntValue())
	{
		LoadBumpMap(CarPaintFlakeTexture);
	}

	LoadTexture(LightWarpTexture);

	if (params[Compress]->IsDefined())
	{
		LoadTexture(Compress, TEXTUREFLAGS_SRGB);
		LoadBumpMap(BumpCompress);
		LoadTexture(Stretch, TEXTUREFLAGS_SRGB);
		LoadBumpMap(BumpStretch);
	}

	if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
	{
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);
		SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);
	}
	else
	{
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_LIGHTMAP);
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP);
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);
	}

	SET_FLAGS2(MATERIAL_VAR2_USE_GBUFFER0);
	SET_FLAGS2(MATERIAL_VAR2_USE_GBUFFER1);
};

SHADER_DRAW
{
	bool bHasFlashlight = UsingFlashlight(params);
	bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;

	bool bSpecularGlossiness = params[SpecularGlossiness]->GetIntValue() != 0;
	bool bHasBaseColor = !bSpecularGlossiness && params[BaseColor]->IsTexture();
	bool bHasMRAOTexture = !bSpecularGlossiness && params[MRAOTexture]->IsTexture();
	bool bHasDiffuse = bSpecularGlossiness && params[Diffuse]->IsTexture();
	bool bHasSpecular = bSpecularGlossiness && params[Specular]->IsTexture();
	bool bHasNormalMap = params[NormalMap]->IsTexture();
	bool bHasEmissionTexture = params[EmissionTexture]->IsTexture();

#ifndef SFM_BLACKBOX_MODE
		bool bHasEnvMap = params[EnvMap]->IsTexture();
#endif
		bool bHasDualLobe = params[DualLobe]->GetIntValue() != 0;
		bool bHasColor = true;
		bool bLightMapped = !IS_FLAG_SET(MATERIAL_VAR_MODEL);
		bool bThicknessTexture = !bLightMapped && params[ThicknessTexture]->IsTexture();
		bool bLightwarpTexture = !bThicknessTexture && params[LightWarpTexture]->IsTexture();
		bool bWrinkleMapping = !bLightMapped && params[Compress]->IsTexture();
		bool bHasParallax = params[Parallax]->GetIntValue() != 0;

		BlendType_t nBlendType = EvaluateBlendRequirements(BaseTexture, true);
		bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

		if (IsSnapshotting())
		{
			// Move the bWorldNormal evaluation up here so we can read it early
			bool bWorldNormal = (ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH ==
				(IS_FLAG2_SET(MATERIAL_VAR2_USE_GBUFFER0) + 2 * IS_FLAG2_SET(MATERIAL_VAR2_USE_GBUFFER1)));

			if (bIsAlphaTested)
			{
				bool bAlphaToCoverage = params[AllowAlphaToCoverage]->IsDefined() && params[AllowAlphaToCoverage]->GetIntValue();

				if (!bWorldNormal)
				{
					if (bAlphaToCoverage)
					{
						// A2C Enabled: Let the hardware MSAA resolve the alpha gradient
						pShaderShadow->EnableAlphaTest(false);
						pShaderShadow->EnableAlphaToCoverage(true);
					}
					else
					{
						// Standard Alphatest: Binary hardware clipping
						pShaderShadow->EnableAlphaToCoverage(false);
						pShaderShadow->EnableAlphaTest(true);
						const float f1AlphaTestReference = params[AlphaTestReference]->GetFloatValue();
						if (f1AlphaTestReference > 0.0f)
						{
							pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, f1AlphaTestReference);
						}
					}
				}
				else
				{
					// SSAO Depth Pass: Disable all hardware transparency! 
					// Our HLSL clip() will handle the geometry cutouts here.
					pShaderShadow->EnableAlphaTest(false);
					pShaderShadow->EnableAlphaToCoverage(false);
				}
			}

			// ... (Continue with the rest of your flashlight/blending setup)

			if (bHasFlashlight)
			{
				if (IS_FLAG_SET(MATERIAL_VAR_TRANSLUCENT))
				{
					pShaderShadow->EnableBlending(true);
					pShaderShadow->BlendFunc(SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE);
				}
				else
				{
					pShaderShadow->EnableBlending(true);
					pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE);
				}
			}
			else
			{
				SetDefaultBlendingShadowState(BaseTexture, true);
			}

			pShaderShadow->EnableSRGBWrite(true);

			if (bHasFlashlight)
				FogToBlack();
			else
				DefaultFog();

			// Ensure the G-Buffer always receives the depth data in the alpha channel!
			if (bWorldNormal)
			{
				pShaderShadow->EnableAlphaWrites(true);
			}
			else
			{
				pShaderShadow->EnableAlphaWrites(bFullyOpaque);
			}

			if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
			{
				unsigned int nFlags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
				int nTexCoords = 1;
				int nUserDataSize = 4;
				pShaderShadow->VertexShaderVertexFormat(nFlags, nTexCoords, NULL, nUserDataSize);
			}
			else
			{
				unsigned int nFlags = VERTEX_POSITION | VERTEX_NORMAL;
				int nTexCoords = 3;
				int nUserDataSize = 0;
				pShaderShadow->VertexShaderVertexFormat(nFlags, nTexCoords, NULL, nUserDataSize);
			}

			if (bSpecularGlossiness)
			{
				pShaderShadow->EnableTexture(SAMPLER_DIFFUSE, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_DIFFUSE, true);
				pShaderShadow->EnableTexture(SAMPLER_SPECULAR, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_SPECULAR, true);
			}
			else
			{
				pShaderShadow->EnableTexture(SAMPLER_BASECOLOR, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_BASECOLOR, true);
				pShaderShadow->EnableTexture(SAMPLER_MRAO, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_MRAO, false);
			}
			pShaderShadow->EnableTexture(SAMPLER_NORMAL, true);
			pShaderShadow->EnableSRGBRead(SAMPLER_NORMAL, false);

			if (bWrinkleMapping)
			{
				pShaderShadow->EnableTexture(SAMPLER_COMPRESS, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_COMPRESS, true);
				pShaderShadow->EnableTexture(SAMPLER_STRETCH, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_STRETCH, true);
				pShaderShadow->EnableTexture(SAMPLER_BUMPCOMPRESS, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_BUMPCOMPRESS, false);
				pShaderShadow->EnableTexture(SAMPLER_BUMPSTRETCH, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_BUMPSTRETCH, false);
			}

			pShaderShadow->EnableTexture(SAMPLER_SSAO, true);
			pShaderShadow->EnableSRGBRead(SAMPLER_SSAO, true);

			if (bHasEmissionTexture)
			{
				pShaderShadow->EnableTexture(SAMPLER_EMISSIVE, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE, true);
			}

			if (bLightwarpTexture)
			{
				pShaderShadow->EnableTexture(SAMPLER_LIGHTWARP, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTWARP, false);
			}
			else
			{
				// We enable s10 unconditionally here so it can be either Thickness OR Flakes
				pShaderShadow->EnableTexture(SHADER_SAMPLER10, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER10, false);
			}

			if (bHasFlashlight)
			{
				pShaderShadow->EnableTexture(SAMPLER_PROJTEXCOOKIE, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_PROJTEXCOOKIE, true);
				pShaderShadow->EnableTexture(SAMPLER_RANDOMROTATION, true);
				pShaderShadow->EnableTexture(SAMPLER_SHADOWDEPTH, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_SHADOWDEPTH, false);
				pShaderShadow->SetShadowDepthFiltering(SAMPLER_SHADOWDEPTH);
				pShaderShadow->EnableTexture(SAMPLER_ENVMAP_FLASHLIGHT, true);
			}
			else
			{
#ifndef SFM_BLACKBOX_MODE
				if (bHasEnvMap)
				{
					pShaderShadow->EnableTexture(SAMPLER_ENVMAP, true);
					if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
						pShaderShadow->EnableSRGBRead(SAMPLER_ENVMAP, true);
				}
#endif
				if (bLightMapped)
				{
					pShaderShadow->EnableTexture(SAMPLER_LIGHTMAP, true);
					pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTMAP, false);
				}
			}

			if (params[PlanarReflection]->GetIntValue())
			{
				pShaderShadow->EnableTexture((Sampler_t)11, true);
				pShaderShadow->EnableSRGBRead((Sampler_t)11, true);
			}

			DECLARE_STATIC_VERTEX_SHADER(pbr_vs30);
			SET_STATIC_VERTEX_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
			SET_STATIC_VERTEX_SHADER(pbr_vs30);

			if (bHasFlashlight)
			{
				if (bSpecularGlossiness)
				{
					DECLARE_STATIC_PIXEL_SHADER(pbr_sg_projtex_ps30);
					SET_STATIC_PIXEL_SHADER_COMBO(PLANARREFLECTION, params[PlanarReflection]->GetIntValue());
					SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, g_pHardwareConfig->GetShadowFilterMode());
					SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, bHasParallax);
					SET_STATIC_PIXEL_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
					SET_STATIC_PIXEL_SHADER_COMBO(WRINKLEMAP, bWrinkleMapping);
					SET_STATIC_PIXEL_SHADER_COMBO(SUBSURFACESCATTERING, bThicknessTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(DUALLOBE, bHasDualLobe);
					SET_STATIC_PIXEL_SHADER_COMBO(ALPHATEST, bIsAlphaTested);
					SET_STATIC_PIXEL_SHADER(pbr_sg_projtex_ps30);
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER(pbr_mrao_projtex_ps30);
					SET_STATIC_PIXEL_SHADER_COMBO(PLANARREFLECTION, params[PlanarReflection]->GetIntValue());
					SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, g_pHardwareConfig->GetShadowFilterMode());
					SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, bHasParallax);
					SET_STATIC_PIXEL_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
					SET_STATIC_PIXEL_SHADER_COMBO(WRINKLEMAP, bWrinkleMapping);
					SET_STATIC_PIXEL_SHADER_COMBO(SUBSURFACESCATTERING, bThicknessTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(DUALLOBE, bHasDualLobe);
					SET_STATIC_PIXEL_SHADER_COMBO(ALPHATEST, bIsAlphaTested);
					SET_STATIC_PIXEL_SHADER(pbr_mrao_projtex_ps30);
				}
			}
			else
			{
				if (bSpecularGlossiness)
				{
					DECLARE_STATIC_PIXEL_SHADER(pbr_sg_ps30);
					SET_STATIC_PIXEL_SHADER_COMBO(PLANARREFLECTION, params[PlanarReflection]->GetIntValue());
					SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, bHasParallax);
					SET_STATIC_PIXEL_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
					SET_STATIC_PIXEL_SHADER_COMBO(WRINKLEMAP, bWrinkleMapping);
					SET_STATIC_PIXEL_SHADER_COMBO(SUBSURFACESCATTERING, bThicknessTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(DUALLOBE, bHasDualLobe);
					SET_STATIC_PIXEL_SHADER_COMBO(ALPHATEST, bIsAlphaTested);
					SET_STATIC_PIXEL_SHADER(pbr_sg_ps30);
				}
				else
				{
					DECLARE_STATIC_PIXEL_SHADER(pbr_mrao_ps30);
					SET_STATIC_PIXEL_SHADER_COMBO(PLANARREFLECTION, params[PlanarReflection]->GetIntValue());
					SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, bHasParallax);
					SET_STATIC_PIXEL_SHADER_COMBO(WORLD_NORMAL, bWorldNormal);
					SET_STATIC_PIXEL_SHADER_COMBO(WRINKLEMAP, bWrinkleMapping);
					SET_STATIC_PIXEL_SHADER_COMBO(SUBSURFACESCATTERING, bThicknessTexture);
					SET_STATIC_PIXEL_SHADER_COMBO(DUALLOBE, bHasDualLobe);
					SET_STATIC_PIXEL_SHADER_COMBO(ALPHATEST, bIsAlphaTested);
					SET_STATIC_PIXEL_SHADER(pbr_mrao_ps30);
				}
			}

			float flLScale = pShaderShadow->GetLightMapScaleFactor();

			PI_BeginCommandBuffer();

			if (!bLightMapped)
			{
#ifndef SFM_BLACKBOX_MODE
				PI_SetPixelShaderAmbientLightCube(PSREG_AMBIENT_CUBE);
#endif
				PI_SetPixelShaderLocalLighting(PSREG_LIGHT_INFO_ARRAY);
			}
			PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW(PSREG_DIFFUSE_MODULATION, flLScale);
			PI_EndCommandBuffer();
		}

		if (pShaderAPI)
		{
			bool bLightingOnly = mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE);

			if (bSpecularGlossiness)
			{
				if (!bLightingOnly && bHasDiffuse)
				{
					BindTexture(SAMPLER_DIFFUSE, Diffuse, Frame);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_DIFFUSE, TEXTURE_GREY);
				}
			}
			else
			{
				if (!bLightingOnly && bHasBaseColor)
				{
					BindTexture(SAMPLER_BASECOLOR, BaseColor, Frame);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_BASECOLOR, TEXTURE_GREY);
				}
			}

#ifndef SFM_BLACKBOX_MODE
			if (mat_specular.GetBool() && bHasEnvMap)
			{
				BindTexture(SAMPLER_ENVMAP, EnvMap, 0);
			}
			else
			{
				pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK);
			}
#endif

			if (bHasEmissionTexture)
			{
				BindTexture(SAMPLER_EMISSIVE, EmissionTexture, 0);
			}

			if (bHasNormalMap)
			{
				BindTexture(SAMPLER_NORMAL, NormalMap, BumpFrame);
			}
			else
			{
				pShaderAPI->BindStandardTexture(SAMPLER_NORMAL, TEXTURE_NORMALMAP_FLAT);
			}

			if (bSpecularGlossiness)
			{
				if (bHasSpecular)
				{
					BindTexture(SAMPLER_SPECULAR, Specular, 0);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_SPECULAR, TEXTURE_GREY_ALPHA_ZERO);
				}
			}
			else
			{
				if (bHasMRAOTexture)
				{
					BindTexture(SAMPLER_MRAO, MRAOTexture, 0);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_MRAO, TEXTURE_WHITE);
				}
			}

			if (bThicknessTexture)
			{
				BindTexture(SAMPLER_THICKNESS, ThicknessTexture, 0);
			}
			else if (bLightwarpTexture)
			{
				BindTexture(SAMPLER_LIGHTWARP, LightWarpTexture, 0);
			}

			if (bWrinkleMapping)
			{
				BindTexture(SAMPLER_COMPRESS, Compress, 0);
				BindTexture(SAMPLER_STRETCH, Stretch, 0);
				BindTexture(SAMPLER_BUMPCOMPRESS, BumpCompress, 0);
				BindTexture(SAMPLER_BUMPSTRETCH, BumpStretch, 0);
			}

			if (bLightMapped)
				s_pShaderAPI->BindStandardTexture(SAMPLER_LIGHTMAP, TEXTURE_LIGHTMAP);

			ITexture* pAOTexture = pShaderAPI->GetTextureRenderingParameter(TEXTURE_RENDERPARM_AMBIENT_OCCLUSION);
			if (pAOTexture)
				BindTexture(SAMPLER_SSAO, pAOTexture);
			else
				pShaderAPI->BindStandardTexture(SAMPLER_SSAO, TEXTURE_WHITE);

			if (params[PlanarReflection]->GetIntValue())
			{
				BindTexture((Sampler_t)11, PlanarReflectionTexture, -1);
				float cPlanarBlur[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
				if (params[PlanarReflectionBlurScale]->IsDefined())
				{
					params[PlanarReflectionBlurScale]->GetVecValue(cPlanarBlur, 2);
				}
				pShaderAPI->SetPixelShaderConstant(45, cPlanarBlur);
			}

			// --- ENVMAP TRANSLATION SPOOF ---
			float cEnvOffset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cEnvOffset[0] = params[EnvmapOffsetX]->GetFloatValue();
			cEnvOffset[1] = params[EnvmapOffsetY]->GetFloatValue();
			cEnvOffset[2] = params[EnvmapOffsetZ]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant(70, cEnvOffset);
			// --------------------------------

			// --- CAR PAINT & PEARLESCENCE ---
			if (params[CarPaint]->GetIntValue())
			{
				BindTexture(SHADER_SAMPLER10, CarPaintFlakeTexture, 0);
			}

			float cPearlColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			params[PearlColor]->GetVecValue(cPearlColor, 3);
			cPearlColor[3] = clamp(params[PearlBlendAmount]->GetFloatValue(), 0.0f, 1.0f);
			pShaderAPI->SetPixelShaderConstant(71, cPearlColor);

			float cPearlParams[4] = { params[PearlTransition]->GetFloatValue(), 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant(72, cPearlParams);

			float cCarPaint[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			params[CarPaintColor]->GetVecValue(cCarPaint, 3);
			cCarPaint[3] = params[FlakeScale]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant(73, cCarPaint);

			float cCarPaintMode[4] = {
				(float)params[CarPaint]->GetIntValue(),
				clamp(params[CarPaintGlossFactor]->GetFloatValue(), 0.0f, 1.0f),
				params[FlakeContrast]->GetFloatValue(), // Pack the contrast into Z
				0.0f
			};
			pShaderAPI->SetPixelShaderConstant(74, cCarPaintMode);
			// --------------------------------

			// --- ALPHATEST REFERENCE & A2C ---
			float cAlphaTestRef[4] = {
				clamp(params[AlphaTestReference]->GetFloatValue(), 0.0f, 1.0f),
				(params[AllowAlphaToCoverage]->IsDefined() && params[AllowAlphaToCoverage]->GetIntValue()) ? 1.0f : 0.0f,
				0.0f, 0.0f
			};
			pShaderAPI->SetPixelShaderConstant(75, cAlphaTestRef);
			// ---------------------------

			Vector4D color(0, 0, 0, 0);
			if (bHasColor)
			{
				params[Color1]->GetVecValue(color.Base(), 3);
			}
			else
			{
				color.Init(1, 1, 1);
			}
			color.w = float(mat_fullbright.GetInt() == 1);
			pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, color.Base());

			LightState_t lightState;
			pShaderAPI->GetDX9LightState(&lightState);

			float cNormalMapControls[4] =
			{
				params[NormalMap_FlipR]->GetIntValue() ? -1.0f : 1.0f,
				params[NormalMap_FlipG]->GetIntValue() ? -1.0f : 1.0f,
				params[NormalMap_FlipB]->GetIntValue() ? -1.0f : 1.0f,
				clamp(params[NormalMapFactor]->GetFloatValue(), 0.0f, 1.0f),
			};
			pShaderAPI->SetPixelShaderConstant(PSREG_SHADER_CONTROLS_2, cNormalMapControls);

			if (bHasDualLobe)
			{
				float cDualLobeControls[4] =
				{
					params[DualLobe_RoughnessBias]->GetFloatValue(),
					clamp(params[DualLobe_LerpFactor]->GetFloatValue(), 0.0f, 1.0f),
					0.0f,
					0.0f
				};
				pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUM_SCALE_BIAS_EXP, cDualLobeControls);
			}

			if (!IS_FLAG_SET(MATERIAL_VAR_MODEL))
			{
				lightState.m_bAmbientLight = false;
				lightState.m_nNumLights = 0;
			}

			FlashlightState_t flashlightState;
			VMatrix flashlightWorldToTexture;
			bool bFlashlightShadows = false;
			if (bHasFlashlight)
			{
				ITexture* pFlashlightDepthTexture;
				flashlightState = pShaderAPI->GetFlashlightStateEx(flashlightWorldToTexture, &pFlashlightDepthTexture);
				bFlashlightShadows = flashlightState.m_bEnableShadows && (pFlashlightDepthTexture != NULL);

				SetFlashLightColorFromState(flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR);

				if (pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && flashlightState.m_bEnableShadows)
				{
					BindTexture(SAMPLER_SHADOWDEPTH, pFlashlightDepthTexture, 0);
					pShaderAPI->BindStandardTexture(SAMPLER_RANDOMROTATION, TEXTURE_SHADOW_NOISE_2D);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_SHADOWDEPTH, TEXTURE_BLACK);
					pShaderAPI->BindStandardTexture(SAMPLER_RANDOMROTATION, TEXTURE_BLACK);
				}
			}

			float vEyePos_SpecExponent[4];
			pShaderAPI->GetWorldSpaceCameraPosition(vEyePos_SpecExponent);

#ifndef SFM_BLACKBOX_MODE
			int iEnvMapLOD = 6;
			auto envTexture = params[EnvMap]->GetTextureValue();
			if (envTexture)
			{
				int width = envTexture->GetMappingWidth();
				int mips = 0;
				while (width >>= 1)
					++mips;
				iEnvMapLOD = mips;
			}

			if (iEnvMapLOD > 12)
				iEnvMapLOD = 12;
			if (iEnvMapLOD < 4)
				iEnvMapLOD = 4;

			vEyePos_SpecExponent[3] = iEnvMapLOD;
#endif

			pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1);
			SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BaseTextureTransform);
			pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

			float flSSAOStrength = 1.0f;
			if (bHasFlashlight)
				flSSAOStrength *= flashlightState.m_flAmbientOcclusion;

			float cMRAOMultiplier[4];
			params[MRAOMultiplier]->GetVecValue(cMRAOMultiplier, 3);
			pShaderAPI->SetPixelShaderConstant(PSREG_PBR_MRAOMULTIPLIER, cMRAOMultiplier);

			float cMRAOBias[4];
			params[MRAOBias]->GetVecValue(cMRAOBias, 3);
			cMRAOBias[3] = params[MicroShadowBias]->GetFloatValue() + pbr_microshadows_globalstrength.GetFloat();
			cMRAOBias[3] = clamp(cMRAOBias[3], 0.0f, 1.0f);
			pShaderAPI->SetPixelShaderConstant(PSREG_PBR_MRAOBIAS, cMRAOBias);

			float cMRAOExponent[4];
			params[MRAOExponent]->GetVecValue(cMRAOExponent, 3);
			cMRAOExponent[3] = GetFloatParam(EnvDlightFactor, params, 1.0f);
			pShaderAPI->SetPixelShaderConstant(PSREG_PBR_MRAOEXPONENT, cMRAOExponent);

			float vExtraFactors[4] =
			{
				GetFloatParam(EmissiveFactor, params, 1.0f),
				GetFloatParam(SpecularFactor, params, 1.0f),
				GetFloatParam(SSSIntensity, params, 1.0f),
				GetFloatParam(SSSPowerScale, params, 1.0f)
			};
			pShaderAPI->SetPixelShaderConstant(PSREG_PBR_EXTRA_FACTORS, vExtraFactors, 1);

			float vSSSColor[4] = { 0, 0, 0, 0 };
			params[SSSColor]->GetVecValue(vSSSColor, 3);
			pShaderAPI->SetPixelShaderConstant(PSREG_PBR_SSS_COLOR, vSSSColor, 1);

			pShaderAPI->SetScreenSizeForVPOS();

			int nLightingPreviewMode = pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING);
			if (nLightingPreviewMode == ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH)
			{
				float vEyeDir[4];
				pShaderAPI->GetWorldSpaceCameraDirection(vEyeDir);

				float flFarZ = pShaderAPI->GetFarZ();
				vEyeDir[0] /= flFarZ;
				vEyeDir[1] /= flFarZ;
				vEyeDir[2] /= flFarZ;
				pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, vEyeDir);
			}

			if (bHasFlashlight)
			{
				float atten[4], pos[4], tweaks[4];
				SetFlashLightColorFromState(flashlightState, pShaderAPI, false, PSREG_FLASHLIGHT_COLOR);

				BindTexture(SAMPLER_PROJTEXCOOKIE, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame);

				if (mat_specular.GetBool() && bHasEnvMap)
				{
					BindTexture(SAMPLER_ENVMAP_FLASHLIGHT, EnvMap, 0);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP_FLASHLIGHT, TEXTURE_BLACK);
				}

				atten[0] = flashlightState.m_fConstantAtten;
				atten[1] = flashlightState.m_fLinearAtten;
				atten[2] = flashlightState.m_fQuadraticAtten;
				atten[3] = flashlightState.m_FarZAtten;
				pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

				pos[0] = flashlightState.m_vecLightOrigin[0];
				pos[1] = flashlightState.m_vecLightOrigin[1];
				pos[2] = flashlightState.m_vecLightOrigin[2];
				pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);

				pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, flashlightWorldToTexture.Base(), 4);

				tweaks[0] = ShadowFilterFromState(flashlightState);
				tweaks[1] = ShadowAttenFromState(flashlightState);
				HashShadow2DJitter(flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
				pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);

				SetupUberlightFromState(pShaderAPI, flashlightState);
			}

			if (bHasParallax)
			{
				float flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				flParams[0] = GetFloatParam(ParallaxDepth, params, 3.0f);
				flParams[1] = GetFloatParam(ParallaxCenter, params, 3.0f);
				pShaderAPI->SetPixelShaderConstant(PSREG_SHADER_CONTROLS, flParams, 1);
			}

			MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
			int numBones = pShaderAPI->GetCurrentNumBones();

			bool bWriteDepthToAlpha = false;
			bool bWriteWaterFogToAlpha = false;
			if (bFullyOpaque)
			{
				bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
				bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				AssertMsg(!(bWriteDepthToAlpha && bWriteWaterFogToAlpha),
						"Can't write two values to alpha at the same time.");
			}

			DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs30);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
			SET_DYNAMIC_VERTEX_SHADER(pbr_vs30);

			if (bHasFlashlight)
			{
				if (bSpecularGlossiness)
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(pbr_sg_projtex_ps30);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
					SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(UBERLIGHT, flashlightState.m_bUberlight);
					SET_DYNAMIC_PIXEL_SHADER(pbr_sg_projtex_ps30);
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(pbr_mrao_projtex_ps30);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
					SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(UBERLIGHT, flashlightState.m_bUberlight);
					SET_DYNAMIC_PIXEL_SHADER(pbr_mrao_projtex_ps30);
				}
			}
			else
			{
				if (bSpecularGlossiness)
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(pbr_sg_ps30);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
					SET_DYNAMIC_PIXEL_SHADER(pbr_sg_ps30);
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(pbr_mrao_ps30);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
					SET_DYNAMIC_PIXEL_SHADER(pbr_mrao_ps30);
				}
			}
		}

	   Draw();
};
END_SHADER