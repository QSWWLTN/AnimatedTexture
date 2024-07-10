// Copyright 2019 Neil Fang. All Rights Reserved.

using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class AnimatedTexture : ModuleRules
{
	public AnimatedTexture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory,"ThirdParty/zstd"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/common"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/compress"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/decompress"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/deprecated"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/dictBuilder"),
                Path.Combine(ModuleDirectory,"ThirdParty/zstd/legacy")
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(DirectoryReference.GetCurrentDirectory().ToString(), "Runtime/TraceLog/Private")
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Engine",
                "RHI",
                "RenderCore"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		 PublicDefinitions.AddRange(new string[]
        {
            "ZSTD_MULTITHREAD=1",
            "ZSTD_LEGACY_SUPPORT=5"
        });
        bEnableUndefinedIdentifierWarnings = false;
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
