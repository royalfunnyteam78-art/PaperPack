// Paper Pack - shared helpers (no OpenFX dependency, so everything here can be unit-tested)
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace pp {

constexpr float kPi = 3.14159265358979f;

// ---------------------------------------------------------------------------
// Image: premultiplied float RGBA, row 0 = TOP of the picture.
// ---------------------------------------------------------------------------
struct Img {
    int w = 0, h = 0;
    std::vector<float> px;
    Img() {}
    Img(int W, int H) : w(W), h(H), px((size_t)W * H * 4, 0.f) {}
    float *at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const float *at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
};

// Parameter values handed to an effect. Points are stored as x (0..1 left to
// right) and y (0..1 top to bottom).
struct PV {
    std::map<std::string, std::array<double, 3>> v;
    double d(const char *id) const {
        auto it = v.find(id);
        return it == v.end() ? 0.0 : it->second[0];
    }
    int i(const char *id) const { return (int)std::lround(d(id)); }
    bool b(const char *id) const { return d(id) > 0.5; }
    void rgb(const char *id, float c[3]) const {
        auto it = v.find(id);
        if (it == v.end()) { c[0] = c[1] = c[2] = 1.f; return; }
        c[0] = (float)it->second[0]; c[1] = (float)it->second[1]; c[2] = (float)it->second[2];
    }
    void pt(const char *id, double &x, double &y) const {
        auto it = v.find(id);
        if (it == v.end()) { x = y = 0.5; return; }
        x = it->second[0]; y = it->second[1];
    }
};

struct TimeInfo {
    double t = 0;        // seconds since the clip started
    double tRemain = 0;  // seconds until the clip ends (at least one frame)
    double dur = 1;      // clip duration in seconds
    double fps = 25;
};

// ---------------------------------------------------------------------------
// Parameter description (used to build the OpenFX parameters automatically)
// ---------------------------------------------------------------------------
enum PType { P_DBL, P_CHOICE, P_BOOL, P_RGB, P_PT };

struct ParamSpec {
    const char *id;
    const char *label;
    const char *hint;
    PType type;
    double def[3];
    double mn, mx, dmn, dmx;
    std::vector<std::string> choices;
};

inline ParamSpec Dbl(const char *id, const char *label, const char *hint, double def,
                     double mn, double mx, double dmn, double dmx) {
    ParamSpec p{id, label, hint, P_DBL, {def, 0, 0}, mn, mx, dmn, dmx, {}};
    return p;
}
inline ParamSpec Choice(const char *id, const char *label, const char *hint, int def,
                        std::vector<std::string> ch) {
    ParamSpec p{id, label, hint, P_CHOICE, {(double)def, 0, 0}, 0, 0, 0, 0, std::move(ch)};
    return p;
}
inline ParamSpec Bool(const char *id, const char *label, const char *hint, bool def) {
    ParamSpec p{id, label, hint, P_BOOL, {def ? 1.0 : 0.0, 0, 0}, 0, 1, 0, 1, {}};
    return p;
}
inline ParamSpec Rgb(const char *id, const char *label, const char *hint, double r, double g, double b) {
    ParamSpec p{id, label, hint, P_RGB, {r, g, b}, 0, 1, 0, 1, {}};
    return p;
}
// y is given top-to-bottom (0 = top of the picture)
inline ParamSpec Pt(const char *id, const char *label, const char *hint, double x, double y) {
    ParamSpec p{id, label, hint, P_PT, {x, y, 0}, 0, 1, 0, 1, {}};
    return p;
}

typedef void (*RenderFn)(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti);

struct EffectDef {
    const char *id;
    const char *name;
    const char *desc;
    std::vector<ParamSpec> params;
    RenderFn fn;
};

// ---------------------------------------------------------------------------
// Small math helpers
// ---------------------------------------------------------------------------
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float sat(float v) { return clampf(v, 0.f, 1.f); }
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    if (e0 == e1) return x < e0 ? 0.f : 1.f;
    float t = sat((x - e0) / (e1 - e0));
    return t * t * (3.f - 2.f * t);
}
inline float deg2rad(float d) { return d * kPi / 180.f; }

inline uint32_t hashu(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float hash2(int x, int y, int seed) {
    uint32_t h = hashu((uint32_t)x * 374761393U + (uint32_t)y * 668265263U + (uint32_t)seed * 2246822519U + 12345U);
    return (h & 0xFFFFFFU) / 16777216.f;
}
inline float vnoise(float x, float y, int seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float u = fx * fx * (3.f - 2.f * fx), v = fy * fy * (3.f - 2.f * fy);
    float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    return mixf(mixf(a, b, u), mixf(c, d, u), v);
}
inline float fbm(float x, float y, int seed, int oct = 4) {
    float s = 0.f, a = 0.5f, f = 1.f, n = 0.f;
    for (int i = 0; i < oct; ++i) {
        s += a * vnoise(x * f, y * f, seed + i * 17);
        n += a; a *= 0.5f; f *= 2.03f;
    }
    return s / n;
}
inline float noise1(float x, int seed) {
    int xi = (int)std::floor(x);
    float f = x - xi, u = f * f * (3.f - 2.f * f);
    return mixf(hash2(xi, 0, seed), hash2(xi + 1, 0, seed), u);
}

// easing
inline float easeOutBack(float t, float s = 1.70158f) {
    t = sat(t) - 1.f;
    return 1.f + (s + 1.f) * t * t * t + s * t * t;
}
inline float easeInCubic(float t) { t = sat(t); return t * t * t; }
inline float easeInOutCubic(float t) {
    t = sat(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f;
}

// ---------------------------------------------------------------------------
// Sampling and compositing (premultiplied RGBA)
// ---------------------------------------------------------------------------
// Pixel (i,j) has its centre at (i+0.5, j+0.5).
inline void sampleTex(const Img &s, float x, float y, float out[4], bool clampEdge = false) {
    float fx = x - 0.5f, fy = y - 0.5f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float tx = fx - ix, ty = fy - iy;
    float c[4][4];
    for (int k = 0; k < 4; ++k) {
        int xx = ix + (k & 1), yy = iy + (k >> 1);
        if (clampEdge) {
            xx = std::min(std::max(xx, 0), s.w - 1);
            yy = std::min(std::max(yy, 0), s.h - 1);
        } else if (xx < 0 || yy < 0 || xx >= s.w || yy >= s.h) {
            c[k][0] = c[k][1] = c[k][2] = c[k][3] = 0.f;
            continue;
        }
        const float *p = s.at(xx, yy);
        c[k][0] = p[0]; c[k][1] = p[1]; c[k][2] = p[2]; c[k][3] = p[3];
    }
    for (int ch = 0; ch < 4; ++ch) {
        float a = mixf(c[0][ch], c[1][ch], tx);
        float b = mixf(c[2][ch], c[3][ch], tx);
        out[ch] = mixf(a, b, ty);
    }
}

// dst = top over dst (both premultiplied)
inline void overPM(float *dst, const float *top) {
    float k = 1.f - top[3];
    dst[0] = top[0] + dst[0] * k;
    dst[1] = top[1] + dst[1] * k;
    dst[2] = top[2] + dst[2] * k;
    dst[3] = top[3] + dst[3] * k;
}

// straight colour c with coverage a, composited over dst
inline void overColor(float *dst, const float c[3], float a) {
    float top[4] = {c[0] * a, c[1] * a, c[2] * a, a};
    overPM(dst, top);
}

inline float luma(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

// ---------------------------------------------------------------------------
// Animation timing shared by many effects (pop in, hold, pop out)
// ---------------------------------------------------------------------------
struct Anim {
    float scale;  // 0..1 with a small overshoot while popping in
    float alpha;  // 0..1
};

inline Anim anim(const TimeInfo &ti, double delay, double tin, double tout, float overshoot = 1.7f) {
    double tt = ti.t - delay;
    float pin = tin > 0.001 ? sat((float)(tt / tin)) : (tt >= 0 ? 1.f : 0.f);
    float pout = tout > 0.001 ? sat((float)((tout - ti.tRemain) / tout)) : 0.f;
    float kin = easeOutBack(pin, overshoot);
    float kout = 1.f - easeInCubic(pout);
    Anim a;
    a.scale = kin * kout;
    a.alpha = sat(pin * 2.5f) * (1.f - pout);
    return a;
}

// ---------------------------------------------------------------------------
// 2D transform: dest = C + P + scale * R(rot) * (local - C)
// ---------------------------------------------------------------------------
struct XF {
    float cx = 0, cy = 0, px = 0, py = 0, sc = 1, cs = 1, sn = 0;
    void init(float cx_, float cy_, float px_, float py_, float scale, float rotRad) {
        cx = cx_; cy = cy_; px = px_; py = py_;
        sc = std::max(scale, 0.001f);
        cs = std::cos(rotRad); sn = std::sin(rotRad);
    }
    void inv(float dx, float dy, float &lx, float &ly) const {
        float rx = (dx - cx - px) / sc, ry = (dy - cy - py) / sc;
        lx = cx + cs * rx + sn * ry;
        ly = cy - sn * rx + cs * ry;
    }
};

// signed distance to a rounded box (negative inside)
inline float sdfBox(float x, float y, float cx, float cy, float hx, float hy, float r) {
    float qx = std::fabs(x - cx) - hx + r;
    float qy = std::fabs(y - cy) - hy + r;
    float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
}

// ---------------------------------------------------------------------------
// Polyline strokes (used by the marker and arrow effects)
// ---------------------------------------------------------------------------
struct P2 { float x, y; };

struct Path {
    std::vector<P2> p;
    std::vector<float> s;  // cumulative length
    void finish() {
        s.assign(p.size(), 0.f);
        for (size_t i = 1; i < p.size(); ++i) {
            float dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y;
            s[i] = s[i - 1] + std::sqrt(dx * dx + dy * dy);
        }
    }
    float length() const { return s.empty() ? 0.f : s.back(); }
};

inline Path trimPath(const Path &a, float frac) {
    Path out;
    if (a.p.size() < 2) return out;
    float target = sat(frac) * a.length();
    out.p.push_back(a.p[0]);
    for (size_t i = 1; i < a.p.size(); ++i) {
        if (a.s[i] <= target) {
            out.p.push_back(a.p[i]);
        } else {
            float seg = a.s[i] - a.s[i - 1];
            float t = seg > 1e-6f ? (target - a.s[i - 1]) / seg : 0.f;
            out.p.push_back({mixf(a.p[i - 1].x, a.p[i].x, t), mixf(a.p[i - 1].y, a.p[i].y, t)});
            break;
        }
    }
    if (out.p.size() < 2) out.p.push_back(out.p[0]);
    out.finish();
    return out;
}

struct StrokeStyle {
    float halfWidth = 3.f;
    bool taper = false;       // thinner at the start
    float dashPeriod = 0.f;   // 0 = solid
    float dashOn = 0.f;
    float offX = 0.f, offY = 0.f;  // used for drop shadows
};

// Draws the stroke over dst (premultiplied). Blend: 0 = normal, 1 = multiply.
inline void drawPath(Img &dst, const Path &P, const StrokeStyle &st, const float col[3], float opacity, int blend = 0) {
    if (P.p.size() < 2 || opacity <= 0.f) return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (auto &q : P.p) {
        minx = std::min(minx, q.x); maxx = std::max(maxx, q.x);
        miny = std::min(miny, q.y); maxy = std::max(maxy, q.y);
    }
    float pad = st.halfWidth + 3.f;
    int x0 = std::max(0, (int)std::floor(minx - pad + st.offX)), x1 = std::min(dst.w - 1, (int)std::ceil(maxx + pad + st.offX));
    int y0 = std::max(0, (int)std::floor(miny - pad + st.offY)), y1 = std::min(dst.h - 1, (int)std::ceil(maxy + pad + st.offY));
    const size_t n = P.p.size();
    const float taperLen = std::max(1.f, P.length() * 0.12f);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float px = x + 0.5f - st.offX, py = y + 0.5f - st.offY;
            float best = -1e9f, bestS = 0.f;
            for (size_t i = 0; i + 1 < n; ++i) {
                float ax = P.p[i].x, ay = P.p[i].y;
                float sx = P.p[i + 1].x - ax, sy = P.p[i + 1].y - ay;
                float l2 = sx * sx + sy * sy;
                float t = l2 > 1e-9f ? ((px - ax) * sx + (py - ay) * sy) / l2 : 0.f;
                t = sat(t);
                float qx = ax + sx * t - px, qy = ay + sy * t - py;
                float d = std::sqrt(qx * qx + qy * qy);
                float sAt = P.s[i] + t * std::sqrt(l2);
                float hw = st.halfWidth;
                if (st.taper) hw *= 0.55f + 0.45f * sat(sAt / taperLen);
                float val = hw - d;
                if (val > best) { best = val; bestS = sAt; }
            }
            float cov = sat(best + 0.5f);
            if (cov <= 0.f) continue;
            if (st.dashPeriod > 0.f) {
                float ph = std::fmod(bestS, st.dashPeriod);
                cov *= sat(std::min(ph, st.dashOn - ph) + 0.5f);
                if (cov <= 0.f) continue;
            }
            cov *= opacity;
            float *d = dst.at(x, y);
            if (blend == 1) {
                for (int c = 0; c < 3; ++c) d[c] *= mixf(1.f, col[c], cov);
            } else {
                overColor(d, col, cov);
            }
        }
    }
}

// filled circle or ring
inline void drawDisc(Img &dst, float cx, float cy, float r, float ringWidth, const float col[3], float opacity,
                     float offX = 0.f, float offY = 0.f) {
    if (opacity <= 0.f || r <= 0.f) return;
    float ext = r + ringWidth + 3.f;
    int x0 = std::max(0, (int)std::floor(cx + offX - ext)), x1 = std::min(dst.w - 1, (int)std::ceil(cx + offX + ext));
    int y0 = std::max(0, (int)std::floor(cy + offY - ext)), y1 = std::min(dst.h - 1, (int)std::ceil(cy + offY + ext));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            float dx = x + 0.5f - cx - offX, dy = y + 0.5f - cy - offY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float cov;
            if (ringWidth > 0.f) cov = sat(ringWidth * 0.5f - std::fabs(dist - r) + 0.5f);
            else cov = sat(r - dist + 0.5f);
            if (cov > 0.f) overColor(dst.at(x, y), col, cov * opacity);
        }
}

}  // namespace pp
