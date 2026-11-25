#pragma once

#include "CoreMinimal.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"

class FSocket;

/**
 * Sends ground-truth source frames to the external halo overlay (HaloOverlay/) as UDP JSON, message v1
 * (Docs/superpowers/specs/2026-11-25-external-sound-halo-overlay-design.md, section 6). Development and
 * comparison only: off unless SoundCompass.ExternalOverlay is 1.
 */
namespace SoundOverlay
{
/** Authored category -> one of the overlay's 15 classes. */
FString ToOverlayClass(FName Category);

/** Listener-relative frames as one message; only frames that are playing now. */
FString BuildMessage(const TArray<FSoundCompassSourceFrame>& Frames, double TimeSeconds);

bool IsEnabled();
TArray<int32> Ports();

class FSender
{
public:
    ~FSender();
    void Send(const FString& Json, const TArray<int32>& Ports);

private:
    FSocket* Socket = nullptr;
};
}
