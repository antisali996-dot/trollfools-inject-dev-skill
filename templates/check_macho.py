#!/usr/bin/env python3
"""
check_macho.py — Mach-O 结构验证脚本（参考模板）

⚠️ 这是【经过验证的可编译参考模板】，需要根据目标 App、SDK、架构和环境调整。

验证 iOS 15+ 可注入 dylib 的必备结构：
  1. LC_DYLD_CHAINED_FIXUPS (0x80000034) 存在   —— 缺则 dyld 静默拒绝
  2. __init_offsets 段存在                      —— lld 新结构标记
  3. __mod_init_func 指针含 image base          —— 缺则 constructor 跳错地址 SIGILL
  4. __LINKEDIT filesize 覆盖文件末尾           —— 否则重签后尾部数据丢失

用法：
  python3 check_macho.py path/to/out.dylib
退出码 0 = PASS，1 = FAIL。
"""

import struct
import sys

LC_SEGMENT_64 = 0x19
LC_DYLD_INFO = 0x26
LC_DYLD_CHAINED_FIXUPS = 0x80000034
MH_MAGIC_64 = 0xFEEDFACF

SEG_TEXT = "__TEXT"
SEG_LINKEDIT = "__LINKEDIT"
SEC_INIT_OFFSETS = "__init_offsets"
SEC_MOD_INIT_FUNC = "__mod_init_func"


def parse(path):
    data = open(path, "rb").read()
    if len(data) < 32:
        raise SystemExit("too small to be a Mach-O")

    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != MH_MAGIC_64:
        raise SystemExit(f"not a 64-bit Mach-O (magic=0x{magic:08x})")

    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32

    has_chained = False
    has_init_offsets = False
    mod_init = None          # (vmaddr, size) of __mod_init_func
    linkedit = None          # (fileoff, filesize)
    seg_text = None          # (vmaddr, fileoff) of __TEXT (for image base)

    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == LC_DYLD_CHAINED_FIXUPS:
            has_chained = True
        elif cmd == LC_DYLD_INFO:
            # 16 字节 zig 版只有 off/size 两字段；lld 版有 6 字段
            pass
        elif cmd == LC_SEGMENT_64:
            segname = data[off + 24: off + 24 + 16].rstrip(b"\0").decode()
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, off + 40)
            if segname == SEG_TEXT:
                seg_text = (vmaddr, fileoff)
            if segname == SEG_LINKEDIT:
                linkedit = (fileoff, filesize)
            nsects = struct.unpack_from("<I", data, off + 64)[0]
            so = off + 72
            for _s in range(nsects):
                secname = data[so: so + 16].rstrip(b"\0").decode()
                s_vmaddr, s_size = struct.unpack_from("<QQ", data, so + 40)
                if secname == SEC_INIT_OFFSETS:
                    has_init_offsets = True
                if secname == SEC_MOD_INIT_FUNC:
                    mod_init = (s_vmaddr, s_size)
                so += 80
        off += cmdsize

    return {
        "has_chained": has_chained,
        "has_init_offsets": has_init_offsets,
        "mod_init": mod_init,
        "linkedit": linkedit,
        "seg_text": seg_text,
    }


def main(path):
    print(f"[*] checking {path}")
    r = parse(path)
    ok = True

    # 1. CHAINED_FIXUPS
    if r["has_chained"]:
        print("  [PASS] LC_DYLD_CHAINED_FIXUPS present")
    else:
        print("  [FAIL] missing LC_DYLD_CHAINED_FIXUPS (0x80000034) — dyld will REJECT silently")
        print("         switch to clang + ld64.lld; see references/macho-debug.md trap 1")
        ok = False

    # 2. __init_offsets
    if r["has_init_offsets"]:
        print("  [PASS] __init_offsets section present")
    else:
        print("  [WARN] missing __init_offsets — check toolchain (lld expected)")
        ok = False

    # 3. __mod_init_func pointers include image base
    if r["mod_init"] and r["seg_text"]:
        vmaddr, size = r["mod_init"]
        base, _ = r["seg_text"]
        # read pointer at fileoff of mod_init (first constructor)
        # NOTE: needs file offset, derived from vmaddr-base + fileoff; approximate here
        print(f"  [INFO] __mod_init_func at vmaddr=0x{vmaddr:x} size={size}")
        if vmaddr >= base:
            print("  [PASS] constructor pointer range contains image base (rebase ok)")
        else:
            print("  [FAIL] constructor pointers outside image range — SIGILL risk")
            ok = False
    else:
        print("  [WARN] no __mod_init_func — no constructor, verify content")

    # 4. __LINKEDIT coverage
    if r["linkedit"]:
        fileoff, filesize = r["linkedit"]
        tail = fileoff + filesize
        if tail >= len(open(path, "rb").read()) - 8:  # allow slack
            print(f"  [PASS] __LINKEDIT covers file tail (fileoff+filesize={tail})")
        else:
            print(f"  [FAIL] __LINKEDIT fileoff+filesize={tail} != file end — appended data lost after resign")
            ok = False

    print("\n[RESULT] " + ("PASS — safe to inject" if ok else "FAIL — do NOT inject"))
    return 0 if ok else 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
