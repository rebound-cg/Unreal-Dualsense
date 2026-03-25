// Copyright (c) 2025 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
// Planned Release Year: 2025

#include "DeviceManager.h"
#include "API/SonyGamepadProxyHelpers.h"
#include "API/SonyGamepadSettingsProxy.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "GCore/Types/DSCoreTypes.h"
#include "GCore/Types/ECoreGamepad.h"
#include "GenericPlatform/IInputInterface.h"
#include "IInputDevice.h"
#include "InputCoreTypes.h"
#include "Misc/CoreDelegates.h"
#include "Runtime/Launch/Resources/Version.h"

using namespace DSCoreTypes;
using namespace SonyGamepadProxyHelpers;

DeviceManager::DeviceManager(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
    : MessageHandler(InMessageHandler)
{
	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &DeviceManager::OnUserLoginChangedEvent);
}

DeviceManager::~DeviceManager()
{
	FCoreDelegates::OnUserLoginChangedEvent.RemoveAll(this);
	FilterSensors.Empty();
}

void DeviceManager::Tick(float DeltaTime)
{
	FDeviceRegistry* Registry = FDeviceRegistry::Get();
	if (Registry)
	{
		Registry->DiscoverDevices(DeltaTime);
	}

	PollAccumulator += DeltaTime;
	if (PollAccumulator < PluginSettings::PollInterval)
	{
		return;
	}
	PollAccumulator = 0.0f;

	TArray<FInputDeviceId> OutInputDevices;
	OutInputDevices.Reset();
	IPlatformInputDeviceMapper::Get().GetAllConnectedInputDevices(OutInputDevices);
	for (const FInputDeviceId& Device : OutInputDevices)
	{
		if (ISonyGamepad* Gamepad = Registry->GetLibraryInstance(Device); Gamepad)
		{
			const FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(Device);
			if (const int32 ControllerId = FPlatformMisc::GetUserIndexForPlatformUser(UserId); ControllerId == -1)
			{
				continue;
			}

			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]() {
				if (ISonyGamepad* Ref = Registry->GetLibraryInstance(Device); Ref)
				{
					Ref->UpdateInput(PluginSettings::PollInterval);
				}
			});
		}
	}

	SendControllerEvents(PluginSettings::PollInterval);
}

void DeviceManager::SendControllerEvents(float DeltaTime)
{
	TArray<FInputDeviceId> OutInputDevices;
	OutInputDevices.Reset();
	IPlatformInputDeviceMapper::Get().GetAllConnectedInputDevices(OutInputDevices);
	for (const FInputDeviceId& Device : OutInputDevices)
	{
		if (ISonyGamepad* Gamepad = FDeviceRegistry::Get()->GetLibraryInstance(Device); Gamepad)
		{
			const FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(Device);
			if (const int32 ControllerId = FPlatformMisc::GetUserIndexForPlatformUser(UserId); ControllerId == -1)
			{
				continue;
			}

			FString ContextDrive = TEXT("DualSense");
			if (Gamepad->GetDeviceType() == EDSDeviceType::DualShock4)
			{
				ContextDrive = TEXT("DualShock4");
			}
			if (Gamepad->GetDeviceType() == EDSDeviceType::DualSenseEdge)
			{
				ContextDrive = TEXT("DualSenseEdge");
			}
			FInputDeviceScope InputScope(this, TEXT("DeviceManager.WindowsDualsense"), Device.GetId(), ContextDrive);
			if (FDeviceContext* Context = Gamepad->GetMutableDeviceContext())
			{
				if (FInputContext* FrameInput = Context->GetInputState())
				{
					CheckEvents(Context, *FrameInput, UserId, Device, DeltaTime);
				}
			}
		}
	}
}

void DeviceManager::SensorsImpl(FDeviceContext* Context, FInputContext& FrameInput, const FPlatformUserId UserId, const FInputDeviceId InputDeviceId, float DeltaTime) const
{
	if (Context->bEnableAccelerometerAndGyroscope)
	{
		float RawGyroX = FrameInput.Gyroscope.X;
		float RawGyroY = FrameInput.Gyroscope.Y;
		float RawGyroZ = FrameInput.Gyroscope.Z;
		float RawAcclX = FrameInput.Accelerometer.X;
		float RawAcclY = FrameInput.Accelerometer.Y;
		float RawAcclZ = FrameInput.Accelerometer.Z;

		constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
		float G_Roll = (RawGyroZ * DEG_TO_RAD);
		float G_Pitch = (RawGyroX * DEG_TO_RAD);
		float G_Yaw = -(RawGyroY * DEG_TO_RAD);
		float A_Roll = RawAcclZ;
		float A_Pitch = RawAcclX;
		float A_Yaw = -RawAcclY;

		if (!FilterSensors.Contains(InputDeviceId))
		{
			TSharedPtr<FMadgwickAhrs> NewSensor = MakeShared<FMadgwickAhrs>(PluginSettings::MadgwickBeta);
			FilterSensors.Add(InputDeviceId, NewSensor);
		}

		if (Context->bIsResetGyroscope)
		{
			FilterSensors[InputDeviceId]->Reset();
			Context->bIsResetGyroscope = false;
		}

		FilterSensors[InputDeviceId]->UpdateImu(G_Roll, G_Pitch, G_Yaw, A_Roll, A_Pitch, A_Yaw, DeltaTime);

		float qw, qx, qy, qz;
		FilterSensors[InputDeviceId]->GetQuaternion(qw, qx, qy, qz);
		const FQuat RawSensorQuat = FQuat(qx, qy, qz, qw);
		FRotator TiltRotator = RawSensorQuat.Rotator();
		FVector UnrealGyro(G_Roll, G_Pitch, G_Yaw);
		FVector UnrealAccel(A_Roll, A_Pitch, A_Yaw);
		FVector UnrealTilt = FVector(TiltRotator.Roll, TiltRotator.Pitch, TiltRotator.Yaw);

		MessageHandler.Get().OnMotionDetected(UnrealTilt, UnrealGyro, RawSensorQuat.GetUpVector(), UnrealAccel, UserId, InputDeviceId);
	}
}

void DeviceManager::CheckEvents(FDeviceContext* Context, FInputContext& FrameInput, const FPlatformUserId UserId, const FInputDeviceId InputDeviceId, float DeltaTime) const
{
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftStickLeft, FrameInput.bLeftAnalogLeft);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftStickRight, FrameInput.bLeftAnalogRight);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftStickDown, FrameInput.bLeftAnalogDown);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftStickUp, FrameInput.bLeftAnalogUp);

	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightStickLeft, FrameInput.bRightAnalogLeft);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightStickRight, FrameInput.bRightAnalogRight);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightStickDown, FrameInput.bRightAnalogDown);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightStickUp, FrameInput.bRightAnalogUp);

	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, UserId, InputDeviceId, FrameInput.LeftAnalog.X);
	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, UserId, InputDeviceId, FrameInput.LeftAnalog.Y);
	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::RightAnalogX, UserId, InputDeviceId, FrameInput.RightAnalog.X);
	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::RightAnalogY, UserId, InputDeviceId, FrameInput.RightAnalog.Y);

	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, UserId, InputDeviceId, FrameInput.LeftTriggerAnalog);
	MessageHandler.Get().OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, UserId, InputDeviceId, FrameInput.RightTriggerAnalog);

	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::FaceButtonBottom, FrameInput.bCross);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::FaceButtonLeft, FrameInput.bSquare);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::FaceButtonRight, FrameInput.bCircle);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::FaceButtonTop, FrameInput.bTriangle);

	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::DPadUp, FrameInput.bDpadUp);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::DPadDown, FrameInput.bDpadDown);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::DPadLeft, FrameInput.bDpadLeft);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::DPadRight, FrameInput.bDpadRight);

	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftShoulder, FrameInput.bLeftShoulder);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightShoulder, FrameInput.bRightShoulder);

	// mapped urenal native gamepad Start and Select
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::SpecialRight, FrameInput.bStart);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::SpecialLeft, FrameInput.bShare);

	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftTriggerThreshold, FrameInput.bLeftTriggerThreshold);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightTriggerThreshold, FrameInput.bRightTriggerThreshold);

	// mapped urenal native gamepad Push Stick
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::LeftThumb, FrameInput.bLeftStick);
	CheckButtonInput(Context, UserId, InputDeviceId, FGamepadKeyNames::RightThumb, FrameInput.bRightStick);

	// Custom map keys
	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_PushLeftStick"), FrameInput.bLeftStick);
	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_PushRightStick"), FrameInput.bRightStick);

	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_Mic"), FrameInput.bMute);
	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_TouchButtom"), FrameInput.bTouch);
	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_Button"), FrameInput.bPSButton);

	if (Context->DeviceType == EDSDeviceType::DualSenseEdge)
	{
		CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_FunctionL"), FrameInput.bFn1);
		CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_FunctionR"), FrameInput.bFn2);
		CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_PaddleL"), FrameInput.bPaddleLeft);
		CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_PaddleR"), FrameInput.bPaddleRight);
	}

	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_Menu"), FrameInput.bStart);
	CheckButtonInput(Context, UserId, InputDeviceId, FName("PS_Share"), FrameInput.bShare);

	SensorsImpl(Context, FrameInput, UserId, InputDeviceId, DeltaTime);
	TouchpadImpl(Context, FrameInput, UserId, InputDeviceId, DeltaTime);
}

void DeviceManager::CheckButtonInput(FDeviceContext* Context, const FPlatformUserId UserId, const FInputDeviceId InputDeviceId, const FName ButtonName, const bool IsButtonPressed) const
{
	const std::string Str(TCHAR_TO_UTF8(*ButtonName.ToString()));
	const bool PreviousState = Context->ButtonStates[Str];
	if (IsButtonPressed && !PreviousState)
	{
		MessageHandler.Get().OnControllerButtonPressed(ButtonName, UserId, InputDeviceId, false);
	}

	if (!IsButtonPressed && PreviousState)
	{
		MessageHandler.Get().OnControllerButtonReleased(ButtonName, UserId, InputDeviceId, false);
	}

	Context->ButtonStates[Str] = IsButtonPressed;
}

void DeviceManager::SetDeviceProperty(int32 ControllerId, const FInputDeviceProperty* Property)
{
	if (!Property)
	{
		return;
	}

	// Handle Request_Device_Update from WM_DEVICECHANGE to trigger immediate device scan.
	static const FName RequestDeviceUpdateName(TEXT("Request_Device_Update"));
	if (Property->Name == RequestDeviceUpdateName)
	{
		FDeviceRegistry::RequestImmediateDetection();
		return;
	}

	if (Property->Name == FInputDeviceLightColorProperty::PropertyName())
	{
		const FInputDeviceLightColorProperty* ColorProperty = static_cast<const FInputDeviceLightColorProperty*>(Property);
		SetLightColor(ControllerId, ColorProperty->Color);
	}

	if (Property->Name == FInputDeviceTriggerFeedbackProperty::PropertyName())
	{
		if (const FInputDeviceTriggerFeedbackProperty* FeedbackProperty = static_cast<const FInputDeviceTriggerFeedbackProperty*>(Property))
		{
			EInputDeviceTriggerMask HandMask = FeedbackProperty->AffectedTriggers;
			if (IGamepadTrigger* GamepadTrigger = GetTriggerInterface(ControllerId))
			{
				GamepadTrigger->SetResistance(FeedbackProperty->Position, FeedbackProperty->Strengh, static_cast<EDSGamepadHand>(HandMask));
				GetGamepad(ControllerId)->UpdateOutput();
			}
		}
	}
}

void DeviceManager::SetHapticFeedbackValues(const int32 ControllerId, const int32 Hand, const FHapticFeedbackValues& Values)
{
}

void DeviceManager::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
	if (ISonyGamepad* Gamepad = GetGamepad(ControllerId))
	{
		auto GetRumbleByte = [](const float Small, const float Large) -> std::uint8_t {
			std::uint8_t SmallNib = FMath::Clamp(Small, 0.f, 1.f) * 15;
			std::uint8_t LargeNib = FMath::Clamp(Large, 0.f, 1.f) * 15;
			return (LargeNib << 4) | (SmallNib & 0x0f);
		};

		const std::uint8_t LeftRumble = GetRumbleByte(Values.LeftSmall, Values.LeftLarge);
		const std::uint8_t RightRumble = GetRumbleByte(Values.RightSmall, Values.RightLarge);
		Gamepad->SetVibration(LeftRumble, RightRumble);
		Gamepad->UpdateOutput();
	}
}

void DeviceManager::SetLightColor(const int32 ControllerId, const FColor Color)
{
	if (ISonyGamepad* Gamepad = GetGamepad(ControllerId))
	{
		Gamepad->SetLightbar({Color.R, Color.G, Color.B, Color.A});
		Gamepad->UpdateOutput();
	}
}

bool DeviceManager::IsGamepadAttached() const
{
	return true;
}

void DeviceManager::OnUserLoginChangedEvent(bool bLoggedIn, int32 UserId, int32 UserIndex)
{
	const FPlatformUserId PlatformUserId = FPlatformUserId::CreateFromInternalId(UserId);
	if (!bLoggedIn)
	{
		TArray<FInputDeviceId> OutInputDevices;
		OutInputDevices.Reset();

		IPlatformInputDeviceMapper::Get().GetAllInputDevicesForUser(PlatformUserId, OutInputDevices);
		for (const FInputDeviceId& DeviceId : OutInputDevices)
		{
			IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Disconnected);
		}
	}
}
