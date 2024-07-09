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

enum class EAnimatedTextureLLMTag {
	ProjectMaxTag = (int)ELLMTag::ProjectTagStart,
	LoadGif,
	AnimTexture,
	RHITexture
};

class FAnimatedTextureModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogAnimTexture, Log, All);
