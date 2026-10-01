#pragma once

#include "CoreMinimal.h"
#include "Windows/WindowsApplication.h"

/**
 * Makes the mouse's horizontal wheel (MX Master thumb wheel) pan 3D level viewports sideways.
 *
 * Slate has no horizontal-wheel event, so the editor normally drops WM_MOUSEHWHEEL. This hooks
 * the raw Windows message instead and strafes the camera of the perspective viewport under the
 * cursor. Hold Shift to pan up/down. Independent of the HTTP bridge and the Logi plugin.
 */
class FMXBridgeViewportScroll : public IWindowsMessageHandler
{
public:
	void Start();
	void Stop();

	virtual bool ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult) override;

private:
	bool bRegistered = false;
};
