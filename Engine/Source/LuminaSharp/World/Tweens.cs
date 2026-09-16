using System;
using Lumina;

namespace LuminaSharp;

// Tweeners run one after another; Parallel puts the next one in the same step as the last.
public readonly struct Tween
{
    internal readonly CWorld World;
    internal readonly uint Id;

    internal Tween(CWorld World, uint Id)
    {
        this.World = World;
        this.Id = Id;
    }

    public bool IsRunning => World != null && CTweenLibrary.IsRunning(World, Id);

    public Tween MoveTo(Entity Target, FVector3 Position, float Duration)
    {
        CTweenLibrary.MoveTo(World, Id, Target, Position, Duration);
        return this;
    }

    // Takes the short way around, since it slerps.
    public Tween RotateTo(Entity Target, FQuat Rotation, float Duration)
    {
        CTweenLibrary.RotateTo(World, Id, Target, Rotation, Duration);
        return this;
    }

    public Tween ScaleTo(Entity Target, FVector3 Scale, float Duration)
    {
        CTweenLibrary.ScaleTo(World, Id, Target, Scale, Duration);
        return this;
    }

    public Tween Value(float From, float To, float Duration, Action<float> Setter)
    {
        ArgumentNullException.ThrowIfNull(Setter);

        CTweenLibrary.ValueTo(World, Id, From, To, Duration, ScriptCallback.OfRepeating(Setter));
        return this;
    }

    // Dead time, for spacing steps apart.
    public Tween Interval(float Duration)
    {
        CTweenLibrary.Interval(World, Id, Duration);
        return this;
    }

    public Tween Call(Action Callback)
    {
        ArgumentNullException.ThrowIfNull(Callback);

        CTweenLibrary.Call(World, Id, ScriptCallback.OfRepeating(Callback));
        return this;
    }

    // Fires after the last step, including after the final loop.
    public Tween OnFinished(Action Callback)
    {
        ArgumentNullException.ThrowIfNull(Callback);

        CTweenLibrary.OnFinished(World, Id, ScriptCallback.OfRepeating(Callback));
        return this;
    }

    // Transition, EaseWith and Delay all apply to the tweener that was added last.
    public Tween Trans(EEaseTransition Transition)
    {
        CTweenLibrary.Trans(World, Id, Transition);
        return this;
    }

    // Named EaseWith so it does not collide with its own argument.
    public Tween EaseWith(EEaseType Ease)
    {
        CTweenLibrary.Ease(World, Id, Ease);
        return this;
    }

    public Tween Delay(float Seconds)
    {
        CTweenLibrary.Delay(World, Id, Seconds);
        return this;
    }

    public Tween Parallel()
    {
        CTweenLibrary.Parallel(World, Id);
        return this;
    }

    // 0 repeats forever, 1 is the default single pass.
    public Tween SetLoops(int Count)
    {
        CTweenLibrary.SetLoops(World, Id, Count);
        return this;
    }

    public Tween SetSpeedScale(float Scale)
    {
        CTweenLibrary.SetSpeedScale(World, Id, Scale);
        return this;
    }

    public Tween SetPaused(bool Paused)
    {
        CTweenLibrary.SetPaused(World, Id, Paused);
        return this;
    }

    // Stops where it is; whatever it was driving keeps its current value.
    public void Kill() => CTweenLibrary.Kill(World, Id);
}

// A world's tween service, reached as World.Tweens. Game thread only.
public readonly struct Tweens
{
    internal readonly CWorld World;

    internal Tweens(CWorld World)
    {
        this.World = World;
    }

    public bool IsValid => World != null;

    public Tween Create() => new Tween(World, CTweenLibrary.Create(World, Entity.Null));

    // Killed automatically when Owner is destroyed, which is what a gameplay tween usually wants.
    public Tween CreateFor(Entity Owner) => new Tween(World, CTweenLibrary.Create(World, Owner));
}
