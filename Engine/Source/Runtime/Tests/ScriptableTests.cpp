#include <gtest/gtest.h>

#include "Containers/Name.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Class.h"
#include "Core/Object/ManagedInstance.h"
#include "Core/Object/ObjectBase.h"
#include "Core/Object/ObjectCore.h"
#include "Scripting/ScriptableObject.h"
#include "Scripting/ScriptableTest.h"

using namespace Lumina;

// Verifies the native half without the managed host, so OnTest runs the C++ default.
TEST(Scriptable, MintInstantiateAndNativeDefaultDispatch)
{
    CClass* Base = CScriptableTest::StaticClass();
    ASSERT_NE(Base, nullptr);

    CScriptClass* Sub = FScriptableRegistry::Mint("ScriptableTest_GTestSub", "CScriptableTest");
    ASSERT_NE(Sub, nullptr) << "minting failed (is the CScriptableTest shim registered?)";
    ProcessNewlyLoadedCObjects(); // finalize registration + CDO so FindObject/NewObject work

    EXPECT_TRUE(Sub->IsChildOf(Base));
    EXPECT_EQ(FindObject<CClass>(FName("ScriptableTest_GTestSub")), Sub);

    CObject* Obj = NewObject(Sub, nullptr, NAME_None, FGuid::New(), OF_Transient);
    ASSERT_NE(Obj, nullptr);
    CScriptableTest* Inst = Cast<CScriptableTest>(Obj);
    ASSERT_NE(Inst, nullptr) << "minted instance is not a CScriptableTest (wrong shim vtable / base)";

    // No managed instance bound (host not running in the test) -> the shim falls through to the C++ default.
    EXPECT_EQ(Inst->OnTest(5), 10);

    // A non-null sentinel proves the object arg passes through to the return.
    CWorld* const Sentinel = reinterpret_cast<CWorld*>(0x1234);
    EXPECT_EQ(Inst->OnEchoWorld(Sentinel), Sentinel);
    EXPECT_EQ(Inst->OnEchoWorld(nullptr), nullptr);
}

// The overrides live on the minted CClass as real functions, and the managed instance in the object's slot.

TEST(Scriptable, MintBuildsFunctionsForTheDeclaredOverrides)
{
    const FString Overridden[] = { FString("OnTest") };

    CScriptClass* Sub = FScriptableRegistry::Mint("ScriptableTest_GTestMask", "CScriptableTest", Overridden);
    ASSERT_NE(Sub, nullptr);
    ProcessNewlyLoadedCObjects();
    Sub->GetDefaultObject();

    const FFunction* Overridden0 = FindScriptOverride(Sub, FName("OnTest"));
    ASSERT_NE(Overridden0, nullptr) << "an event the C# subclass overrides was not minted as a function";
    EXPECT_TRUE(Overridden0->IsScriptImplemented());

    const FFunction* Base = CScriptableTest::StaticClass()->FindFunction(FName("OnTest"));
    ASSERT_NE(Base, nullptr);
    EXPECT_EQ(Overridden0->GetParams().size(), Base->GetParams().size())
        << "an override shares the base's parameters, so the frame the shim fills is the same one";
    EXPECT_EQ(Overridden0->GetParmsSize(), Base->GetParmsSize());

    EXPECT_EQ(FindScriptOverride(Sub, FName("OnEchoWorld")), nullptr)
        << "an event the subclass did not override must not be minted";

    EXPECT_EQ(Cast<CScriptClass>(CScriptableTest::StaticClass()), nullptr)
        << "a native class must not be script-defined at all, so its shim never looks for a managed instance";
    EXPECT_EQ(FindScriptOverride(CScriptableTest::StaticClass(), FName("OnTest")), nullptr);
}

// A declared override that is later removed must stop dispatching, since minted classes are reused by name.
TEST(Scriptable, ReapplyingOverridesDropsTheOnesNoLongerDeclared)
{
    const FString Both[] = { FString("OnTest"), FString("OnEchoWorld") };

    CScriptClass* Sub = FScriptableRegistry::Mint("ScriptableTest_GTestReapply", "CScriptableTest", Both);
    ASSERT_NE(Sub, nullptr);
    ProcessNewlyLoadedCObjects();
    Sub->GetDefaultObject();

    ASSERT_NE(FindScriptOverride(Sub, FName("OnTest")), nullptr);
    ASSERT_NE(FindScriptOverride(Sub, FName("OnEchoWorld")), nullptr);

    const FString Fewer[] = { FString("OnTest") };
    FScriptableRegistry::ApplyScriptOverrides(Sub, Fewer);

    EXPECT_NE(FindScriptOverride(Sub, FName("OnTest")), nullptr);
    EXPECT_EQ(FindScriptOverride(Sub, FName("OnEchoWorld")), nullptr)
        << "an override removed from the C# type must not survive the reload that reuses the class";
}

// With the override declared but no managed instance, the shim must fall through to the C++ default.
TEST(Scriptable, DispatchFallsBackToNativeWhenNoManagedInstanceExists)
{
    const FString Overridden[] = { FString("OnTest") };

    CScriptClass* Sub = FScriptableRegistry::Mint("ScriptableTest_GTestDispatch", "CScriptableTest", Overridden);
    ASSERT_NE(Sub, nullptr);
    ProcessNewlyLoadedCObjects();
    Sub->GetDefaultObject();
    ASSERT_NE(FindScriptOverride(Sub, FName("OnTest")), nullptr);

    CObject* Object = NewObject(Sub, nullptr, NAME_None, FGuid::New(), OF_Transient);
    ASSERT_NE(Object, nullptr);
    CScriptableTest* Typed = Cast<CScriptableTest>(Object);
    ASSERT_NE(Typed, nullptr);

    const int32 LiveBefore = ManagedInstances::GetLiveCount();

    EXPECT_EQ(Typed->OnTest(5), 10) << "override declared + no managed instance must fall back to the C++ default";
    EXPECT_EQ(Typed->OnTest(7), 14) << "the fallback must be stable across repeated dispatches";

    EXPECT_EQ(ManagedInstances::GetLiveCount(), LiveBefore)
        << "a failed instance creation must not occupy a slot (it would leak one per object)";

    Object->ForceDestroyNow();
    EXPECT_EQ(ManagedInstances::GetLiveCount(), LiveBefore);
}

// The class default object must never acquire a managed counterpart.
TEST(Scriptable, DefaultObjectNeverGetsAManagedInstance)
{
    const FString Overridden[] = { FString("OnTest") };

    CScriptClass* Sub = FScriptableRegistry::Mint("ScriptableTest_GTestCdo", "CScriptableTest", Overridden);
    ASSERT_NE(Sub, nullptr);
    ProcessNewlyLoadedCObjects();

    CObject* Cdo = Sub->GetDefaultObject();
    ASSERT_NE(Cdo, nullptr);
    ASSERT_TRUE(Cdo->HasAnyFlag(OF_DefaultObject));

    const int32 LiveBefore = ManagedInstances::GetLiveCount();

    EXPECT_EQ(Scriptable::GetOrCreateInstance(Cdo), nullptr) << "the CDO must never bind a managed instance";
    EXPECT_EQ(ManagedInstances::Find(Cdo), nullptr);
    EXPECT_EQ(ManagedInstances::GetLiveCount(), LiveBefore);
}
