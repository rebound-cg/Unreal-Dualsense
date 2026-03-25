// Copyright (c) 2025 Rafael Valoto. All Rights Reserved.
// macOS platform implementation by Brandon Bahn.
// Project: GamepadCore
// Description: macOS HID device I/O via hidapi (IOKit backend).
 
#pragma once
 
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include <vector>
 
class FMacDeviceInfo
{
public:
	static void Read(FDeviceContext* Context);
	static void Write(FDeviceContext* Context);
	static void Detect(std::vector<FDeviceContext>& Devices);
	static bool CreateHandle(FDeviceContext* Context);
	static void InvalidateHandle(FDeviceContext* Context);
	static void ProcessAudioHaptic(FDeviceContext* Context);
	static void InitializeAudioDevice(FDeviceContext* Context);
 
private:
	static void ConfigureFeatures(FDeviceContext* Context);
};
 