using LuminaBuildTool.Configuration;

public class Recast : LuminaThirdPartyModuleRules
{
    public Recast(TargetInfo Target)
        : base(Target)
    {
        PublicIncludePaths.Add("Recast/Include");
        PublicIncludePaths.Add("Detour/Include");

        // 32-bit poly refs split 32 bits across salt/tile/poly, and dtNavMesh::init refuses a layout
        // leaving under 10 salt bits, which caps a mesh at 256 tiles. Public because it changes
        // sizeof(dtPolyRef) and sizeof(dtLink), so every dependent must agree with Detour's own sources.
        PublicDefinitions.Add("DT_POLYREF64=1");

        SourceDirectories.Add("Recast/Source");
        SourceDirectories.Add("Detour/Source");
        // These three each define their own file-scope prev/next/area2/left/vequal helpers, so any
        // two of them in one translation unit redefine each other, and an initializer then binds to
        // a function rather than the intended variable (C2084, C2440). Held back individually; the
        // rest of Recast and all of Detour merge.
        ExcludeFromUnity.Add("RecastMesh.cpp");
        ExcludeFromUnity.Add("RecastContour.cpp");
        ExcludeFromUnity.Add("RecastMeshDetail.cpp");
    }
}
