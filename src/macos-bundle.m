#import <Foundation/Foundation.h>

#import "macos-bundle.h"

const char *getBundlePath(void)
{
    NSString *bp = [[NSBundle mainBundle] resourcePath];
    
    return ((const char *)[bp cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getBundleVersion(void)
{
    NSDictionary *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlever = [infodict objectForKey:@"CFBundleVersion"];

    return ((const char *)[bundlever cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getBundleName(void)
{
    NSDictionary *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlename = [infodict objectForKey:@"CFBundleName"];

    return ((const char *)[bundlename cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getCopyRight(void)
{
    NSDictionary *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundlecr = [infodict objectForKey:@"NSHumanReadableCopyright"];

    return ((const char *)[bundlecr cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getMinimumOS(void)
{
    NSDictionary *infodict = [[NSBundle mainBundle] infoDictionary];
    NSString *bundleminver = [infodict objectForKey:@"LSMinimumSystemVersion"];

    return ((const char *)[bundleminver cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getExecutablePath(void)
{
    return ((const char *)[[[NSBundle mainBundle] executablePath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getFrameworksPath(void)
{
    return ((const char *)[[[NSBundle mainBundle] privateFrameworksPath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getBundleID(void)
{
    return ((const char *)[[[NSBundle mainBundle] bundleIdentifier] cStringUsingEncoding:NSUTF8StringEncoding]);
}

const char *getHomeDir(void)
{
    return ((const char *)[NSHomeDirectory() cStringUsingEncoding:NSUTF8StringEncoding]);
}
