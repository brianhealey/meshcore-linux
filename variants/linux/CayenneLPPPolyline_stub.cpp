// Stub implementation of CayenneLPPPolyline for the Linux target.
//
// CayenneLPP/src/CayenneLPPPolyline.cpp calls std::max(size_t, unsigned long)
// which is ambiguous on 32-bit ARM (Raspberry Pi) where size_t == unsigned int.
// MeshCore never calls addPolyline() or decodes polyline data, so these stubs
// satisfy the linker without pulling in any of the problematic template code.

#include <CayenneLPPPolyline.h>

CayenneLPPPolyline::CayenneLPPPolyline(uint32_t /*size*/) {}

std::vector<uint8_t> CayenneLPPPolyline::encode(
        const std::vector<Point>& /*coords*/,
        uint8_t /*factor*/,
        Simplification /*simplification*/) {
    return {};
}

std::vector<uint8_t> CayenneLPPPolyline::encode(
        const std::vector<Point>& /*coords*/,
        Precision /*precision*/,
        Simplification /*simplification*/) {
    return {};
}

std::vector<std::pair<double, double>>
CayenneLPPPolyline::decode(const std::vector<uint8_t>& /*buffer*/) {
    return {};
}

CayenneLPPPolyline::Stats CayenneLPPPolyline::getEncodeStats() const {
    return {};
}
