# PlatformIO pre-build script: patch third-party library sources for
# 32-bit ARM compatibility before compilation.
import os
Import("env")

libdeps = env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV")

# CayenneLPP 1.6.1: CayenneLPPPolyline.cpp uses std::max(size_t, unsigned long)
# which fails on 32-bit targets where size_t == unsigned int != unsigned long.
# Fix: cast the literal to size_t so both arguments have the same type.
polyline = os.path.join(libdeps, "CayenneLPP", "src", "CayenneLPPPolyline.cpp")
if os.path.isfile(polyline):
    with open(polyline) as f:
        content = f.read()
    patched = content.replace(
        "std::max(m_buffer.size(), 8UL)",
        "std::max(m_buffer.size(), (size_t)8)"
    )
    if patched != content:
        with open(polyline, "w") as f:
            f.write(patched)
        print("patch_libs: patched CayenneLPPPolyline.cpp for 32-bit size_t")
