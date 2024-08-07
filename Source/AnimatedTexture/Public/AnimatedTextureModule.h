/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub Repo: https://github.com/neil3d/UnrealAnimatedTexturePlugin
 * GitHub Page: http://neil3d.github.io
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define UseZstd
#define ZSTD_STATIC_LINKING_ONLY

DECLARE_STATS_GROUP(TEXT("AnimTextureTimerGroup"), STATGROUP_AnimTextureTimerGroup, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ReanderAnimTexture"), STAT_ReanderAnimTexture, STATGROUP_AnimTextureTimerGroup);
DECLARE_CYCLE_STAT(TEXT("DeCompress"), STAT_DeCompress, STATGROUP_AnimTextureTimerGroup);

enum class EAnimatedTextureLLMTag {
	ProjectMaxTag = (int)ELLMTag::ProjectTagStart,
	LoadAnimTexture,
	AnimTexture,
	RHITexture,
	DecompressTexture,
	InitRHITexture
};

class FAnimatedTextureModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogAnimTexture, Log, All);
