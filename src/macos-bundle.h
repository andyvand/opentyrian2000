#ifndef __MACOS_BUNDLE__
#define __MACOS_BUNDLE__

#ifdef __cplusplus
extern "C"
{
#endif

extern const char *getBundlePath(void);
extern const char *getBundleVersion(void);
extern const char *getBundleName(void);
extern const char *getCopyRight(void);
extern const char *getMinimumOS(void);
extern const char *getExecutablePath(void);
extern const char *getFrameworksPath(void);
extern const char *getBundleID(void);
extern const char *getHomeDir(void);

#ifdef __cplusplus
}
#endif

#endif /* __MACOS_BUNDLE__ */
