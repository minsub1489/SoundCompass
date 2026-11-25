#include "SoundVisualization/SoundOverlaySender.h"

#include "SoundVisualization/SoundCategory.h"
#include "HAL/IConsoleManager.h"
#include "IPAddress.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace SoundOverlay
{
namespace
{
TAutoConsoleVariable<int32> CVarExternalOverlay(TEXT("SoundCompass.ExternalOverlay"), 0,
    TEXT("1 = send listener-relative sound frames to the external halo overlay over UDP (development only)."));
TAutoConsoleVariable<FString> CVarExternalOverlayPorts(TEXT("SoundCompass.ExternalOverlayPorts"), TEXT("47800"),
    TEXT("Comma-separated localhost UDP ports for SoundCompass.ExternalOverlay, e.g. 47800,47801."));

const TCHAR* const Classes[] = {TEXT("Alarm"), TEXT("Dog"), TEXT("Door"), TEXT("Explosion"), TEXT("Fire"),
    TEXT("Footstep"), TEXT("GlassBreak"), TEXT("Gunshot"), TEXT("Impact"), TEXT("Item"), TEXT("Reload"),
    TEXT("Throwable"), TEXT("Vehicle"), TEXT("Voice"), TEXT("Other")};
}

FString ToOverlayClass(FName Category)
{
    const FString Name = Category.ToString();
    for (const TCHAR* Class : Classes)
    {
        if (Name.Equals(Class, ESearchCase::IgnoreCase)) { return Class; }
    }
    const FName Kind = SoundCompassCategory(Category);
    if (Kind == TEXT("Conversation")) { return TEXT("Voice"); }
    if (Kind == TEXT("Alert")) { return TEXT("Alarm"); }
    if (Kind == TEXT("Throw")) { return TEXT("Throwable"); }
    for (const TCHAR* Same : {TEXT("Gunshot"), TEXT("Explosion"), TEXT("Footstep"), TEXT("Door"), TEXT("GlassBreak")})
    {
        if (Kind == Same) { return Same; }
    }
    if (Kind == TEXT("Noise"))
    {
        const FString Lower = Name.ToLower();
        for (const TCHAR* Vehicle : {TEXT("heli"), TEXT("plane"), TEXT("aircraft"), TEXT("engine"), TEXT("car"), TEXT("traffic")})
        {
            if (Lower.Contains(Vehicle)) { return TEXT("Vehicle"); }
        }
    }
    return TEXT("Other");
}

FString BuildMessage(const TArray<FSoundCompassSourceFrame>& Frames, double TimeSeconds)
{
    FString Out;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("v"), 1);
    Writer->WriteValue(TEXT("src"), TEXT("ue"));
    Writer->WriteValue(TEXT("mode"), TEXT("ground_truth"));
    Writer->WriteValue(TEXT("t"), TimeSeconds);
    Writer->WriteArrayStart(TEXT("events"));
    for (const FSoundCompassSourceFrame& Frame : Frames)
    {
        if (!Frame.bPlaying) { continue; }
        const float Distance = Frame.DistanceMeters;
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("id"), Frame.SourceId);
        Writer->WriteValue(TEXT("cls"), ToOverlayClass(Frame.Category));
        Writer->WriteValue(TEXT("conf"), 1.0);
        Writer->WriteValue(TEXT("az"), Frame.AzimuthDegrees);
        Writer->WriteValue(TEXT("fb"), 1.0);
        Writer->WriteValue(TEXT("dist_m"), Distance);
        Writer->WriteValue(TEXT("dist_bin"), Distance < 5.0f ? TEXT("near") : (Distance < 15.0f ? TEXT("mid") : TEXT("far")));
        Writer->WriteValue(TEXT("dist_src"), TEXT("ue"));
        Writer->WriteValue(TEXT("elev_m"), Frame.ElevationMeters);
        Writer->WriteValue(TEXT("level"), FMath::Clamp(Frame.EstimatedListenerLevel, 0.0f, 1.0f));
        Writer->WriteObjectEnd();
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    return Out;
}

bool IsEnabled() { return CVarExternalOverlay.GetValueOnGameThread() != 0; }

TArray<int32> Ports()
{
    TArray<FString> Parts;
    CVarExternalOverlayPorts.GetValueOnGameThread().ParseIntoArray(Parts, TEXT(","));
    TArray<int32> Out;
    for (const FString& Part : Parts)
    {
        const int32 Port = FCString::Atoi(*Part.TrimStartAndEnd());
        if (Port > 0 && Port < 65536) { Out.Add(Port); }
    }
    return Out;
}

FSender::~FSender()
{
    if (Socket) { ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); }
}

void FSender::Send(const FString& Json, const TArray<int32>& Ports)
{
    ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Subsystem) { return; }
    if (!Socket) { Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("SoundCompassOverlay"), false); }
    if (!Socket) { return; }
    const FTCHARToUTF8 Utf8(*Json);
    const TSharedRef<FInternetAddr> Address = Subsystem->CreateInternetAddr();
    Address->SetLoopbackAddress();
    for (const int32 Port : Ports)
    {
        Address->SetPort(Port);
        int32 Sent = 0;
        Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent, *Address);
    }
}
}
