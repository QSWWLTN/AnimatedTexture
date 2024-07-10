// Copyright 2019 Neil Fang. All Rights Reserved.

#include "AnimatedTextureModule.h"

DEFINE_LOG_CATEGORY(LogAnimTexture);
#define LOCTEXT_NAMESPACE "FAnimatedTextureModule"

DECLARE_LLM_MEMORY_STAT(TEXT("AnimTexture"), STAT_CustomGroupLLM, STATGROUP_LLM);

DECLARE_LLM_MEMORY_STAT(TEXT("AnimTexture"), STAT_AnimTexture_LLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("RHITexture"), STAT_RHITexture_LLM, STATGROUP_LLMFULL);

void FAnimatedTextureModule::StartupModule()
{
	LLM(FLowLevelMemTracker::Get().RegisterProjectTag((int32)EAnimatedTextureLLMTag::AnimTexture, TEXT("AnimTexture"), GET_STATFNAME(STAT_AnimTexture_LLM), GET_STATFNAME(STAT_CustomGroupLLM), -1));
	LLM(FLowLevelMemTracker::Get().RegisterProjectTag((int32)EAnimatedTextureLLMTag::RHITexture, TEXT("AnimRHITexture"), GET_STATFNAME(STAT_RHITexture_LLM), GET_STATFNAME(STAT_CustomGroupLLM), -1));
}

void FAnimatedTextureModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAnimatedTextureModule, AnimatedTexture)