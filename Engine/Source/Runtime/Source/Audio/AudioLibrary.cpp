#include "RuntimePCH.h"

#include "AudioLibrary.h"

#include "Assets/AssetTypes/Audio/AudioGraph.h"
#include "Assets/AssetTypes/Audio/AudioStream.h"
#include "Assets/AssetTypes/Audio/SoundBase.h"
#include "Audio/AudioContext.h"
#include "Audio/AudioGlobals.h"
#include "Audio/AudioSettings.h"
#include "Config/Config.h"
#include "Core/Object/Cast.h"

namespace Lumina
{
    namespace
    {
        // Clamped rather than trusted, since an enum crossing from script can hold anything its width allows.
        EAudioBus SafeBus(EAudioBus Bus)
        {
            const int32 Index = Math::Clamp((int32)Bus, 0, (int32)NumAudioBuses - 1);
            return (EAudioBus)Index;
        }

        FAudioHandle PlayResolved(CSoundBase* Sound, const FAudioPlayParams& Params)
        {
            if (Sound == nullptr || !Sound->IsPlayable())
            {
                return FAudioHandle::Invalid();
            }

            // A graph played this way gets a voice handle but no instance, so it takes no parameters.
            if (CAudioGraph* Graph = Cast<CAudioGraph>(Sound))
            {
                const FAudioDeviceInfo DeviceInfo = Audio::Context().GetDeviceInfo();
                const uint32 SampleRate = DeviceInfo.SampleRate != 0 ? DeviceInfo.SampleRate : 48000;

                TSharedPtr<FAudioGraphInstance> Instance = Graph->CreateInstance(SampleRate, 2);
                return Instance ? Audio::Context().PlayAudioGraph(Instance, Params) : FAudioHandle::Invalid();
            }

            if (CAudioStream* Stream = Cast<CAudioStream>(Sound))
            {
                return Audio::Context().PlayAudio(Stream->GetAudioData(), Params);
            }

            return FAudioHandle::Invalid();
        }
    }

    FAudioHandle CAudioLibrary::PlaySound2D(CSoundBase* Sound, float Volume, float Pitch, bool bLoop)
    {
        FAudioPlayParams Params;
        Params.Volume = Volume;
        Params.Pitch = Pitch;
        Params.bLooping = bLoop;
        return PlayResolved(Sound, Params);
    }

    FAudioHandle CAudioLibrary::PlaySoundAtLocation(CSoundBase* Sound, FVector3 Location, float Volume,
        float Pitch, float MinDistance, float MaxDistance, bool bLoop)
    {
        FAudioPlayParams Params;
        Params.Volume = Volume;
        Params.Pitch = Pitch;
        Params.bLooping = bLoop;
        Params.bSpatialized = true;
        Params.Position = Location;
        Params.Attenuation.MinDistance = MinDistance;
        Params.Attenuation.MaxDistance = MaxDistance;
        return PlayResolved(Sound, Params);
    }

    FAudioHandle CAudioLibrary::PlaySoundEx(CSoundBase* Sound, FAudioPlayParams Params)
    {
        Params.Bus = SafeBus(Params.Bus);
        return PlayResolved(Sound, Params);
    }

    FAudioPlayParams CAudioLibrary::DefaultPlayParams()
    {
        return FAudioPlayParams();
    }

    void CAudioLibrary::Stop(FAudioHandle Handle, bool bAllowFadeOut, float FadeSeconds)
    {
        Audio::Context().StopSound(Handle,
            bAllowFadeOut ? EAudioStopMode::AllowFadeOut : EAudioStopMode::Immediate, FadeSeconds);
    }

    void CAudioLibrary::StopAll(bool bAllowFadeOut, float FadeSeconds)
    {
        Audio::Context().StopAllSounds(
            bAllowFadeOut ? EAudioStopMode::AllowFadeOut : EAudioStopMode::Immediate, FadeSeconds);
    }

    void CAudioLibrary::SetVolume(FAudioHandle Handle, float Volume)
    {
        Audio::Context().SetVolume(Handle, Volume);
    }

    void CAudioLibrary::SetPitch(FAudioHandle Handle, float Pitch)
    {
        Audio::Context().SetPitch(Handle, Pitch);
    }

    void CAudioLibrary::SetLooping(FAudioHandle Handle, bool bLoop)
    {
        Audio::Context().SetLooping(Handle, bLoop);
    }

    void CAudioLibrary::SetPaused(FAudioHandle Handle, bool bPaused)
    {
        Audio::Context().SetPaused(Handle, bPaused);
    }

    void CAudioLibrary::SetPan(FAudioHandle Handle, float Pan)
    {
        Audio::Context().SetPan(Handle, Pan);
    }

    void CAudioLibrary::SetBus(FAudioHandle Handle, EAudioBus Bus)
    {
        Audio::Context().SetBus(Handle, SafeBus(Bus));
    }

    void CAudioLibrary::FadeTo(FAudioHandle Handle, float Volume, float Seconds)
    {
        Audio::Context().FadeTo(Handle, Volume, Seconds);
    }

    void CAudioLibrary::SeekToFrame(FAudioHandle Handle, uint64 Frame)
    {
        Audio::Context().SeekToFrame(Handle, Frame);
    }

    EAudioVoiceState CAudioLibrary::GetVoiceState(FAudioHandle Handle)
    {
        return Audio::Context().GetVoiceState(Handle);
    }

    uint64 CAudioLibrary::GetPlaybackFrame(FAudioHandle Handle)
    {
        return Audio::Context().GetPlaybackFrame(Handle);
    }

    void CAudioLibrary::SetPosition(FAudioHandle Handle, FVector3 Position)
    {
        Audio::Context().SetPosition(Handle, Position);
    }

    void CAudioLibrary::SetVelocity(FAudioHandle Handle, FVector3 Velocity)
    {
        Audio::Context().SetVelocity(Handle, Velocity);
    }

    void CAudioLibrary::SetDirection(FAudioHandle Handle, FVector3 Direction)
    {
        Audio::Context().SetDirection(Handle, Direction);
    }

    void CAudioLibrary::SetMinMaxDistance(FAudioHandle Handle, float MinDistance, float MaxDistance)
    {
        Audio::Context().SetMinMaxDistance(Handle, MinDistance, MaxDistance);
    }

    void CAudioLibrary::SetAttenuation(FAudioHandle Handle, SAudioAttenuation Attenuation)
    {
        Audio::Context().SetAttenuation(Handle, Attenuation);
    }

    void CAudioLibrary::SetOcclusion(FAudioHandle Handle, float Amount, float LowPassFrequency,
        float VolumeAttenuation)
    {
        Audio::Context().SetOcclusion(Handle, Amount, LowPassFrequency, VolumeAttenuation);
    }

    void CAudioLibrary::SetLowPassCutoff(FAudioHandle Handle, float CutoffHz)
    {
        Audio::Context().SetLowPassCutoff(Handle, CutoffHz);
    }

    bool CAudioLibrary::SetGraphFloat(FAudioHandle Handle, const FName& Name, float Value)
    {
        return Audio::Context().SetGraphFloatParameter(Handle, Name, Value);
    }

    bool CAudioLibrary::SetGraphInt(FAudioHandle Handle, const FName& Name, int32 Value)
    {
        return Audio::Context().SetGraphIntParameter(Handle, Name, Value);
    }

    bool CAudioLibrary::SetGraphBool(FAudioHandle Handle, const FName& Name, bool Value)
    {
        return Audio::Context().SetGraphBoolParameter(Handle, Name, Value);
    }

    bool CAudioLibrary::TriggerGraph(FAudioHandle Handle, const FName& Name)
    {
        return Audio::Context().TriggerGraphParameter(Handle, Name);
    }

    float CAudioLibrary::GetGraphFloatOutput(FAudioHandle Handle, const FName& Name)
    {
        return Audio::Context().GetGraphFloatOutput(Handle, Name);
    }

    uint32 CAudioLibrary::GetGraphTriggerCount(FAudioHandle Handle, const FName& Name)
    {
        return Audio::Context().GetGraphTriggerOutputCount(Handle, Name);
    }

    int32 CAudioLibrary::GetActiveVoiceCount()
    {
        return (int32)Audio::Context().GetActiveVoiceCount();
    }

    void CAudioLibrary::SetBusVolume(EAudioBus Bus, float Volume)
    {
        Audio::Context().SetBusVolume(SafeBus(Bus), Volume);
    }

    float CAudioLibrary::GetBusVolume(EAudioBus Bus)
    {
        if (!Audio::HasDevice())
        {
            return 0.0f;
        }
        return Audio::Context().GetBusVolume(SafeBus(Bus));
    }

    void CAudioLibrary::SetBusMuted(EAudioBus Bus, bool bMuted)
    {
        Audio::Context().SetBusMuted(SafeBus(Bus), bMuted);
    }

    bool CAudioLibrary::IsBusMuted(EAudioBus Bus)
    {
        return Audio::Context().IsBusMuted(SafeBus(Bus));
    }

    void CAudioLibrary::SetBusReverbSend(EAudioBus Bus, float SendLevel)
    {
        Audio::Context().SetBusReverbSend(SafeBus(Bus), SendLevel);
    }

    void CAudioLibrary::SetReverbParams(float RoomSize, float Damping, float Width, float WetLevel)
    {
        FAudioReverbParams Params;
        Params.RoomSize = RoomSize;
        Params.Damping = Damping;
        Params.Width = Width;
        Params.WetLevel = WetLevel;
        Audio::Context().SetReverbParams(Params);
    }

    void CAudioLibrary::SetDopplerScale(float Scale)
    {
        Audio::Context().SetDopplerScale(Scale);
    }

    void CAudioLibrary::SetSuspended(bool bSuspended)
    {
        Audio::Context().SetSuspended(bSuspended);
    }

    void CAudioLibrary::SaveMixSettings()
    {
        // Collapsing this would overwrite the user's saved mix with defaults on a headless save.
        if (!Audio::HasDevice() || GConfig == nullptr)
        {
            return;
        }

        CAudioSettings* Settings = GetMutableDefault<CAudioSettings>();
        if (Settings == nullptr)
        {
            return;
        }

        for (uint32 Index = 0; Index < NumAudioBuses; ++Index)
        {
            Settings->SetBusVolume((EAudioBus)Index, Audio::Context().GetBusVolume((EAudioBus)Index));
        }

        GConfig->SaveSettings(CAudioSettings::StaticClass());
    }
}
