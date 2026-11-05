// The CPU class that decides the onnxruntime kernels of the ensemble model:
// the checks of MlasPlatform::MlasPlatform() in onnxruntime 1.31.1
// (onnxruntime/core/mlas/lib/platform.cpp) that change a kernel this model
// reaches. tools/goldens/cpu_class.py is the same function and cites the
// source lines; the dumper writes its result into the golden header.
#pragma once

#include <string>

#if defined(__x86_64__)
#include <cpuid.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace cpu_class {

#if defined(__x86_64__)
inline unsigned long long xgetbv(unsigned index) {
    unsigned eax, edx;
    __asm__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return (static_cast<unsigned long long>(edx) << 32) | eax;
}

// MlasInitAMX: the kernel must grant the AMX tile data state.
inline bool amx_permitted() {
    constexpr int arch_get_xcomp_perm = 0x1022, arch_req_xcomp_perm = 0x1023, xfeature_xtiledata = 18;
    if (syscall(SYS_arch_prctl, arch_req_xcomp_perm, xfeature_xtiledata)) return false;
    unsigned long mask = 0;
    if (syscall(SYS_arch_prctl, arch_get_xcomp_perm, &mask)) return false;
    return (mask & ((1UL << 17) | (1UL << 18))) != 0;
}
#endif

inline std::string current() {
#if defined(__x86_64__)
    unsigned c1[4], c7[4], c71[4];
    __cpuid(1, c1[0], c1[1], c1[2], c1[3]);
    if ((c1[2] & 0x18000000) != 0x18000000) return "sse";
    const unsigned long long xcr0 = xgetbv(0);
    if ((xcr0 & 0x6) != 0x6) return "sse";
    std::string out = "avx";
    __cpuid_count(7, 0, c7[0], c7[1], c7[2], c7[3]);
    if ((c1[2] & 0x1000) && (c7[1] & 0x20)) {
        out += "+avx2";
        __cpuid_count(7, 1, c71[0], c71[1], c71[2], c71[3]);
        if (c71[0] & 0x10) out += "+avxvnni";
        if ((c7[1] & 0x10000) && (xcr0 & 0xE0) == 0xE0) {
            out += "+avx512f";
            if ((c7[1] & 0xC0020000) == 0xC0020000) {
                out += "+avx512core";
                if (c7[2] & 0x800) out += "+avx512vnni";
            }
        }
        if (c71[3] & 0x10) out += "+avxvnniint8";
        constexpr unsigned long long xtile = (1ULL << 17) | (1ULL << 18);
        if ((c7[3] & (1U << 24)) && (c7[3] & (1U << 25)) && (xcr0 & xtile) == xtile && amx_permitted()) out += "+amx";
    }
    return out;
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

// Empty when the golden was dumped on this CPU class; otherwise the reason
// that its model records cannot match, and how to dump them on this CPU.
inline std::string mismatch(const std::string& golden, const std::string& golden_class) {
    const std::string here = current();
    if (golden_class == here) return "";
    return golden + " holds the model output of CPU class " + golden_class + ", and this CPU is class " + here +
           ". onnxruntime runs other kernels on this class, so the model records cannot match. Dump the model "
           "goldens on this CPU with tools/dump_model_goldens.sh <dir>, then configure with "
           "-DARBTOK_GOLDENS_DIR=<dir>.";
}

}  // namespace cpu_class
