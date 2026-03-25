// Copyright (c) 2025 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
// macOS platform implementation by Brandon Bahn.
// Planned Release Year: 2025

#include "WindowsDualsense_ds5w/Public/WindowsDualsense_ds5w.h"
#include "API/SonyGamepadProxyHelpers.h"
#include "GCore/Interfaces/IPlatformHardwareInfo.h"
#include "Helpers/DualSenseLog.h"
#include "Implementations/Adapters/DeviceRegistry.h"

#if PLATFORM_WINDOWS
#include "Implementations/Platforms/Windows/WindowsHardwarePolicy.h"
#endif

#if PLATFORM_LINUX
#include "Implementations/Platforms/Commons/LinuxHardwarePolicy.h"
#include "Framework/Application/SlateApplication.h"
#include "Subsystems/SonyInputProcessor.h"
#include "SDL.h"
#endif

#if PLATFORM_MAC
#include "Implementations/Platforms/Mac/MacHardwarePolicy.h"
#include "Framework/Application/SlateApplication.h"
#include "Subsystems/SonyInputProcessor.h"
#include <hidapi/hidapi.h>
#endif

#include "DeviceManager.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FWindowsDualsense_ds5wModule"

void FWindowsDualsense_ds5wModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	RegisterCustomKeys();

#if PLATFORM_WINDOWS
	// Initialize PlatformHardware
	std::unique_ptr<IPlatformHardwareInfo> WindowsInstance = std::make_unique<FWindowsPlatform::FWindowsHardware>();
	IPlatformHardwareInfo::SetInstance(std::move(WindowsInstance));

	// Initialize FDeviceRegistry
	FDeviceRegistry::Initialize();

#elif PLATFORM_LINUX
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
	{
		UE_LOG(LogDualSense, Error, TEXT("Failed to initialize subsystems of SDL: %s"), UTF8_TO_TCHAR(SDL_GetError()));
	}

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<FSonyInputProcessor> SonyInputProcessor = MakeShared<FSonyInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(SonyInputProcessor);
	}

	std::unique_ptr<IPlatformHardwareInfo> LinuxInstance = std::make_unique<FLinuxPlatform::FLinuxHardware>();
	IPlatformHardwareInfo::SetInstance(std::move(LinuxInstance));
	FDeviceRegistry::Initialize();

#elif PLATFORM_MAC
	if (hid_init() != 0)
	{
		UE_LOG(LogDualSense, Error, TEXT("Failed to initialize hidapi for macOS"));
	}

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<FSonyInputProcessor> SonyInputProcessor = MakeShared<FSonyInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(SonyInputProcessor);
	}

	std::unique_ptr<IPlatformHardwareInfo> MacInstance = std::make_unique<FMacPlatform::FMacHardware>();
	IPlatformHardwareInfo::SetInstance(std::move(MacInstance));
	FDeviceRegistry::Initialize();
#endif
}

void FWindowsDualsense_ds5wModule::ShutdownModule()
{
#if PLATFORM_LINUX
	SDL_Quit();
#endif
#if PLATFORM_MAC
	hid_exit();
#endif
#if PLATFORM_LINUX || PLATFORM_MAC
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SonyInputProcessor);
	}
#endif
}

FWindowsDualsense_ds5wModule::FCustomInputDeviceFactory FWindowsDualsense_ds5wModule::CustomInputDeviceFactory = nullptr;

TSharedPtr<IInputDevice> FWindowsDualsense_ds5wModule::CreateInputDevice(
    const TSharedRef<FGenericApplicationMessageHandler>& InCustomMessageHandler)
{
	if (CustomInputDeviceFactory)
	{
		return CustomInputDeviceFactory(InCustomMessageHandler);
	}

	return MakeShareable(new DeviceManager(InCustomMessageHandler));
}

void FWindowsDualsense_ds5wModule::SetCustomInputDeviceFactory(FCustomInputDeviceFactory Factory)
{
	CustomInputDeviceFactory = Factory;
}

void FWindowsDualsense_ds5wModule::RegisterCustomKeys()
{
	const FKey Mic("PS_Mic");
	const FKey Menu("PS_Menu");
	const FKey Shared("PS_Share");
	const FKey TouchButtom("PS_TouchButtom");
	const FKey PlayStationButton("PS_Button");
	const FKey PS_PushLeftStick("PS_PushLeftStick");
	const FKey PS_PushRightStick("PS_PushRightStick");
	const FKey PS_FunctionL("PS_FunctionL");
	const FKey PS_FunctionR("PS_FunctionR");
	const FKey PS_PaddleL("PS_PaddleL");
	const FKey PS_PaddleR("PS_PaddleR");

	EKeys::AddKey(FKeyDetails(
	    PS_FunctionL,
	    FText::FromString("PlayStation Left Function Button"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PS_FunctionR,
	    FText::FromString("PlayStation Right Function Button"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PS_PaddleL,
	    FText::FromString("PlayStation Left Paddle"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PS_PaddleR,
	    FText::FromString("PlayStation Right Paddle"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PS_PushLeftStick,
	    FText::FromString("PlayStation Left Thumbstick Button"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PS_PushRightStick,
	    FText::FromString("PlayStation Right Thumbstick Button"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    Shared,
	    FText::FromString("PlayStation Share"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    Menu,
	    FText::FromString("PlayStation Menu"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    PlayStationButton,
	    FText::FromString("PlayStation Button"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    Mic,
	    FText::FromString("PlayStation Mic"),
	    FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(
	    TouchButtom,
	    FText::FromString("PlayStation Touchpad Button"),
	    FKeyDetails::GamepadKey));
}

IMPLEMENT_MODULE(FWindowsDualsense_ds5wModule, WindowsDualsense_ds5w)
