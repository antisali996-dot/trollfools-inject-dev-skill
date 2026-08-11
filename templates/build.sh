#!/bin/bash
# build.sh — TrollFools 注入插件构建脚本（参考模板）
#
# ⚠️ 这是【经过验证的可编译参考模板】，需要根据目标 App、SDK、架构和环境调整。
#
# 工具链结论（来自 25+ 版本迭代）：
#   ✅ LLVM clang + ld64.lld —— 生成正确 CHAINED_FIXUPS + __init_offsets
#   ❌ zig —— 产物缺 CHAINED_FIXUPS + __init_offsets，被 dyld 静默拒绝
#
# 构建后必须用 check_macho.py 验证产物结构，再注入到设备。

set -euo pipefail

# ========== 可调参数 ==========
SRC="src.c"                 # 源码（默认 src.c，可改为模板 hook.c）
OUT="out.dylib"             # 输出 dylib
ARCH="arm64"                # 架构：arm64 / arm64e / arm64;arm64e (fat)
IOS_SDK="${IOS_SDK:-$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null || echo /path/to/iphoneos.sdk)}"
THEOS_SDK="${THEOS_SDK:-/path/to/theos}"   # 无 Xcode 环境：Theos 提供头文件 + tbd

# ========== 编译 ==========
echo "[*] Compiling ${SRC} -> ${OUT} (arch=${ARCH})"

clang \
  -target arm64-apple-ios \
  -arch "${ARCH}" \
  -isysroot "${IOS_SDK}" \
  -I"${THEOS_SDK}/include" \
  -fobjc-arc \
  -fno-stack-protector \
  -fvisibility=hidden \
  -dynamiclib \
  -undefined dynamic_lookup \
  -o "${OUT}" \
  "${SRC}"

# ========== 结构自检 ==========
echo "[*] Verifying Mach-O structure..."
python3 check_macho.py "${OUT}" || {
  echo "[!] Structure check FAILED — do NOT inject this dylib."
  echo "    See references/macho-debug.md (CHAINED_FIXUPS / __init_offsets / __LINKEDIT)"
  exit 1
}

echo "[*] OK: ${OUT} ready for TrollFools injection"
