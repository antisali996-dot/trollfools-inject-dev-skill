/*
 * hook.c — TrollFools 注入插件参考模板
 *
 * ⚠️ 这是【经过验证的可编译参考模板】，需要根据目标 App、SDK、架构和环境调整：
 *   - 目标 App 的类名、方法名、bundle id 必然不同
 *   - p_xxx 函数指针需对应实际 dlsym 的符号
 *   - 必须基于实际逆向结果调整，不可直接复制使用
 *
 * 结构（必须遵守，否则崩溃）：
 *   1. constructor 只做 dlsym 符号解析 + 注册启动完成通知观察者
 *   2. 全部 Hook 在 UIApplicationDidFinishLaunching 通知回调内执行
 *      （constructor 早期直接调 objc runtime API 会 SIGILL）
 *   3. Hook 点逐一用 class_getClassMethod/class_getInstanceMethod 确认存在
 *   4. 回调内 UIKit 操作必须 dispatch_async 到主线程
 *      （Darwin/NSNotificationCenter 回调不保证在主线程）
 *
 * 诊断信号（v2.1.0 语义，三层严格区分，详见 references/ui-diagnostics.md）：
 *   - PLUGIN LOADED    ：constructor 执行 = dylib 已加载
 *   - HOOK INSTALLED   ：Class/Method 找到且 IMP 已替换（注册成功）
 *   - HOOK MISS        ：目标方法不存在/安装失败（版本差异，不崩溃）
 *   - HOOK HIT #N      ：目标 selector 真正执行（必须在 Hook 函数内部输出）
 *
 *   铁律：HOOK INSTALLED ≠ HOOK HIT。
 *   HIT 只允许在 Hook 函数 body 内输出，禁止在注册代码里打印 HIT。
 *
 * 可选诊断开关（不影响 Hook 核心行为）：
 *   -DENABLE_HIT_DIAGNOSTICS：开启 HIT 计数 + 可见 UI 状态更新（Debug/diagnostic 构建）
 *   默认关闭：Release 构建不包含诊断 UI，仅保留 stderr LOG。
 *
 * 编译：见 build.sh（LLVM clang + ld64.lld，-fno-stack-protector，-undefined dynamic_lookup）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <objc/objc.h>
#include <dispatch/dispatch.h>

/* ========== 0. 目标占位（替换为实际值） ========== */
#define PLUGIN_TAG         "MyPlugin"
#define TARGET_BUNDLE_ID   "com.example.target"   /* 目标 App bundle id */
#define TARGET_CLASS_NAME  "ExampleAdManager"      /* 目标类名 */
#define TARGET_SEL_ON      "showSplashAds"         /* 目标方法 1 */
#define TARGET_SEL_OFF     "isAdsWithAdCode:"      /* 目标方法 2 */
#define SWIZZLE_CLASS_SEL  "sharedInstance"        /* 需要 swizzle 的类方法 */
#define TARGET_ALERT_MSG   "Injected OK"

/* ========== 1. 符号指针（constructor 里 dlsym 解析） ========== */
static Class (*p_objc_getClass)(const char *name);
static id   (*p_objc_msgSend)(id self, SEL _cmd, ...);
static SEL  (*p_sel_registerName)(const char *str);
static Method (*p_class_getClassMethod)(Class cls, SEL name);
static Method (*p_class_getInstanceMethod)(Class cls, SEL name);
static IMP  (*p_method_setImplementation)(Method method, IMP imp);
static id   (*p_objc_getClass_shared)(void);

/* ========== 2. 调试日志（stderr，方便排查） ========== */
#define LOG(fmt, ...) fprintf(stderr, "[" PLUGIN_TAG "] " fmt "\n", ##__VA_ARGS__)

/* ========== 3. Hook 辅助 ========== */
/* 安装 Hook：成功 → HOOK INSTALLED；失败（方法不存在）→ HOOK MISS（版本差异，不崩溃） */
static void hook_method(Class cls, const char *selName, IMP newImp, bool isClassMethod)
{
    if (!cls) {
        LOG("HOOK MISS: class is NULL for %s", selName);
        return;
    }
    SEL sel = p_sel_registerName(selName);
    Method m = isClassMethod
        ? p_class_getClassMethod(cls, sel)
        : p_class_getInstanceMethod(cls, sel);
    if (m) {
        p_method_setImplementation(m, newImp);
        LOG("HOOK INSTALLED: %s %s", isClassMethod ? "+" : "-", selName);
    } else {
        LOG("HOOK MISS (method not found): %s %s", isClassMethod ? "+" : "-", selName);
    }
}

/* ========== 4. 替换实现（stub，HOOK HIT 必须在这里输出） ========== */
/* 拦截「自动弹出」：置空。HIT 计数 + 诊断仅在开启时记录 */
static int hookHitCount = 0;

static void stub_noop(id self, SEL _cmd, ...)
{
#ifdef ENABLE_HIT_DIAGNOSTICS
    hookHitCount++;
    LOG("HOOK HIT #%d: -[%s %s] blocked", hookHitCount,
        class_getName(object_getClass(self)), sel_getName(_cmd));
#else
    LOG("HOOK HIT: -[%s %s] blocked",
        class_getName(object_getClass(self)), sel_getName(_cmd));
#endif
}

/* 拦截「总开关」：返回 NO */
static BOOL stub_false(id self, SEL _cmd, ...)
{
#ifdef ENABLE_HIT_DIAGNOSTICS
    hookHitCount++;
    LOG("HOOK HIT #%d: -[%s %s] -> NO", hookHitCount,
        class_getName(object_getClass(self)), sel_getName(_cmd));
#else
    LOG("HOOK HIT: -[%s %s] -> NO",
        class_getName(object_getClass(self)), sel_getName(_cmd));
#endif
    return NO;
}

/* ========== 5. 可见验证弹窗（必须主线程调用） ========== */
/* UIAlertView + NSTimer 2 秒自动 dismiss（App UI 就绪后调用） */
static void show_alert(void)
{
    id cls = p_objc_getClass("UIAlertView");
    if (!cls) { LOG("UIAlertView not available"); return; }

    SEL initSel  = p_sel_registerName("initWithTitle:message:delegate:cancelButtonTitle:otherButtonTitles:");
    SEL showSel  = p_sel_registerName("show");
    SEL dismissSel = p_sel_registerName("dismissWithClickedButtonIndex:animated:");

    id alert = p_objc_msgSend(cls, initSel,
                              @"Injected", /* title */
                              TARGET_ALERT_MSG, /* message */
                              nil, nil, nil);
    if (alert) {
        p_objc_msgSend(alert, showSel);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC),
                       dispatch_get_main_queue(), ^{
            p_objc_msgSend(alert, dismissSel, 0, YES);
        });
    }
}

/* 可选：把信号持久化（无日志环境 fallback，见 ui-diagnostics.md §7）。
 * 注意：不假设任何绝对路径；沙盒 Documents 用户可能无法访问，
 * 优先使用 NSUserDefaults 或用户可读的机制。默认关闭。 */
#ifdef ENABLE_HIT_DIAGNOSTICS
static void persist_diagnostic(const char *key, const char *value)
{
    /* 示意：通过 objc runtime 调 NSUserDefaults setObject:forKey: */
    /* 实现时用 dlsym 解析 + p_objc_msgSend，勿在 constructor 阶段调用 */
}
#endif

/* ========== 6. 启动完成回调（全部 Hook 在这里执行，UIKit 操作切主线程） ========== */
static void on_did_finish_launching(CFNotificationCenterRef center, void *observer,
                                    CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    /* Darwin/NSNotificationCenter 回调不保证主线程——UIKit 操作必须切主线程 */
    dispatch_async(dispatch_get_main_queue(), ^{
        LOG("app did finish launching, begin hooking");

        Class cls = p_objc_getClass(TARGET_CLASS_NAME);
        if (!cls) {
            LOG("target class %s not found", TARGET_CLASS_NAME);
            return;
        }

        /* 6.1 逐点确认 + Hook（方法不存在会打 HOOK MISS，不崩溃） */
        hook_method(cls, TARGET_SEL_ON,  (IMP)stub_noop,  false);
        hook_method(cls, TARGET_SEL_OFF, (IMP)stub_false, true);

        /* 6.2 可见验证弹窗（主线程） */
        show_alert();
    });
}

/* ========== 7. 入口 ========== */
__attribute__((constructor))
static void init(void)
{
    LOG("PLUGIN LOADED, resolving symbols...");

    /* 7.1 解析所有 objc/Foundation 符号（全部运行时获取） */
    *(void **)&p_objc_getClass       = dlsym(RTLD_DEFAULT, "objc_getClass");
    *(void **)&p_objc_msgSend        = dlsym(RTLD_DEFAULT, "objc_msgSend");
    *(void **)&p_sel_registerName    = dlsym(RTLD_DEFAULT, "sel_registerName");
    *(void **)&p_class_getClassMethod    = dlsym(RTLD_DEFAULT, "class_getClassMethod");
    *(void **)&p_class_getInstanceMethod = dlsym(RTLD_DEFAULT, "class_getInstanceMethod");
    *(void **)&p_method_setImplementation = dlsym(RTLD_DEFAULT, "method_setImplementation");

    /* 7.2 注册启动完成通知观察者（不在 constructor 里执行任何 Hook） */
    /* 可用 NSNotificationCenter 或 CFNotificationCenterAddObserver */
    /* 注意：观察者回调不保证主线程，回调内 UIKit 必须 dispatch_async 主线程 */
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                    NULL, on_did_finish_launching,
                                    CFSTR("UIApplicationDidFinishLaunchingNotification"),
                                    NULL, CFNotificationSuspensionBehaviorDeliverImmediately);

    LOG("constructor done, waiting for app launch");
}
