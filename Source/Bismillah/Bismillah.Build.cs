// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Bismillah : ModuleRules
{
	public Bismillah(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Bismillah",
			"Bismillah/Variant_Platforming",
			"Bismillah/Variant_Platforming/Animation",
			"Bismillah/Variant_Combat",
			"Bismillah/Variant_Combat/AI",
			"Bismillah/Variant_Combat/Animation",
			"Bismillah/Variant_Combat/Gameplay",
			"Bismillah/Variant_Combat/Interfaces",
			"Bismillah/Variant_Combat/UI",
			"Bismillah/Variant_SideScrolling",
			"Bismillah/Variant_SideScrolling/AI",
			"Bismillah/Variant_SideScrolling/Gameplay",
			"Bismillah/Variant_SideScrolling/Interfaces",
			"Bismillah/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
