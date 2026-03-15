"""
PlatformIO extra_script (pre): exclude CayenneLPPPolyline.cpp from the build.

CayenneLPP/src/CayenneLPPPolyline.cpp calls std::max(size_t, unsigned long)
which is ambiguous on 32-bit ARM (Raspberry Pi) where size_t == unsigned int.
MeshCore does not use the CayenneLPPPolyline type, so the translation unit is
safe to drop.
"""
Import("env")  # noqa: F821 — SCons injects this


def skip_polyline(node):
    """Return None to exclude this node from compilation."""
    return None


env.AddBuildMiddleware(skip_polyline, "*/CayenneLPPPolyline.cpp")
