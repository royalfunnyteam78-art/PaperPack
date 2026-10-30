// Paper Pack - OpenFX glue. Builds the OpenFX parameters from the effect table in
// effects.hpp and converts frames to/from the host. All ten effects live in one plugin file.

#include <memory>
#include <string>
#include <vector>

#include "ofxsImageEffect.h"
#include "effects.hpp"

#define kPluginGrouping "Paper Pack"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 1

namespace {

template <class T>
void loadImage(OFX::Image *img, const OfxRectI &b, pp::Img &out, float maxv) {
    const int w = b.x2 - b.x1, h = b.y2 - b.y1;
    out = pp::Img(w, h);
    for (int y = 0; y < h; ++y) {
        const T *row = (const T *)img->getPixelAddress(b.x1, b.y2 - 1 - y);
        if (!row) continue;
        float *o = out.at(0, y);
        for (int x = 0; x < w * 4; ++x) o[x] = (float)row[x] / maxv;
    }
}

inline void putVal(unsigned char *d, float v) {
    v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    *d = (unsigned char)(v * 255.f + 0.5f);
}
inline void putVal(float *d, float v) { *d = v; }

template <class T>
void storeImage(OFX::Image *img, const OfxRectI &b, const OfxRectI &win, const pp::Img &in) {
    for (int oy = win.y1; oy < win.y2; ++oy) {
        T *row = (T *)img->getPixelAddress(win.x1, oy);
        if (!row) continue;
        const int cy = (b.y2 - 1) - oy;
        if (cy < 0 || cy >= in.h) continue;
        for (int ox = win.x1; ox < win.x2; ++ox, row += 4) {
            const int cx = ox - b.x1;
            if (cx < 0 || cx >= in.w) continue;
            const float *p = in.at(cx, cy);
            for (int c = 0; c < 4; ++c) putVal(row + c, p[c]);
        }
    }
}

}  // namespace

class GenericEffect : public OFX::ImageEffect {
public:
    GenericEffect(OfxImageEffectHandle handle, const pp::EffectDef *def)
        : OFX::ImageEffect(handle), def_(def), dstClip_(0), srcClip_(0) {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        for (size_t i = 0; i < def_->params.size(); ++i) {
            const pp::ParamSpec &p = def_->params[i];
            switch (p.type) {
                case pp::P_DBL: params_.push_back(fetchDoubleParam(p.id)); break;
                case pp::P_CHOICE: params_.push_back(fetchChoiceParam(p.id)); break;
                case pp::P_BOOL: params_.push_back(fetchBooleanParam(p.id)); break;
                case pp::P_RGB: params_.push_back(fetchRGBParam(p.id)); break;
                case pp::P_PT: params_.push_back(fetchDouble2DParam(p.id)); break;
            }
        }
    }

    virtual void render(const OFX::RenderArguments &args) {
        std::unique_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
        std::unique_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
        if (!dst.get() || !src.get()) OFX::throwSuiteStatusException(kOfxStatFailed);
        if (dst->getPixelComponents() != OFX::ePixelComponentRGBA ||
            src->getPixelComponents() != OFX::ePixelComponentRGBA) {
            OFX::throwSuiteStatusException(kOfxStatErrFormat);
        }
        const OfxRectI db = dst->getBounds();
        const OfxRectI sb = src->getBounds();
        if ((db.x2 - db.x1) != (sb.x2 - sb.x1) || (db.y2 - db.y1) != (sb.y2 - sb.y1)) {
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }

        pp::Img in, out(db.x2 - db.x1, db.y2 - db.y1);
        const OFX::BitDepthEnum depth = dst->getPixelDepth();
        if (depth == OFX::eBitDepthUByte) loadImage<unsigned char>(src.get(), sb, in, 255.f);
        else if (depth == OFX::eBitDepthFloat) loadImage<float>(src.get(), sb, in, 1.f);
        else OFX::throwSuiteStatusException(kOfxStatErrUnsupported);

        double fps = srcClip_->getFrameRate();
        if (fps <= 0.0) fps = 25.0;
        const OfxRangeD range = srcClip_->getFrameRange();
        pp::TimeInfo ti;
        ti.fps = fps;
        ti.t = std::max(0.0, (args.time - range.min) / fps);
        ti.dur = std::max(1.0, range.max - range.min + 1.0) / fps;
        ti.tRemain = std::max(1.0 / fps, (range.max - args.time + 1.0) / fps);

        pp::PV pv = readParams(args.time);
        def_->fn(in, out, pv, ti);

        if (depth == OFX::eBitDepthUByte) storeImage<unsigned char>(dst.get(), db, args.renderWindow, out);
        else storeImage<float>(dst.get(), db, args.renderWindow, out);
    }

private:
    pp::PV readParams(double t) {
        pp::PV pv;
        for (size_t i = 0; i < def_->params.size(); ++i) {
            const pp::ParamSpec &p = def_->params[i];
            std::array<double, 3> v = {0, 0, 0};
            switch (p.type) {
                case pp::P_DBL: v[0] = static_cast<OFX::DoubleParam *>(params_[i])->getValueAtTime(t); break;
                case pp::P_CHOICE: {
                    int cv = 0;
                    static_cast<OFX::ChoiceParam *>(params_[i])->getValueAtTime(t, cv);
                    v[0] = cv;
                    break;
                }
                case pp::P_BOOL: v[0] = static_cast<OFX::BooleanParam *>(params_[i])->getValueAtTime(t) ? 1.0 : 0.0; break;
                case pp::P_RGB: {
                    double r, g, b;
                    static_cast<OFX::RGBParam *>(params_[i])->getValueAtTime(t, r, g, b);
                    v[0] = r; v[1] = g; v[2] = b;
                    break;
                }
                case pp::P_PT: {
                    double x, y;
                    static_cast<OFX::Double2DParam *>(params_[i])->getValueAtTime(t, x, y);
                    v[0] = x; v[1] = 1.0 - y;  // host y points up, effects use y pointing down
                    break;
                }
            }
            pv.v[p.id] = v;
        }
        return pv;
    }

    const pp::EffectDef *def_;
    OFX::Clip *dstClip_;
    OFX::Clip *srcClip_;
    std::vector<OFX::Param *> params_;
};

static void describeGeneric(OFX::ImageEffectDescriptor &desc, const pp::EffectDef *def) {
    desc.setLabels(def->name, def->name, def->name);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(def->desc);
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedBitDepth(OFX::eBitDepthUByte);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(false);
    desc.setSupportsTiles(false);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
}

static void describeContext(OFX::ImageEffectDescriptor &desc, const pp::EffectDef *def) {
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(false);
    srcClip->setIsMask(false);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(false);

    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    for (size_t i = 0; i < def->params.size(); ++i) {
        const pp::ParamSpec &p = def->params[i];
        switch (p.type) {
            case pp::P_DBL: {
                OFX::DoubleParamDescriptor *d = desc.defineDoubleParam(p.id);
                d->setLabels(p.label, p.label, p.label);
                d->setHint(p.hint);
                d->setDefault(p.def[0]);
                d->setRange(p.mn, p.mx);
                d->setDisplayRange(p.dmn, p.dmx);
                d->setAnimates(true);
                if (page) page->addChild(*d);
                break;
            }
            case pp::P_CHOICE: {
                OFX::ChoiceParamDescriptor *c = desc.defineChoiceParam(p.id);
                c->setLabels(p.label, p.label, p.label);
                c->setHint(p.hint);
                for (size_t k = 0; k < p.choices.size(); ++k) c->appendOption(p.choices[k]);
                c->setDefault((int)p.def[0]);
                c->setAnimates(true);
                if (page) page->addChild(*c);
                break;
            }
            case pp::P_BOOL: {
                OFX::BooleanParamDescriptor *b = desc.defineBooleanParam(p.id);
                b->setLabels(p.label, p.label, p.label);
                b->setHint(p.hint);
                b->setDefault(p.def[0] > 0.5);
                b->setAnimates(true);
                if (page) page->addChild(*b);
                break;
            }
            case pp::P_RGB: {
                OFX::RGBParamDescriptor *c = desc.defineRGBParam(p.id);
                c->setLabels(p.label, p.label, p.label);
                c->setHint(p.hint);
                c->setDefault(p.def[0], p.def[1], p.def[2]);
                c->setAnimates(true);
                if (page) page->addChild(*c);
                break;
            }
            case pp::P_PT: {
                OFX::Double2DParamDescriptor *d = desc.defineDouble2DParam(p.id);
                d->setLabels(p.label, p.label, p.label);
                d->setHint(p.hint);
                d->setDoubleType(OFX::eDoubleTypeXYAbsolute);
                d->setDefaultCoordinateSystem(OFX::eCoordinatesNormalised);
                d->setDefault(p.def[0], 1.0 - p.def[1]);
                d->setAnimates(true);
                if (page) page->addChild(*d);
                break;
            }
        }
    }
}

// One factory class per effect (the OpenFX support library wants a class per plugin).
#define PP_FACTORY(NAME, INDEX)                                                                         \
    mDeclarePluginFactory(NAME##Factory, {}, {});                                                      \
    void NAME##Factory::describe(OFX::ImageEffectDescriptor &desc) {                                    \
        describeGeneric(desc, &pp::allEffects()[INDEX]);                                                \
    }                                                                                                   \
    void NAME##Factory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum) {         \
        describeContext(desc, &pp::allEffects()[INDEX]);                                                \
    }                                                                                                   \
    OFX::ImageEffect *NAME##Factory::createInstance(OfxImageEffectHandle h, OFX::ContextEnum) {         \
        return new GenericEffect(h, &pp::allEffects()[INDEX]);                                          \
    }

PP_FACTORY(Fx0, 0)
#ifndef PP_DIAG
PP_FACTORY(Fx1, 1)
PP_FACTORY(Fx2, 2)
PP_FACTORY(Fx3, 3)
PP_FACTORY(Fx4, 4)
PP_FACTORY(Fx5, 5)
PP_FACTORY(Fx6, 6)
PP_FACTORY(Fx7, 7)
PP_FACTORY(Fx8, 8)
PP_FACTORY(Fx9, 9)
#endif

namespace OFX {
namespace Plugin {
void getPluginIDs(OFX::PluginFactoryArray &ids) {
    const std::vector<pp::EffectDef> &defs = pp::allEffects();
    static Fx0Factory f0((std::string("com.paperpack.") + defs[0].id), kPluginVersionMajor, kPluginVersionMinor);
#ifndef PP_DIAG
    static Fx1Factory f1((std::string("com.paperpack.") + defs[1].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx2Factory f2((std::string("com.paperpack.") + defs[2].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx3Factory f3((std::string("com.paperpack.") + defs[3].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx4Factory f4((std::string("com.paperpack.") + defs[4].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx5Factory f5((std::string("com.paperpack.") + defs[5].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx6Factory f6((std::string("com.paperpack.") + defs[6].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx7Factory f7((std::string("com.paperpack.") + defs[7].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx8Factory f8((std::string("com.paperpack.") + defs[8].id), kPluginVersionMajor, kPluginVersionMinor);
    static Fx9Factory f9((std::string("com.paperpack.") + defs[9].id), kPluginVersionMajor, kPluginVersionMinor);
#endif
    ids.push_back(&f0);
#ifndef PP_DIAG
    ids.push_back(&f1); ids.push_back(&f2); ids.push_back(&f3); ids.push_back(&f4);
    ids.push_back(&f5); ids.push_back(&f6); ids.push_back(&f7); ids.push_back(&f8); ids.push_back(&f9);
#endif
}
}  // namespace Plugin
}  // namespace OFX
