#include "RuntimePCH.h"

#include "ObjectReferenceProvider.h"

#include "Containers/Vector.h"

namespace Lumina
{
    namespace
    {
        TVector<IObjectReferenceProvider*>& Registered()
        {
            static TVector<IObjectReferenceProvider*> Instance;
            return Instance;
        }
    }

    void FObjectReferenceProviders::Register(IObjectReferenceProvider* Provider)
    {
        if (Provider == nullptr)
        {
            return;
        }

        TVector<IObjectReferenceProvider*>& All = Registered();
        for (IObjectReferenceProvider* Existing : All)
        {
            if (Existing == Provider)
            {
                return;
            }
        }
        All.push_back(Provider);
    }

    void FObjectReferenceProviders::Unregister(IObjectReferenceProvider* Provider)
    {
        TVector<IObjectReferenceProvider*>& All = Registered();
        for (size_t Index = 0; Index < All.size(); ++Index)
        {
            if (All[Index] == Provider)
            {
                All[Index] = All.back();
                All.pop_back();
                return;
            }
        }
    }

    int32 FObjectReferenceProviders::ForEach(TFunctionRef<void(IObjectReferenceProvider&)> Func)
    {
        int32 Visited = 0;
        for (IObjectReferenceProvider* Provider : Registered())
        {
            if (Provider != nullptr)
            {
                Func(*Provider);
                ++Visited;
            }
        }
        return Visited;
    }
}
