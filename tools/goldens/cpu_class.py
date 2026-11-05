"""The CPU class that decides the onnxruntime kernels of the ensemble model.

onnxruntime 1.31.1 picks its MLAS kernels once, in MlasPlatform::MlasPlatform()
(onnxruntime/core/mlas/lib/platform.cpp at tag v1.31.1), from CPUID and XGETBV.
The model runs MatMulInteger, DynamicQuantizeLSTM and DynamicQuantizeLinear,
which reach the U8S8 and U8U8 GEMM kernels, the logistic and tanh kernels and
the quantize kernels. The class names each check of that function that
changes one of them, in the order of the source:

  avx          line 501: CPUID.1:ECX bits 27 and 28 (AVX, OSXSAVE), and line 512: XCR0 & 0x6
  avx2         line 546: CPUID.1:ECX bit 12 (FMA3) and CPUID.7.0:EBX bit 5 (AVX2)
  avxvnni      line 608: CPUID.7.1:EAX bit 4
  avx512f      line 623: CPUID.7.0:EBX bit 16, and XCR0 & 0xE0
  avx512core   line 650: CPUID.7.0:EBX bits 17, 30 and 31 (DQ, BW, VL)
  avx512vnni   line 665: CPUID.7.0:ECX bit 11
  avxvnniint8  line 681: CPUID.7.1:EDX bit 4
  amx          line 704: CPUID.7.0:EDX bits 24 and 25, XCR0 bits 17 and 18, and the
               ARCH_REQ_XCOMP_PERM request of MlasInitAMX (line 214)

avx2 and the checks after it are nested in the avx2 check, and avx512core and
avx512vnni in the avx512f check. The F16C (line 585), hybrid-core (line 593) and
AVX-NE-CONVERT (line 694) checks change only float16 kernels and the thread
count, which this model does not reach. tests/cpu_class.hpp is the same
function in C++.
"""
import ctypes
import mmap
import platform

# void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t out[4]) in the System V AMD64 ABI.
_CPUID = bytes([0x53, 0x89, 0xF8, 0x89, 0xF1, 0x49, 0x89, 0xD0, 0x0F, 0xA2, 0x41, 0x89, 0x00, 0x41, 0x89, 0x58, 0x04,
                0x41, 0x89, 0x48, 0x08, 0x41, 0x89, 0x50, 0x0C, 0x5B, 0xC3])
# uint64_t xgetbv(uint32_t index).
_XGETBV = bytes([0x89, 0xF9, 0x0F, 0x01, 0xD0, 0x48, 0xC1, 0xE2, 0x20, 0x48, 0x09, 0xD0, 0xC3])
SYS_ARCH_PRCTL = 158
ARCH_GET_XCOMP_PERM = 0x1022
ARCH_REQ_XCOMP_PERM = 0x1023
XFEATURE_XTILEDATA = 18
XFEATURE_MASK_XTILE = (1 << 17) | (1 << 18)


def _functions():
    page = mmap.mmap(-1, mmap.PAGESIZE, prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC)
    page.write(_CPUID + _XGETBV)
    base = ctypes.addressof(ctypes.c_char.from_buffer(page))
    cpuid = ctypes.CFUNCTYPE(None, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32 * 4))(base)
    xgetbv = ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_uint32)(base + len(_CPUID))
    return page, cpuid, xgetbv


def _amx_permitted():
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.syscall(SYS_ARCH_PRCTL, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA):
        return False
    mask = ctypes.c_ulong(0)
    if libc.syscall(SYS_ARCH_PRCTL, ARCH_GET_XCOMP_PERM, ctypes.byref(mask)):
        return False
    return bool(mask.value & XFEATURE_MASK_XTILE)


def cpu_class():
    if platform.machine() not in ("x86_64", "AMD64"):
        return platform.machine()
    page, cpuid, xgetbv = _functions()

    def leaf(n, sub=0):
        out = (ctypes.c_uint32 * 4)()
        cpuid(n, sub, ctypes.byref(out))
        return list(out)

    try:
        eax1, ebx1, ecx1, edx1 = leaf(1)
        tiers = []
        if ecx1 & 0x18000000 != 0x18000000:
            return "sse"
        xcr0 = xgetbv(0)
        if xcr0 & 0x6 != 0x6:
            return "sse"
        tiers.append("avx")
        c7 = leaf(7, 0)
        if ecx1 & 0x1000 and c7[1] & 0x20:
            tiers.append("avx2")
            c71 = leaf(7, 1)
            if c71[0] & 0x10:
                tiers.append("avxvnni")
            if c7[1] & 0x10000 and xcr0 & 0xE0 == 0xE0:
                tiers.append("avx512f")
                if c7[1] & 0xC0020000 == 0xC0020000:
                    tiers.append("avx512core")
                    if c7[2] & 0x800:
                        tiers.append("avx512vnni")
            if c71[3] & 0x10:
                tiers.append("avxvnniint8")
            if c7[3] & (1 << 24) and c7[3] & (1 << 25) and xcr0 & XFEATURE_MASK_XTILE == XFEATURE_MASK_XTILE \
                    and _amx_permitted():
                tiers.append("amx")
        return "+".join(tiers)
    finally:
        del cpuid, xgetbv
        page.close()


if __name__ == "__main__":
    print(cpu_class())
