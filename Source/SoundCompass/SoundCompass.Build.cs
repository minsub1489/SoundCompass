// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SoundCompass : ModuleRules
{
	public SoundCompass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "AudioMixer" });

		PublicIncludePaths.AddRange(new string[] {
			"SoundCompass",
			"SoundCompass/Variant_Platforming",
			"SoundCompass/Variant_Platforming/Animation",
			"SoundCompass/Variant_Combat",
			"SoundCompass/Variant_Combat/AI",
			"SoundCompass/Variant_Combat/Animation",
			"SoundCompass/Variant_Combat/Gameplay",
			"SoundCompass/Variant_Combat/Interfaces",
			"SoundCompass/Variant_Combat/UI",
			"SoundCompass/Variant_SideScrolling",
			"SoundCompass/Variant_SideScrolling/AI",
			"SoundCompass/Variant_SideScrolling/Gameplay",
			"SoundCompass/Variant_SideScrolling/Interfaces",
			"SoundCompass/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
