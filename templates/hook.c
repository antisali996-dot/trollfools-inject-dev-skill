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
 *   4. 所有 hook 成功/失败打印 LOG（stderr），方便排查
 *
 * 编译：见 build.sh（LLVM clang + ld64.lld，-fno-stack-protector，-undefined dynamic_lookup）
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <objc/objc.h>

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
/* 记录 MISS：方法不存在 = 版本差异，需重新逆向确认，不崩溃 */
static void hook_method(Class cls, const char *selName, IMP newImp, bool isClassMethod)
{
    if (!cls) {
        LOG("hook: cls is NULL for %s", selName);
        return;
    }
    SEL sel = p_sel_registerName(selName);
    Method m = isClassMethod
        ? p_class_getClassMethod(cls, sel)
        : p_class_getInstanceMethod(cls, sel);
    if (m) {
        p_method_setImplementation(m, newImp);
        LOG("hook OK: %s %s", isClassMethod ? "+" : "-", selName);
    } else {
        LOG("hook MISS (method not found): %s %s", isClassMethod ? "+" : "-", selName);
    }
}

/* ========== 4. 替换实现（stub） ========== */
/* 拦截「自动弹出」：置空 */
static void stub_noop(id self, SEL _cmd, ...)
{
    LOG("blocked: %s", sel_getName(_cmd));
}

/* 拦截「总开关」：返回 NO */
static BOOL stub_false(id self, SEL _cmd, ...)
{
    LOG("blocked: %s -> NO", sel_getName(_cmd));
    return NO;
}

/* ========== 5. 可见验证弹窗 ========== */
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

/* ========== 6. 启动完成回调（全部 Hook 在这里执行） ========== */
static void on_did_finish_launching(CFNotificationCenterRef center, void *observer,
                                    CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    LOG("app did finish launching, begin hooking");

    Class cls = p_objc_getClass(TARGET_CLASS_NAME);
    if (!cls) {
        LOG("target class %s not found", TARGET_CLASS_NAME);
        return;
    }

    /* 6.1 逐点确认 + Hook（方法不存在会打 MISS，不崩溃） */
    hook_method(cls, TARGET_SEL_ON,  (IMP)stub_noop,  false);
    hook_method(cls, TARGET_SEL_OFF, (IMP)stub_false, true);

    /* 6.2 可见验证弹窗 */
    show_alert();
}

/* ========== 7. 入口 ========== */
__attribute__((constructor))
static void init(void)
{
    LOG("plugin loaded, resolving symbols...");

    /* 7.1 解析所有 objc/Foundation 符号（全部运行时获取） */
    *(void **)&p_objc_getClass       = dlsym(RTLD_DEFAULT, "objc_getClass");
    *(void **)&p_objc_msgSend        = dlsym(RTLD_DEFAULT, "objc_msgSend");
    *(void **)&p_sel_registerName    = dlsym(RTLD_DEFAULT, "sel_registerName");
    *(void **)&p_class_getClassMethod    = dlsym(RTLD_DEFAULT, "class_getClassMethod");
    *(void **)&p_class_getInstanceMethod = dlsym(RTLD_DEFAULT, "class_getInstanceMethod");
    *(void **)&p_method_setImplementation = dlsym(RTLD_DEFAULT, "method_setImplementation");

    /* 7.2 注册启动完成通知观察者（不在 constructor 里执行任何 Hook） */
    /* 可用 NSNotificationCenter 或 CFNotificationCenterAddObserver */
    /* 本例用 CFNotificationCenter（构造期安全） */
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(),
                                    NULL, on_did_finish_launching,
                                    CFSTR("UIApplicationDidFinishLaunchingNotification"),
                                    NULL, CFNotificationSuspensionBehaviorDeliverImmediately);

    LOG("constructor done, waiting for app launch");
}
