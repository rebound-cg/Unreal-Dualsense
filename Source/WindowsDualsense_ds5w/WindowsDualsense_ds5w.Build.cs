// Copyright (c) 2025 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
// macOS hidapi support by Brandon Bahn.
// Planned Release Year: 2025
 
using System.IO;
using UnrealBuildTool;
 
public class WindowsDualsense_ds5w : ModuleRules
{
    public WindowsDualsense_ds5w(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
 
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ApplicationCore", "InputCore", "InputDevice" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "AudioMixer", "SignalProcessing", "AudioExtensions", "AudioPlatformConfiguration" });
 
        var gamepadCoreRoot = Path.Combine(ModuleDirectory, "Private", "GamepadCore");
        var sanitizedPath = gamepadCoreRoot.Replace("\\", "/");
        PublicDefinitions.Add($"GAMEPAD_CORE_PROJECT_ROOT=\"{sanitizedPath}\"");
        PublicDefinitions.Add("GAMEPAD_CORE_HAS_AUDIO=1");
 
        PublicIncludePaths.Add(Path.Combine(gamepadCoreRoot, "Source", "Public"));
        PrivateIncludePaths.Add(Path.Combine(gamepadCoreRoot, "Source", "Private"));
        PublicIncludePaths.Add(Path.Combine(gamepadCoreRoot, "Libs"));
        PrivateIncludePaths.Add(Path.Combine(gamepadCoreRoot, "Libs", "miniaudio"));
 
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("hid.lib");
            PublicSystemLibraries.Add("Cfgmgr32.lib");
        }
 
        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "SDL2" });
        }
 
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // hidapi via Homebrew
            PublicSystemIncludePaths.Add("/opt/homebrew/include");
            PublicAdditionalLibraries.Add("/opt/homebrew/lib/libhidapi.dylib");
 
            // macOS frameworks required by hidapi's IOKit backend
            PublicFrameworks.AddRange(new string[] { "IOKit", "CoreFoundation" });
        }
    }
}