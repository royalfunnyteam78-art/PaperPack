// Diagnostic plugin written directly against the OpenFX C API (no support library).
// It only copies the input to the output. If Filmora loads this one but not the others,
// the problem is in the support library or in the parameters.

#include <cstring>
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxProperty.h"

static OfxHost *gHost = 0;
static OfxPropertySuiteV1 *gProp = 0;
static OfxImageEffectSuiteV1 *gEff = 0;

static OfxStatus doLoad() {
    if (!gHost) return kOfxStatErrMissingHostFeature;
    gProp = (OfxPropertySuiteV1 *)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    gEff = (OfxImageEffectSuiteV1 *)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    if (!gProp || !gEff) return kOfxStatErrMissingHostFeature;
    return kOfxStatOK;
}

static OfxStatus doDescribe(OfxImageEffectHandle eff) {
    OfxPropertySetHandle p;
    gEff->getPropertySet(eff, &p);
    gProp->propSetString(p, kOfxPropLabel, 0, "PP Raw Minimal");
    gProp->propSetString(p, kOfxImageEffectPluginPropGrouping, 0, "Paper Pack Test");
    gProp->propSetString(p, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
    gProp->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthFloat);
    gProp->propSetString(p, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderFullySafe);
    return kOfxStatOK;
}

static OfxStatus doDescribeInContext(OfxImageEffectHandle eff) {
    OfxPropertySetHandle p;
    gEff->clipDefine(eff, kOfxImageEffectSimpleSourceClipName, &p);
    gProp->propSetString(p, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gEff->clipDefine(eff, kOfxImageEffectOutputClipName, &p);
    gProp->propSetString(p, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    return kOfxStatOK;
}

static OfxStatus doRender(OfxImageEffectHandle eff, OfxPropertySetHandle inArgs) {
    double time = 0;
    int win[4] = {0, 0, 0, 0};
    gProp->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gProp->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, win);

    OfxImageClipHandle srcClip = 0, dstClip = 0;
    gEff->clipGetHandle(eff, kOfxImageEffectSimpleSourceClipName, &srcClip, 0);
    gEff->clipGetHandle(eff, kOfxImageEffectOutputClipName, &dstClip, 0);

    OfxPropertySetHandle sImg = 0, dImg = 0;
    if (gEff->clipGetImage(srcClip, time, 0, &sImg) != kOfxStatOK) return kOfxStatFailed;
    if (gEff->clipGetImage(dstClip, time, 0, &dImg) != kOfxStatOK) {
        gEff->clipReleaseImage(sImg);
        return kOfxStatFailed;
    }

    void *sp = 0, *dp = 0;
    int sRow = 0, dRow = 0;
    int sb[4] = {0, 0, 0, 0}, db[4] = {0, 0, 0, 0};
    char *depth = 0;
    gProp->propGetPointer(sImg, kOfxImagePropData, 0, &sp);
    gProp->propGetInt(sImg, kOfxImagePropRowBytes, 0, &sRow);
    gProp->propGetIntN(sImg, kOfxImagePropBounds, 4, sb);
    gProp->propGetPointer(dImg, kOfxImagePropData, 0, &dp);
    gProp->propGetInt(dImg, kOfxImagePropRowBytes, 0, &dRow);
    gProp->propGetIntN(dImg, kOfxImagePropBounds, 4, db);
    gProp->propGetString(dImg, kOfxImageEffectPropPixelDepth, 0, &depth);

    const int bytesPerPixel = (depth && std::strcmp(depth, kOfxBitDepthFloat) == 0) ? 16 : 4;
    if (sp && dp) {
        for (int y = win[1]; y < win[3]; ++y) {
            const char *s = (const char *)sp + (long)(y - sb[1]) * sRow + (long)(win[0] - sb[0]) * bytesPerPixel;
            char *d = (char *)dp + (long)(y - db[1]) * dRow + (long)(win[0] - db[0]) * bytesPerPixel;
            std::memcpy(d, s, (size_t)(win[2] - win[0]) * bytesPerPixel);
        }
    }
    gEff->clipReleaseImage(sImg);
    gEff->clipReleaseImage(dImg);
    return kOfxStatOK;
}

static OfxStatus mainEntry(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle) {
    OfxImageEffectHandle eff = (OfxImageEffectHandle)handle;
    if (std::strcmp(action, kOfxActionLoad) == 0) return doLoad();
    if (std::strcmp(action, kOfxActionDescribe) == 0) return doDescribe(eff);
    if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) return doDescribeInContext(eff);
    if (std::strcmp(action, kOfxActionCreateInstance) == 0) return kOfxStatOK;
    if (std::strcmp(action, kOfxActionDestroyInstance) == 0) return kOfxStatOK;
    if (std::strcmp(action, kOfxImageEffectActionRender) == 0) return doRender(eff, inArgs);
    return kOfxStatReplyDefault;
}

static void setHostFunc(OfxHost *h) { gHost = h; }

static OfxPlugin gPlugin = {kOfxImageEffectPluginApi, 1, "com.paperpack.RawMin", 1, 0, setHostFunc, mainEntry};

OfxExport int OfxGetNumberOfPlugins(void) { return 1; }
OfxExport OfxPlugin *OfxGetPlugin(int nth) { return nth == 0 ? &gPlugin : 0; }
