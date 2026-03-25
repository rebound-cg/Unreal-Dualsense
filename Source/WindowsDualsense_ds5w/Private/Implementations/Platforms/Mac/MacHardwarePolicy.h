// Copyright (c) 2025 Rafael Valoto. All Rights Reserved.
// macOS platform implementation by Brandon Bahn.
// Project: GamepadCore
// Description: macOS hardware policy adapter using hidapi (IOKit backend).
#pragma once
#include "GCore/Templates/TGenericHardwareInfo.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include "Implementations/Platforms/Mac/MacDeviceInfo.h"

namespace FMacPlatform
{
	struct FMacHardwarePolicy;
	using FMacHardware = GamepadCore::TGenericHardwareInfo<FMacHardwarePolicy>;

	struct FMacHardwarePolicy
	{
		FMacHardwarePolicy() = default;

		void Read(FDeviceContext* Context)
		{
			FMacDeviceInfo::Read(Context);
		}

		void Write(FDeviceContext* Context)
		{
			FMacDeviceInfo::Write(Context);
		}

		void Detect(std::vector<FDeviceContext>& Devices)
		{
			FMacDeviceInfo::Detect(Devices);
		}

		bool CreateHandle(FDeviceContext* Context)
		{
			return FMacDeviceInfo::CreateHandle(Context);
		}

		void InvalidateHandle(FDeviceContext* Context)
		{
			FMacDeviceInfo::InvalidateHandle(Context);
		}

		void ProcessAudioHaptic(FDeviceContext* Context)
		{
			FMacDeviceInfo::ProcessAudioHaptic(Context);
		}

		void InitializeAudioDevice(FDeviceContext* Context)
		{
			FMacDeviceInfo::InitializeAudioDevice(Context);
		}
	};
} // namespace FMacPlatform