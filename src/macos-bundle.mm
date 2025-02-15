#import <Foundation/Foundation.h>

#import "macos-bundle.h"

extern "C" const char *getBundlePath(void)
{
    NSString *bp = [[NSBundle mainBundle] resourcePath];
    
    return ((const char *)[bp cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getBundleVersion(void)
{
    NSDictionary<NSString *, id> *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlever = [infodict objectForKey:@"CFBundleVersion"];

    return ((const char *)[bundlever cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getBundleName(void)
{
    NSDictionary<NSString *, id> *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlename = [infodict objectForKey:@"CFBundleName"];

    return ((const char *)[bundlename cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getCopyRight(void)
{
    NSDictionary<NSString *, id> *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlecr = [infodict objectForKey:@"NSHumanReadableCopyright"];

    return ((const char *)[bundlecr cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getMinimumOS(void)
{
    NSDictionary<NSString *, id> *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundleminver = [infodict objectForKey:@"LSMinimumSystemVersion"];

    return ((const char *)[bundleminver cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getExecutablePath(void)
{
    return ((const char *)[[[NSBundle mainBundle] executablePath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getFrameworksPath(void)
{
    return ((const char *)[[[NSBundle mainBundle] privateFrameworksPath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

extern "C" const char *getBundleID(void)
{
    return ((const char *)[[[NSBundle mainBundle] bundleIdentifier] cStringUsingEncoding:NSUTF8StringEncoding]);
}
