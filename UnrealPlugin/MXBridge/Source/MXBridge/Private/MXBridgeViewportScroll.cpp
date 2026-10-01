#include "MXBridgeViewportScroll.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
	// World units moved per wheel notch at camera speed 1; scaled by the viewport's camera speed
	// so it feels consistent with WASD flying.
	constexpr float UnitsPerNotch = 100.0f;

	FWindowsApplication* GetWindowsApplication()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		// The editor only runs on FWindowsApplication on Win64 (the plugin's sole platform).
		return static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get());
	}

	/** The perspective level viewport under the mouse cursor, if any. */
	FLevelEditorViewportClient* FindHoveredPerspectiveViewport()
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule ? LevelEditorModule->GetFirstLevelEditor() : nullptr;
		if (!LevelEditor.IsValid())
		{
			return nullptr;
		}

		const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
		for (const TSharedPtr<SLevelViewport>& Viewport : LevelEditor->GetViewports())
		{
			if (Viewport.IsValid() && Viewport->GetCachedGeometry().IsUnderLocation(Cursor))
			{
				FLevelEditorViewportClient& Client = Viewport->GetLevelViewportClient();
				return Client.IsPerspective() ? &Client : nullptr;
			}
		}
		return nullptr;
	}
}

void FMXBridgeViewportScroll::Start()
{
	if (FWindowsApplication* App = GetWindowsApplication())
	{
		App->AddMessageHandler(*this);
		bRegistered = true;
	}
}

void FMXBridgeViewportScroll::Stop()
{
	if (bRegistered)
	{
		if (FWindowsApplication* App = GetWindowsApplication())
		{
			App->RemoveMessageHandler(*this);
		}
		bRegistered = false;
	}
}

bool FMXBridgeViewportScroll::ProcessMessage(HWND, uint32 Msg, WPARAM WParam, LPARAM, int32& OutResult)
{
	if (Msg != WM_MOUSEHWHEEL || !GEditor)
	{
		return false;
	}

	// Leave the wheel alone while a game is being played in the viewport.
	if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
	{
		return false;
	}

	FLevelEditorViewportClient* Client = FindHoveredPerspectiveViewport();
	if (!Client)
	{
		return false;
	}

	// Positive delta = wheel rotated right. Smooth-scrolling wheels send fractions of a notch.
	const float Notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / WHEEL_DELTA;
	const float Distance = Notches * UnitsPerNotch * Client->GetCameraSpeed();

	const FRotationMatrix ViewAxes(Client->GetViewRotation());
	const bool bVertical = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	const FVector Direction = ViewAxes.GetScaledAxis(bVertical ? EAxis::Z : EAxis::Y); // Z = up, Y = right

	Client->SetViewLocation(Client->GetViewLocation() + Direction * Distance);
	Client->Invalidate();

	OutResult = 0;
	return true;
}
