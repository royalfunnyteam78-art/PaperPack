// Paper Pack - the effects. Pure C++ (no OpenFX), see ofx_glue.cpp for the host side.
#pragma once

#include "common.hpp"

namespace pp {

// ===========================================================================
// 1. PAPER SWAY - the clip flutters like paper in the wind
// ===========================================================================
static void fxPaperSway(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const double amount = pv.d("amount"), speed = pv.d("speed"), wl = pv.d("wavelength");
    const double angle = pv.d("angle"), shading = pv.d("shading"), sway = pv.d("sway"), seed = pv.d("seed");
    const int pin = pv.i("pin");
    const float cx = w * 0.5f, cy = h * 0.5f;
    float pivX = cx, pivY = cy;
    switch (pin) {
        case 1: pivY = 0.f; break;
        case 2: pivX = 0.f; break;
        case 3: pivX = (float)w; break;
        case 4: pivY = (float)h; break;
        default: break;
    }
    const float ang = deg2rad((float)angle);
    const float ca = std::cos(ang), sa = std::sin(ang);
    const float wlPx = std::max(8.f, (float)wl / 100.f * w);
    const float k = 2.f * kPi / wlPx;
    const float omega = 2.f * kPi * (float)speed;
    const float t = (float)ti.t;
    const float gRot = deg2rad((float)sway) * std::sin(0.6f * omega * t + (float)seed);
    const float gc = std::cos(-gRot), gs = std::sin(-gRot);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float u = px / w, v = py / h;  // v = 0 at the top
            float weight = 1.f;
            switch (pin) {
                case 1: weight = v; break;
                case 2: weight = u; break;
                case 3: weight = 1.f - u; break;
                case 4: weight = 1.f - v; break;
                default: break;
            }
            const float p = (px - cx) * ca + (py - cy) * sa;
            const float ph1 = k * p - omega * t + (float)seed;
            const float ph2 = 1.9f * k * p + 1.3f * omega * t + (float)seed * 1.7f;
            const float d1 = std::sin(ph1), d2 = std::sin(ph2);
            const float A = (float)amount * weight;
            const float dx = A * (-sa * d1 + 0.35f * ca * d2);
            const float dy = A * (ca * d1 + 0.35f * sa * d2);
            const float rx = px - pivX, ry = py - pivY;
            const float gx = pivX + gc * rx - gs * ry;
            const float gy = pivY + gs * rx + gc * ry;
            float o[4];
            sampleTex(src, gx + dx, gy + dy, o);
            if (shading > 0.0) {
                float s = 1.f + (float)shading * 0.25f * (std::cos(ph1) + 0.5f * std::cos(ph2)) * weight;
                s = clampf(s, 0.2f, 1.8f);
                o[0] *= s; o[1] *= s; o[2] *= s;
            }
            float *d = dst.at(x, y);
            for (int c = 0; c < 4; ++c) d[c] = o[c];
        }
    }
}

// ===========================================================================
// 2. TORN PAPER NOTE - clip (or blank paper) on a torn sheet with a shadow
// ===========================================================================
static void fxTornNote(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    const Anim an = anim(ti, pv.d("delay"), pv.d("popin"), pv.d("popout"));
    if (an.alpha <= 0.001f) return;
    double ppx, ppy;
    pv.pt("pos", ppx, ppy);
    XF xf;
    xf.init(w * 0.5f, h * 0.5f, (float)(ppx - 0.5) * w, (float)(ppy - 0.5) * h, an.scale,
            deg2rad((float)pv.d("rotation") + (1.f - an.scale) * -10.f));
    const bool showClip = pv.i("mode") == 0;
    const float s0 = (float)pv.d("size") / 100.f;
    const float hw = w * 0.5f * s0, hh = h * 0.5f * s0;
    const float m = (float)pv.d("margin") / 100.f * minD;
    const float rough = (float)pv.d("roughness") * 0.012f * minD;
    const float grain = (float)pv.d("grain");
    const float shOp = (float)pv.d("shadow");
    float paper[3];
    pv.rgb("paper", paper);
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float shx = 0.006f * minD, shy = 0.012f * minD, blur = 0.012f * minD;
    const float rimW = 0.006f * minD + 1.f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float lx, ly;
            xf.inv(x + 0.5f, y + 0.5f, lx, ly);
            float d = sdfBox(lx, ly, cx, cy, hw + m, hh + m, 0.f);
            float n = (fbm(lx / (0.05f * minD), ly / (0.05f * minD), 11, 4) - 0.5f) * 2.f * rough;
            n += (fbm(lx / (0.008f * minD), ly / (0.008f * minD), 23, 2) - 0.5f) * 2.f * rough * 0.35f;
            const float de = d + n;
            const float inside = smoothstepf(0.75f, -0.75f, de);
            float out[4] = {0, 0, 0, 0};
            // shadow
            float ds = sdfBox(lx - shx, ly - shy, cx, cy, hw + m, hh + m, 0.f) + n * 0.5f;
            float shA = shOp * smoothstepf(blur, -blur * 0.2f, ds);
            if (shA > 0.f) { out[3] = shA * (1.f - inside); }
            if (inside > 0.f) {
                float edge = -de;
                float rim = 1.f - smoothstepf(0.f, rimW, edge);
                float col[3];
                for (int c = 0; c < 3; ++c) col[c] = mixf(paper[c], 0.99f, rim * 0.85f);
                float tex = (fbm(lx / (0.03f * minD), ly / (0.03f * minD), 5, 4) - 0.5f) * 0.07f +
                            (hash2((int)lx, (int)ly, 1) - 0.5f) * 0.06f * grain;
                for (int c = 0; c < 3; ++c) col[c] = clampf(col[c] + tex, 0.f, 1.f);
                if (showClip) {
                    float ci = smoothstepf(0.75f, -0.75f, sdfBox(lx, ly, cx, cy, hw, hh, 0.f));
                    if (ci > 0.f) {
                        float s[4];
                        sampleTex(src, cx + (lx - cx) / s0, cy + (ly - cy) / s0, s, true);
                        for (int c = 0; c < 3; ++c) {
                            float over = s[c] + col[c] * (1.f - s[3]);
                            col[c] = mixf(col[c], over, ci);
                        }
                    }
                }
                out[0] = col[0] * inside; out[1] = col[1] * inside; out[2] = col[2] * inside;
                out[3] = inside + out[3];
                out[3] = sat(out[3]);
            }
            float *dp = dst.at(x, y);
            for (int c = 0; c < 4; ++c) dp[c] = out[c] * an.alpha;
        }
    }
}

// ===========================================================================
// 3. INK EDGE PHOTO - photo with brushed ink edges on aged paper
// ===========================================================================
static void fxInkPhoto(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    const Anim an = anim(ti, pv.d("delay"), pv.d("popin"), pv.d("popout"), 1.2f);
    double ppx, ppy;
    pv.pt("pos", ppx, ppy);
    XF xf;
    xf.init(w * 0.5f, h * 0.5f, (float)(ppx - 0.5) * w, (float)(ppy - 0.5) * h, std::max(an.scale, 0.001f), 0.f);
    const bool paperBg = pv.i("bg") == 0;
    const float s0 = (float)pv.d("size") / 100.f;
    const float hw = w * 0.5f * s0, hh = h * 0.5f * s0;
    const float rough = (float)pv.d("roughness");
    const float sepia = (float)pv.d("sepia");
    const float splat = (float)pv.d("splatter");
    const float aging = (float)pv.d("aging");
    float ink[3], tint[3];
    pv.rgb("ink", ink);
    pv.rgb("paper", tint);
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float cell = std::max(2.f, 0.006f * minD);
    const float reach = 0.05f * minD;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float *dp = dst.at(x, y);
            float out[4] = {0, 0, 0, 0};
            float paperLum = 0.9f;
            if (paperBg) {
                float fx = (x + 0.5f) / minD, fy = (y + 0.5f) / minD;
                float mott = fbm(fx * 4.f, fy * 4.f, 3, 5);
                float u = (x + 0.5f) / w - 0.5f, v = (y + 0.5f) / h - 0.5f;
                float vig = (u * u + v * v) * 2.f;
                float stain = smoothstepf(0.58f, 0.8f, fbm(fx * 7.f, fy * 7.f, 9, 4)) * 0.14f * aging;
                float g = (0.92f + 0.14f * mott) * (1.f - aging * 0.45f * vig * vig) - stain;
                g += (hash2(x, y, 4) - 0.5f) * 0.05f * aging;
                for (int c = 0; c < 3; ++c) out[c] = clampf(tint[c] * g, 0.f, 1.f);
                out[3] = 1.f;
                paperLum = luma(out[0], out[1], out[2]);
            }
            float lx, ly;
            xf.inv(x + 0.5f, y + 0.5f, lx, ly);
            float d = sdfBox(lx, ly, cx, cy, hw, hh, 0.f);
            float n = (fbm(lx / (0.06f * minD), ly / (0.06f * minD), 31, 4) - 0.5f) * 2.f * rough * 0.05f * minD;
            n += (fbm(lx / (0.012f * minD), ly / (0.012f * minD), 41, 3) - 0.5f) * 2.f * rough * 0.015f * minD;
            const float de = d + n;
            const float mask = smoothstepf(1.f, -1.f, de) * an.alpha;
            if (mask > 0.f) {
                float s[4];
                sampleTex(src, cx + (lx - cx) / s0, cy + (ly - cy) / s0, s, true);
                float a = std::max(s[3], 1e-4f);
                float r = s[0] / a, g = s[1] / a, b = s[2] / a;
                float l = luma(r, g, b);
                r = mixf(r, l * 1.07f, sepia); g = mixf(g, l * 0.74f, sepia); b = mixf(b, l * 0.43f, sepia);
                float rim = (1.f - smoothstepf(0.f, 0.007f * minD, -de));
                float dark = 1.f - 0.45f * rim;
                float mul = paperBg ? (0.88f + 0.12f * paperLum) : 1.f;
                float lay[4] = {clampf(r * dark * mul, 0.f, 1.f) * a * mask, clampf(g * dark * mul, 0.f, 1.f) * a * mask,
                                clampf(b * dark * mul, 0.f, 1.f) * a * mask, a * mask};
                // ink soaked into the paper along the rim
                float inkA = rim * 0.35f * mask;
                float inkLay[4] = {ink[0] * inkA, ink[1] * inkA, ink[2] * inkA, inkA};
                overPM(lay, inkLay);  // note: ink on top of photo rim
                overPM(out, lay);
            }
            if (splat > 0.f && de > 0.f && de < reach) {
                int ix = (int)std::floor(lx / cell), iy = (int)std::floor(ly / cell);
                float hv = hash2(ix, iy, 77);
                float prob = 0.07f * splat * (1.f - de / reach);
                if (hv > 1.f - prob) {
                    float ccx = (ix + 0.5f + (hash2(ix, iy, 78) - 0.5f) * 0.5f) * cell;
                    float ccy = (iy + 0.5f + (hash2(ix, iy, 79) - 0.5f) * 0.5f) * cell;
                    float rr = cell * (0.18f + 0.3f * hash2(ix, iy, 80));
                    float dd = std::sqrt((lx - ccx) * (lx - ccx) + (ly - ccy) * (ly - ccy));
                    float a = sat(rr - dd + 0.5f) * 0.75f * an.alpha;
                    overColor(out, ink, a);
                }
            }
            for (int c = 0; c < 4; ++c) dp[c] = out[c];
        }
    }
}

// ===========================================================================
// 4. CIRCLE BUBBLE - round photo with a border that pops in
// ===========================================================================
static void fxBubble(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    const Anim an = anim(ti, pv.d("delay"), pv.d("popin"), pv.d("popout"));
    if (an.alpha <= 0.001f) return;
    double ppx, ppy;
    pv.pt("pos", ppx, ppy);
    const float cxp = (float)ppx * w, cyp = (float)ppy * h;
    const float Rn = (float)pv.d("radius") / 100.f * minD;
    const float bw = (float)pv.d("border") / 100.f * minD;
    const float zoom = std::max(0.1f, (float)pv.d("zoom"));
    const float fit = (2.f * Rn / minD) * zoom;
    const float sc = std::max(an.scale, 0.001f);
    const float shOp = (float)pv.d("shadow");
    float bcol[3];
    pv.rgb("bordercolor", bcol);
    const float shx = 0.006f * minD, shy = 0.012f * minD, blur = 0.012f * minD;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float rx = (x + 0.5f - cxp) / sc, ry = (y + 0.5f - cyp) / sc;
            float dist = std::sqrt(rx * rx + ry * ry);
            float photoCov = sat((Rn - dist) * sc + 0.5f);
            float discCov = sat((Rn + bw - dist) * sc + 0.5f);
            float out[4] = {0, 0, 0, 0};
            float sdx = rx - shx, sdy = ry - shy;
            float sd = (std::sqrt(sdx * sdx + sdy * sdy) - (Rn + bw)) * sc;
            float shA = shOp * smoothstepf(blur, -blur * 0.2f, sd);
            out[3] = shA * (1.f - discCov);
            if (discCov > 0.f) {
                float lay[4] = {bcol[0] * discCov, bcol[1] * discCov, bcol[2] * discCov, discCov};
                if (photoCov > 0.f) {
                    float s[4];
                    sampleTex(src, w * 0.5f + rx / fit, h * 0.5f + ry / fit, s, true);
                    float ph[4] = {s[0] * photoCov, s[1] * photoCov, s[2] * photoCov, s[3] * photoCov};
                    overPM(lay, ph);
                }
                overPM(out, lay);
            }
            float *dp = dst.at(x, y);
            for (int c = 0; c < 4; ++c) dp[c] = out[c] * an.alpha;
        }
    }
}

// ===========================================================================
// 5. MARKER DRAW - hand drawn underline / highlighter / box / circle
// ===========================================================================
static Path linePath(P2 a, P2 b, float amp, float freqPx, int seed, int n = 40) {
    Path P;
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
    float nx = -dy / len, ny = dx / len;
    for (int i = 0; i < n; ++i) {
        float u = i / (float)(n - 1);
        float off = (noise1(u * len / freqPx, seed) - 0.5f) * 2.f * amp + std::sin(u * kPi) * amp * 0.6f;
        P.p.push_back({a.x + dx * u + nx * off, a.y + dy * u + ny * off});
    }
    P.finish();
    return P;
}

static Path boxPath(float x0, float y0, float x1, float y1, float amp, int seed) {
    P2 c[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    for (int i = 0; i < 4; ++i) {
        c[i].x += (hash2(i, 1, seed) - 0.5f) * amp * 3.f;
        c[i].y += (hash2(i, 2, seed) - 0.5f) * amp * 3.f;
    }
    P2 seq[6] = {c[0], c[1], c[2], c[3], c[0], {mixf(c[0].x, c[1].x, 0.18f) + amp, mixf(c[0].y, c[1].y, 0.18f) + amp * 0.6f}};
    Path P;
    for (int e = 0; e < 5; ++e) {
        P2 a = seq[e], b = seq[e + 1];
        float dx = b.x - a.x, dy = b.y - a.y;
        float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
        float nx = -dy / len, ny = dx / len;
        int n = 14;
        for (int i = (e == 0 ? 0 : 1); i <= n; ++i) {
            float u = i / (float)n;
            float off = (noise1(u * len / (amp * 12.f + 20.f) + e * 3.1f, seed + e) - 0.5f) * 2.f * amp;
            P.p.push_back({a.x + dx * u + nx * off, a.y + dy * u + ny * off});
        }
    }
    P.finish();
    return P;
}

static Path ellipsePath(float cx, float cy, float rx, float ry, float amp, int seed) {
    Path P;
    const int n = 90;
    const float th0 = deg2rad(-110.f), sweep = deg2rad(385.f);
    const float base = std::max(8.f, std::min(rx, ry));
    for (int i = 0; i <= n; ++i) {
        float u = i / (float)n;
        float th = th0 + sweep * u;
        float rs = 1.f + 0.045f * u + (noise1(u * 6.f, seed) - 0.5f) * 2.f * amp / base;
        P.p.push_back({cx + rx * rs * std::cos(th), cy + ry * rs * std::sin(th)});
    }
    P.finish();
    return P;
}

static void fxMarker(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    dst = src;
    const int style = pv.i("style");
    double ax, ay, bx, by;
    pv.pt("pointa", ax, ay);
    pv.pt("pointb", bx, by);
    P2 A{(float)ax * w, (float)ay * h}, B{(float)bx * w, (float)by * h};
    float col[3];
    pv.rgb("color", col);
    const float rough = (float)pv.d("roughness");
    const float amp = rough * 0.006f * minD;
    const float opacity = (float)pv.d("opacity");
    const double delay = pv.d("delay"), drawT = std::max(0.05, pv.d("drawtime")), fade = pv.d("fadeout");
    float prog = easeInOutCubic((float)((ti.t - delay) / drawT));
    if (ti.t < delay || prog <= 0.001f) return;
    float fadeK = fade > 0.001 ? 1.f - sat((float)((fade - ti.tRemain) / fade)) : 1.f;
    const float op = opacity * fadeK;
    if (op <= 0.f) return;

    if (style == 1) {
        // highlighter band
        float dx = B.x - A.x, dy = B.y - A.y;
        float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
        float ux = dx / len, uy = dy / len;
        float hh = (float)pv.d("hlheight") / 100.f * minD * 0.5f;
        float reach = prog * len;
        int blend = pv.i("blend");
        float ext = hh + len + 6.f;
        int x0 = std::max(0, (int)std::floor(std::min(A.x, B.x) - hh - 6.f)), x1 = std::min(w - 1, (int)std::ceil(std::max(A.x, B.x) + hh + 6.f));
        int y0 = std::max(0, (int)std::floor(std::min(A.y, B.y) - hh - 6.f)), y1 = std::min(h - 1, (int)std::ceil(std::max(A.y, B.y) + hh + 6.f));
        (void)ext;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float px = x + 0.5f - A.x, py = y + 0.5f - A.y;
                float u = px * ux + py * uy;
                float v = -px * uy + py * ux;
                float us = u - v * 0.10f;  // slanted ends like a chisel marker
                float lnoise = (noise1(v / (hh * 0.8f + 1.f) + 3.f, 5) - 0.5f) * hh * 0.35f * rough;
                float rnoise = (noise1(v / (hh * 0.8f + 1.f) + 9.f, 6) - 0.5f) * hh * 0.35f * rough;
                float hv = hh * (1.f + (noise1(u / (len * 0.25f + 1.f), 7) - 0.5f) * 0.14f * rough);
                float cov = sat(us + lnoise + 0.5f) * sat(reach - us + rnoise + 0.5f) * sat(hv - std::fabs(v) + 0.5f);
                if (cov <= 0.f) continue;
                cov *= op;
                float *d = dst.at(x, y);
                if (blend == 0) {
                    for (int c = 0; c < 3; ++c) d[c] *= mixf(1.f, col[c], cov);
                } else {
                    overColor(d, col, cov * 0.75f);
                }
            }
        return;
    }

    Path full;
    if (style == 0) {
        full = linePath(A, B, amp, 0.08f * minD + 10.f, 3);
    } else if (style == 2) {
        full = boxPath(std::min(A.x, B.x), std::min(A.y, B.y), std::max(A.x, B.x), std::max(A.y, B.y), amp, 5);
    } else {
        float ccx = (A.x + B.x) * 0.5f, ccy = (A.y + B.y) * 0.5f;
        full = ellipsePath(ccx, ccy, std::max(4.f, std::fabs(B.x - A.x) * 0.5f), std::max(4.f, std::fabs(B.y - A.y) * 0.5f), amp, 8);
    }
    Path part = trimPath(full, prog);
    StrokeStyle st;
    st.halfWidth = std::max(0.8f, (float)pv.d("thickness") / 100.f * minD * 0.5f);
    st.taper = (style == 0);
    if (pv.b("shadow")) {
        StrokeStyle sh = st;
        sh.offX = 0.003f * minD; sh.offY = 0.005f * minD;
        const float black[3] = {0, 0, 0};
        drawPath(dst, part, sh, black, 0.28f * op);
    }
    drawPath(dst, part, st, col, op);
}

// ===========================================================================
// 6. ARROW & ROUTE - curved arrow or dashed travel route that draws itself
// ===========================================================================
static void fxArrow(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    dst = src;
    const int style = pv.i("style");
    double ax, ay, bx, by;
    pv.pt("pointa", ax, ay);
    pv.pt("pointb", bx, by);
    P2 A{(float)ax * w, (float)ay * h}, B{(float)bx * w, (float)by * h};
    float col[3];
    pv.rgb("color", col);
    const double delay = pv.d("delay"), drawT = std::max(0.05, pv.d("drawtime")), fade = pv.d("fadeout");
    float prog = easeInOutCubic((float)((ti.t - delay) / drawT));
    if (ti.t < delay || prog <= 0.001f) return;
    float op = fade > 0.001 ? 1.f - sat((float)((fade - ti.tRemain) / fade)) : 1.f;
    if (op <= 0.f) return;

    float dx = B.x - A.x, dy = B.y - A.y;
    float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
    float nx = -dy / len, ny = dx / len;
    float bend = (float)pv.d("curve") / 100.f * len;
    P2 C{(A.x + B.x) * 0.5f + nx * bend, (A.y + B.y) * 0.5f + ny * bend};
    Path full;
    for (int i = 0; i <= 64; ++i) {
        float t = i / 64.f, u = 1.f - t;
        full.p.push_back({u * u * A.x + 2 * u * t * C.x + t * t * B.x, u * u * A.y + 2 * u * t * C.y + t * t * B.y});
    }
    full.finish();
    Path part = trimPath(full, prog);

    StrokeStyle st;
    st.halfWidth = std::max(0.8f, (float)pv.d("thickness") / 100.f * minD * 0.5f);
    st.taper = true;
    if (style >= 1) {
        float per = std::max(4.f, (float)pv.d("dash") / 100.f * minD);
        st.dashPeriod = per;
        st.dashOn = per * 0.55f;
    }
    const bool shadow = pv.b("shadow");
    const float black[3] = {0, 0, 0};
    const float sx = 0.003f * minD, sy = 0.005f * minD;
    if (shadow) {
        StrokeStyle sh = st;
        sh.offX = sx; sh.offY = sy;
        drawPath(dst, part, sh, black, 0.3f * op);
    }
    drawPath(dst, part, st, col, op);

    const float hw = st.halfWidth;
    if (style == 2) {
        // route: dot at the start, ring + dot at the destination
        if (shadow) drawDisc(dst, A.x, A.y, hw * 1.9f, 0.f, black, 0.3f * op, sx, sy);
        drawDisc(dst, A.x, A.y, hw * 1.9f, 0.f, col, op);
        float endK = sat((prog - 0.93f) / 0.07f);
        if (endK > 0.f) {
            float rr = hw * 3.4f * easeOutBack(endK, 2.f);
            if (shadow) drawDisc(dst, B.x, B.y, rr, hw * 1.1f, black, 0.3f * op, sx, sy);
            drawDisc(dst, B.x, B.y, rr, hw * 1.1f, col, op);
            drawDisc(dst, B.x, B.y, hw * 1.3f * endK, 0.f, col, op);
        }
    } else {
        // arrow head
        float headK = sat((prog - 0.85f) / 0.15f);
        if (headK > 0.f) {
            float tx = B.x - C.x, ty = B.y - C.y;
            float tl = std::max(1e-3f, std::sqrt(tx * tx + ty * ty));
            tx /= tl; ty /= tl;
            float L = (float)pv.d("headsize") / 100.f * minD * headK;
            StrokeStyle hs;
            hs.halfWidth = hw;
            for (int side = -1; side <= 1; side += 2) {
                float a = deg2rad(28.f * side);
                float bxv = -tx * std::cos(a) + ty * std::sin(a);
                float byv = -tx * std::sin(a) - ty * std::cos(a);
                Path hp;
                hp.p = {B, {B.x + bxv * L, B.y + byv * L}};
                hp.finish();
                if (shadow) {
                    StrokeStyle sh = hs;
                    sh.offX = sx; sh.offY = sy;
                    drawPath(dst, hp, sh, black, 0.3f * op);
                }
                drawPath(dst, hp, hs, col, op);
            }
        }
    }
}

// ===========================================================================
// 7. SLIDE IN / OUT - clip enters and leaves from any side
// ===========================================================================
static void fxSlide(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const int inDir = pv.i("indir"), outDir = pv.i("outdir");
    const double delay = pv.d("delay"), tin = pv.d("induration"), tout = pv.d("outduration");
    const float dist = (float)pv.d("distance") / 100.f;
    const float bounce = (float)pv.d("bounce");
    const float tilt = (float)pv.d("tilt");
    const bool fade = pv.b("fade");
    double tt = ti.t - delay;
    float pin = inDir == 4 ? 1.f : (tin > 0.001 ? sat((float)(tt / tin)) : (tt >= 0 ? 1.f : 0.f));
    float pout = (outDir == 4 || tout <= 0.001) ? 0.f : sat((float)((tout - ti.tRemain) / tout));
    float ein = easeOutBack(pin, bounce * 2.5f);
    float eout = easeInCubic(pout);
    const float vx[4] = {-1, 1, 0, 0}, vy[4] = {0, 0, -1, 1};
    float offX = 0, offY = 0;
    if (inDir < 4) { offX += (1.f - ein) * vx[inDir] * dist * w; offY += (1.f - ein) * vy[inDir] * dist * h; }
    if (outDir < 4) { offX += eout * vx[outDir] * dist * w; offY += eout * vy[outDir] * dist * h; }
    float rot = deg2rad(tilt) * ((inDir < 4 ? (1.f - ein) : 0.f) - (outDir < 4 ? eout : 0.f));
    float alpha = fade ? sat(pin * 1.5f) * (1.f - pout) : 1.f;
    XF xf;
    xf.init(w * 0.5f, h * 0.5f, offX, offY, 1.f, rot);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float lx, ly, s[4];
            xf.inv(x + 0.5f, y + 0.5f, lx, ly);
            sampleTex(src, lx, ly, s);
            float *d = dst.at(x, y);
            for (int c = 0; c < 4; ++c) d[c] = s[c] * alpha;
        }
}

// ===========================================================================
// 8. STICKER OUTLINE - white cut-out border, glow and shadow around the subject
// ===========================================================================
static void distanceOutside(const Img &src, float thr, std::vector<float> &D) {
    const int w = src.w, h = src.h;
    D.assign((size_t)w * h, 1e9f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (src.at(x, y)[3] >= thr) D[(size_t)y * w + x] = 0.f;
    const float d1 = 1.f, d2 = 1.41421f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * w + x;
            float v = D[i];
            if (x > 0) v = std::min(v, D[i - 1] + d1);
            if (y > 0) {
                v = std::min(v, D[i - w] + d1);
                if (x > 0) v = std::min(v, D[i - w - 1] + d2);
                if (x < w - 1) v = std::min(v, D[i - w + 1] + d2);
            }
            D[i] = v;
        }
    for (int y = h - 1; y >= 0; --y)
        for (int x = w - 1; x >= 0; --x) {
            size_t i = (size_t)y * w + x;
            float v = D[i];
            if (x < w - 1) v = std::min(v, D[i + 1] + d1);
            if (y < h - 1) {
                v = std::min(v, D[i + w] + d1);
                if (x < w - 1) v = std::min(v, D[i + w + 1] + d2);
                if (x > 0) v = std::min(v, D[i + w - 1] + d2);
            }
            D[i] = v;
        }
}

static void fxSticker(const Img &src, Img &dst, const PV &pv, const TimeInfo &) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    const float r = (float)pv.d("width") / 100.f * minD;
    const float glowSize = (float)pv.d("glowsize") / 100.f * minD;
    const float glowStr = (float)pv.d("glowstrength");
    const float shOp = (float)pv.d("shadow");
    const float soft = 0.01f * minD;
    const int sdx = (int)std::lround(0.006f * minD), sdy = (int)std::lround(0.012f * minD);
    float oc[3], gc[3];
    pv.rgb("outlinecolor", oc);
    pv.rgb("glowcolor", gc);
    std::vector<float> D;
    distanceOutside(src, (float)pv.d("threshold"), D);
    const float black[3] = {0, 0, 0};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * w + x;
            float d0 = D[i];
            float out[4] = {0, 0, 0, 0};
            if (shOp > 0.f) {
                int xs = std::min(std::max(x - sdx, 0), w - 1), ys = std::min(std::max(y - sdy, 0), h - 1);
                float ds = D[(size_t)ys * w + xs];
                float a = shOp * (1.f - smoothstepf(r, r + soft + 1.f, ds));
                overColor(out, black, a);
            }
            if (glowSize > 0.5f && glowStr > 0.f && d0 > r) {
                float a = glowStr * std::exp(-(d0 - r) / std::max(1.f, glowSize * 0.45f));
                overColor(out, gc, sat(a));
            }
            float cov = sat(r + 0.5f - d0);
            if (cov > 0.f && r > 0.f) overColor(out, oc, cov);
            overPM(out, src.at(x, y));
            float *dp = dst.at(x, y);
            for (int c = 0; c < 4; ++c) dp[c] = out[c];
        }
}

// ===========================================================================
// 9. POLAROID FRAME - white photo frame with tape that drops in
// ===========================================================================
static void fxPolaroid(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    const float minD = (float)std::min(w, h);
    const Anim an = anim(ti, pv.d("delay"), pv.d("popin"), pv.d("popout"), 1.4f);
    if (an.alpha <= 0.001f) return;
    double ppx, ppy;
    pv.pt("pos", ppx, ppy);
    XF xf;
    xf.init(w * 0.5f, h * 0.5f, (float)(ppx - 0.5) * w, (float)(ppy - 0.5) * h, an.scale,
            deg2rad((float)pv.d("rotation") + (1.f - an.scale) * 14.f));
    const float s0 = (float)pv.d("size") / 100.f;
    const float hw = w * 0.5f * s0, hh = h * 0.5f * s0;
    const float b = (float)pv.d("border") / 100.f * (2.f * hw);
    const float cap = (float)pv.d("caption") / 100.f * (2.f * hh);
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float pcy = cy - cap * 0.5f;  // photo centre
    const float fhw = hw + b, fhh = hh + b + cap * 0.5f;
    float paper[3];
    pv.rgb("paper", paper);
    const bool tape = pv.b("tape");
    const float shOp = (float)pv.d("shadow");
    const float shx = 0.006f * minD, shy = 0.014f * minD, blur = 0.014f * minD;
    const float rad = 0.006f * minD;
    const float tapeRot = deg2rad(-4.f), tc = std::cos(tapeRot), ts = std::sin(tapeRot);
    const float tapeY = cy - fhh + 0.004f * minD;
    const float thw = fhw * 0.28f, thh = fhw * 0.075f;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float lx, ly;
            xf.inv(x + 0.5f, y + 0.5f, lx, ly);
            float d = sdfBox(lx, ly, cx, cy, fhw, fhh, rad);
            float inside = smoothstepf(0.75f, -0.75f, d);
            float out[4] = {0, 0, 0, 0};
            float ds = sdfBox(lx - shx, ly - shy, cx, cy, fhw, fhh, rad);
            out[3] = shOp * smoothstepf(blur, -blur * 0.2f, ds) * (1.f - inside);
            if (inside > 0.f) {
                float g = 0.97f + 0.03f * sat((ly - (cy - fhh)) / (2.f * fhh));
                float tex = (hash2(x, y, 2) - 0.5f) * 0.02f;
                float col[3];
                for (int c = 0; c < 3; ++c) col[c] = clampf(paper[c] * g + tex, 0.f, 1.f);
                float pi = smoothstepf(0.75f, -0.75f, sdfBox(lx, ly, cx, pcy, hw, hh, 0.f));
                if (pi > 0.f) {
                    float s[4];
                    sampleTex(src, w * 0.5f + (lx - cx) / s0, h * 0.5f + (ly - pcy) / s0, s, true);
                    for (int c = 0; c < 3; ++c) col[c] = mixf(col[c], s[c] + col[c] * (1.f - s[3]), pi);
                }
                out[0] = col[0] * inside; out[1] = col[1] * inside; out[2] = col[2] * inside;
                out[3] = sat(inside + out[3]);
            }
            if (tape) {
                float rx = lx - cx, ry = ly - tapeY;
                float ux = tc * rx + ts * ry, uy = -ts * rx + tc * ry;
                float tcov = smoothstepf(0.75f, -0.75f, sdfBox(ux, uy, 0.f, 0.f, thw, thh, 0.f)) * 0.62f;
                if (tcov > 0.f) {
                    const float tcol[3] = {0.93f, 0.87f, 0.68f};
                    overColor(out, tcol, tcov);
                }
            }
            float *dp = dst.at(x, y);
            for (int c = 0; c < 4; ++c) dp[c] = out[c] * an.alpha;
        }
}

// ===========================================================================
// 10. AUTO ZOOM - slow push-in / pan over the whole clip (Ken Burns for stills)
// ===========================================================================
static void fxAutoZoom(const Img &src, Img &dst, const PV &pv, const TimeInfo &ti) {
    const int w = src.w, h = src.h;
    float p = sat((float)(ti.t / std::max(0.001, ti.dur)));
    float e = pv.i("ease") == 1 ? easeInOutCubic(p) : p;
    float z0 = (float)pv.d("startzoom"), z1 = (float)pv.d("endzoom");
    float zoom = std::max(0.1f, mixf(z0, z1, e));
    int dir = pv.i("pandir");
    const float vx[5] = {0, -1, 1, 0, 0}, vy[5] = {0, 0, 0, -1, 1};
    float amt = (float)pv.d("panamount") / 100.f;
    float offX = vx[dir] * (e - 0.5f) * amt * w;
    float offY = vy[dir] * (e - 0.5f) * amt * h;
    double zx, zy;
    pv.pt("center", zx, zy);
    float Zx = (float)zx * w, Zy = (float)zy * h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float sx = Zx + (x + 0.5f - Zx - offX) / zoom;
            float sy = Zy + (y + 0.5f - Zy - offY) / zoom;
            float s[4];
            sampleTex(src, sx, sy, s, true);
            float *d = dst.at(x, y);
            for (int c = 0; c < 4; ++c) d[c] = s[c];
        }
}

// ===========================================================================
// Effect table (this is what builds the OpenFX parameters)
// ===========================================================================
static void addTiming(std::vector<ParamSpec> &v, double in, double out) {
    v.push_back(Dbl("delay", "Start Delay (s)", "Seconds after the clip starts before it appears.", 0.0, 0.0, 120.0, 0.0, 5.0));
    v.push_back(Dbl("popin", "Pop In (s)", "Length of the entrance animation.", in, 0.0, 10.0, 0.0, 2.0));
    v.push_back(Dbl("popout", "Pop Out (s)", "Length of the exit animation at the end of the clip. 0 keeps it on screen until the end.", out, 0.0, 10.0, 0.0, 2.0));
}

static void addDrawTiming(std::vector<ParamSpec> &v, double delay, double draw) {
    v.push_back(Dbl("delay", "Draw Delay (s)", "Seconds after the clip starts before drawing begins.", delay, 0.0, 120.0, 0.0, 5.0));
    v.push_back(Dbl("drawtime", "Draw Time (s)", "How long the drawing takes.", draw, 0.05, 20.0, 0.1, 3.0));
    v.push_back(Dbl("fadeout", "Fade Out (s)", "Fade away at the end of the clip. 0 = stay.", 0.0, 0.0, 10.0, 0.0, 2.0));
}

#ifdef PP_DIAG
// Diagnostic build: a single pass-through effect. PP_DIAG selects which parameter kinds it declares:
// 0 = none, 1 = + double/choice/bool, 2 = + RGB colour and 2D point.
static void fxDiag(const Img &s, Img &d, const PV &, const TimeInfo &) { d = s; }

inline const std::vector<EffectDef> &allEffects() {
    static std::vector<EffectDef> defs;
    if (!defs.empty()) return defs;
    static const char *ids[] = {"Diag0", "Diag1", "Diag2"};
    static const char *names[] = {"PP Diag 0 (no params)", "PP Diag 1 (basic params)", "PP Diag 2 (colour and point)"};
    EffectDef e{ids[PP_DIAG], names[PP_DIAG], "Pass-through test effect used to find out what the host accepts.", {}, fxDiag};
    if (PP_DIAG >= 1) {
        e.params.push_back(Dbl("amount", "Amount", "Test number.", 1.0, 0.0, 10.0, 0.0, 10.0));
        e.params.push_back(Choice("mode", "Mode", "Test choice.", 0, {"One", "Two"}));
        e.params.push_back(Bool("flag", "Flag", "Test checkbox.", true));
    }
    if (PP_DIAG >= 2) {
        e.params.push_back(Rgb("tint", "Colour", "Test colour.", 1.0, 0.5, 0.2));
        e.params.push_back(Pt("spot", "Point", "Test point.", 0.5, 0.5));
    }
    defs.push_back(e);
    return defs;
}
#else
inline const std::vector<EffectDef> &allEffects() {
    static std::vector<EffectDef> defs;
    if (!defs.empty()) return defs;

    {  // 1
        EffectDef e{"PaperSway", "Paper Sway", "Makes a clip or photo flutter like paper in the wind. Pin an edge so it hangs like a sheet.", {}, fxPaperSway};
        e.params.push_back(Choice("pin", "Pin", "Which edge is held in place.", 1,
                                  {"Free (whole image waves)", "Pinned at top", "Pinned at left", "Pinned at right", "Pinned at bottom"}));
        e.params.push_back(Dbl("amount", "Amount", "Strength of the flutter in pixels.", 14.0, 0.0, 500.0, 0.0, 60.0));
        e.params.push_back(Dbl("speed", "Speed", "Wave speed in cycles per second.", 0.6, 0.0, 10.0, 0.0, 3.0));
        e.params.push_back(Dbl("wavelength", "Wave Size", "Distance between waves, percent of width.", 45.0, 5.0, 400.0, 10.0, 150.0));
        e.params.push_back(Dbl("angle", "Wave Direction", "Direction the waves travel, in degrees.", 90.0, -360.0, 360.0, -180.0, 180.0));
        e.params.push_back(Dbl("shading", "Paper Shading", "Light and dark ripples that make it look like paper.", 0.5, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("sway", "Sway", "Slow overall swing in degrees.", 3.0, 0.0, 45.0, 0.0, 15.0));
        e.params.push_back(Dbl("seed", "Variation", "Change this to get a different motion.", 0.0, -1000.0, 1000.0, 0.0, 20.0));
        defs.push_back(e);
    }
    {  // 2
        EffectDef e{"TornPaperNote", "Torn Paper Note", "Puts the clip on a torn paper note with a shadow. Use Blank Paper to get a note you can type on with the Filmora text tool.", {}, fxTornNote};
        e.params.push_back(Choice("mode", "Content", "Show the clip on the paper, or only the blank paper.", 0, {"Clip on paper", "Blank paper (for text)"}));
        e.params.push_back(Pt("pos", "Position", "Where the note sits.", 0.5, 0.5));
        e.params.push_back(Dbl("size", "Clip Size (%)", "Size of the clip on the note.", 78.0, 10.0, 100.0, 20.0, 100.0));
        e.params.push_back(Dbl("margin", "Paper Margin (%)", "Paper border around the clip.", 5.0, 0.0, 40.0, 0.0, 15.0));
        e.params.push_back(Dbl("roughness", "Tear Roughness", "How ragged the torn edge is.", 1.0, 0.0, 4.0, 0.0, 3.0));
        e.params.push_back(Dbl("rotation", "Rotation", "Tilt in degrees.", -2.0, -45.0, 45.0, -15.0, 15.0));
        e.params.push_back(Rgb("paper", "Paper Colour", "Colour of the paper.", 0.96, 0.93, 0.85));
        e.params.push_back(Dbl("grain", "Paper Grain", "Paper fibre texture.", 0.5, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("shadow", "Shadow", "Drop shadow strength.", 0.45, 0.0, 1.0, 0.0, 1.0));
        addTiming(e.params, 0.45, 0.0);
        defs.push_back(e);
    }
    {  // 3
        EffectDef e{"InkEdgePhoto", "Ink Edge Photo", "Photo with brushed, ink-soaked edges on aged paper. Perfect for archive and history shots.", {}, fxInkPhoto};
        e.params.push_back(Choice("bg", "Background", "Aged paper page, or transparent so it sits over other clips.", 0, {"Aged paper", "Transparent"}));
        e.params.push_back(Pt("pos", "Position", "Where the photo sits.", 0.5, 0.5));
        e.params.push_back(Dbl("size", "Photo Size (%)", "Size of the photo.", 70.0, 10.0, 100.0, 20.0, 100.0));
        e.params.push_back(Dbl("roughness", "Edge Roughness", "How ragged the ink edge is.", 0.8, 0.0, 3.0, 0.0, 2.0));
        e.params.push_back(Dbl("sepia", "Sepia", "Warm old-photo tone.", 0.35, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("splatter", "Ink Splatter", "Little ink drops around the edge.", 0.6, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("aging", "Paper Aging", "Stains and dark corners on the paper.", 0.6, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Rgb("paper", "Paper Colour", "Colour of the paper.", 0.90, 0.82, 0.66));
        e.params.push_back(Rgb("ink", "Ink Colour", "Colour of the ink.", 0.12, 0.09, 0.07));
        addTiming(e.params, 0.5, 0.0);
        defs.push_back(e);
    }
    {  // 4
        EffectDef e{"CircleBubble", "Circle Bubble", "Round photo with a border that pops in. Great for faces and locations on a map.", {}, fxBubble};
        e.params.push_back(Pt("pos", "Position", "Centre of the bubble.", 0.5, 0.5));
        e.params.push_back(Dbl("radius", "Radius (%)", "Bubble size as a percent of the frame's short side.", 30.0, 2.0, 100.0, 5.0, 60.0));
        e.params.push_back(Dbl("zoom", "Clip Zoom", "Zoom of the clip inside the bubble.", 1.0, 0.2, 10.0, 0.5, 4.0));
        e.params.push_back(Dbl("border", "Border (%)", "Border thickness.", 1.4, 0.0, 10.0, 0.0, 5.0));
        e.params.push_back(Rgb("bordercolor", "Border Colour", "Colour of the border.", 1.0, 1.0, 1.0));
        e.params.push_back(Dbl("shadow", "Shadow", "Drop shadow strength.", 0.5, 0.0, 1.0, 0.0, 1.0));
        addTiming(e.params, 0.4, 0.0);
        defs.push_back(e);
    }
    {  // 5
        EffectDef e{"MarkerDraw", "Marker Draw", "Hand-drawn red underline, yellow highlighter, box or circle that draws itself over the picture. Drag Point A and Point B to place it.", {}, fxMarker};
        e.params.push_back(Choice("style", "Style", "What to draw.", 0, {"Underline", "Highlighter", "Box", "Circle"}));
        e.params.push_back(Pt("pointa", "Point A", "Underline/Highlighter: start. Box/Circle: one corner.", 0.2, 0.55));
        e.params.push_back(Pt("pointb", "Point B", "Underline/Highlighter: end. Box/Circle: opposite corner.", 0.8, 0.55));
        e.params.push_back(Rgb("color", "Colour", "Marker colour. Red for underline, yellow for highlighter.", 0.92, 0.12, 0.10));
        e.params.push_back(Dbl("thickness", "Thickness (%)", "Line thickness (underline, box, circle).", 1.3, 0.1, 10.0, 0.2, 3.0));
        e.params.push_back(Dbl("hlheight", "Highlighter Height (%)", "Height of the highlighter band.", 5.0, 0.5, 40.0, 1.0, 15.0));
        e.params.push_back(Choice("blend", "Highlighter Blend", "Multiply keeps dark text readable.", 0, {"Multiply", "Normal"}));
        e.params.push_back(Dbl("roughness", "Hand-drawn Wobble", "How shaky the stroke is.", 0.7, 0.0, 3.0, 0.0, 2.0));
        e.params.push_back(Dbl("opacity", "Opacity", "Overall opacity.", 1.0, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Bool("shadow", "Shadow", "Soft shadow under the stroke.", false));
        addDrawTiming(e.params, 0.3, 0.6);
        defs.push_back(e);
    }
    {  // 6
        EffectDef e{"ArrowRoute", "Arrow and Route", "Curved arrow or dashed travel route that draws itself from A to B. Route style adds a start dot and a destination pin.", {}, fxArrow};
        e.params.push_back(Choice("style", "Style", "Arrow, dashed arrow, or travel route.", 0, {"Arrow", "Dashed arrow", "Travel route"}));
        e.params.push_back(Pt("pointa", "Point A (start)", "Where it starts.", 0.2, 0.7));
        e.params.push_back(Pt("pointb", "Point B (end)", "Where it ends.", 0.75, 0.3));
        e.params.push_back(Dbl("curve", "Curve", "Bend of the line (negative bends the other way).", 25.0, -100.0, 100.0, -60.0, 60.0));
        e.params.push_back(Rgb("color", "Colour", "Line colour.", 1.0, 1.0, 1.0));
        e.params.push_back(Dbl("thickness", "Thickness (%)", "Line thickness.", 0.9, 0.1, 10.0, 0.2, 3.0));
        e.params.push_back(Dbl("headsize", "Arrow Head Size (%)", "Size of the arrow head.", 3.5, 0.5, 20.0, 1.0, 8.0));
        e.params.push_back(Dbl("dash", "Dash Length (%)", "Dash length for dashed styles.", 2.2, 0.5, 20.0, 1.0, 6.0));
        e.params.push_back(Bool("shadow", "Shadow", "Soft shadow under the line.", true));
        addDrawTiming(e.params, 0.3, 0.9);
        defs.push_back(e);
    }
    {  // 7
        EffectDef e{"SlideInOut", "Slide In Out", "Clip slides in from any side with an optional bounce, and slides out at the end.", {}, fxSlide};
        e.params.push_back(Choice("indir", "Enter From", "Where the clip comes in from.", 0, {"Left", "Right", "Top", "Bottom", "None"}));
        e.params.push_back(Choice("outdir", "Exit To", "Where the clip leaves to.", 4, {"Left", "Right", "Top", "Bottom", "None"}));
        e.params.push_back(Dbl("induration", "Enter Time (s)", "Length of the entrance.", 0.55, 0.0, 10.0, 0.0, 3.0));
        e.params.push_back(Dbl("outduration", "Exit Time (s)", "Length of the exit at the end of the clip.", 0.45, 0.0, 10.0, 0.0, 3.0));
        e.params.push_back(Dbl("delay", "Start Delay (s)", "Seconds before the entrance begins.", 0.0, 0.0, 120.0, 0.0, 5.0));
        e.params.push_back(Dbl("bounce", "Bounce", "Overshoot when landing.", 0.35, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("distance", "Distance (%)", "100 = starts fully off screen.", 100.0, 5.0, 300.0, 20.0, 150.0));
        e.params.push_back(Dbl("tilt", "Tilt (deg)", "Rotation while sliding.", 4.0, -45.0, 45.0, -15.0, 15.0));
        e.params.push_back(Bool("fade", "Fade", "Fade while sliding.", false));
        defs.push_back(e);
    }
    {  // 8
        EffectDef e{"StickerOutline", "Sticker Outline", "Cut-out sticker look: white outline, optional colour glow and shadow around the subject. Needs a clip with transparency (cut the subject out first).", {}, fxSticker};
        e.params.push_back(Dbl("width", "Outline Width (%)", "Thickness of the outline.", 1.2, 0.0, 10.0, 0.0, 4.0));
        e.params.push_back(Rgb("outlinecolor", "Outline Colour", "Colour of the outline.", 1.0, 1.0, 1.0));
        e.params.push_back(Dbl("glowsize", "Glow Size (%)", "Size of the colour glow. 0 = off.", 0.0, 0.0, 30.0, 0.0, 10.0));
        e.params.push_back(Rgb("glowcolor", "Glow Colour", "Colour of the glow.", 1.0, 0.45, 0.10));
        e.params.push_back(Dbl("glowstrength", "Glow Strength", "Glow strength.", 0.9, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("shadow", "Shadow", "Drop shadow strength.", 0.4, 0.0, 1.0, 0.0, 1.0));
        e.params.push_back(Dbl("threshold", "Alpha Threshold", "Which pixels count as the subject.", 0.5, 0.05, 0.95, 0.05, 0.95));
        defs.push_back(e);
    }
    {  // 9
        EffectDef e{"PolaroidFrame", "Polaroid Frame", "White photo frame with tape that drops in at a slight angle. Ideal for travel photos.", {}, fxPolaroid};
        e.params.push_back(Pt("pos", "Position", "Where the photo sits.", 0.5, 0.5));
        e.params.push_back(Dbl("size", "Photo Size (%)", "Size of the photo inside the frame.", 68.0, 10.0, 100.0, 20.0, 100.0));
        e.params.push_back(Dbl("rotation", "Rotation", "Tilt in degrees.", -3.0, -45.0, 45.0, -15.0, 15.0));
        e.params.push_back(Dbl("border", "Border (%)", "White border width.", 6.0, 0.0, 30.0, 0.0, 15.0));
        e.params.push_back(Dbl("caption", "Caption Space (%)", "Extra white space at the bottom.", 18.0, 0.0, 60.0, 0.0, 40.0));
        e.params.push_back(Rgb("paper", "Frame Colour", "Colour of the frame.", 0.98, 0.97, 0.94));
        e.params.push_back(Bool("tape", "Tape", "Show a piece of tape at the top.", true));
        e.params.push_back(Dbl("shadow", "Shadow", "Drop shadow strength.", 0.5, 0.0, 1.0, 0.0, 1.0));
        addTiming(e.params, 0.55, 0.0);
        defs.push_back(e);
    }
    {  // 10
        EffectDef e{"AutoZoom", "Auto Zoom", "Slow zoom and pan across the whole clip. Brings still photos to life.", {}, fxAutoZoom};
        e.params.push_back(Dbl("startzoom", "Start Zoom", "Zoom at the start of the clip.", 1.0, 0.2, 5.0, 0.5, 2.0));
        e.params.push_back(Dbl("endzoom", "End Zoom", "Zoom at the end of the clip.", 1.15, 0.2, 5.0, 0.5, 2.0));
        e.params.push_back(Choice("pandir", "Pan", "Direction of the slow pan.", 0, {"None", "Left", "Right", "Up", "Down"}));
        e.params.push_back(Dbl("panamount", "Pan Amount (%)", "How far it pans.", 4.0, 0.0, 30.0, 0.0, 15.0));
        e.params.push_back(Pt("center", "Zoom Centre", "The point the zoom pushes towards.", 0.5, 0.5));
        e.params.push_back(Choice("ease", "Easing", "Linear or smooth start and stop.", 1, {"Linear", "Smooth"}));
        defs.push_back(e);
    }
    return defs;
}
#endif  // PP_DIAG

}  // namespace pp
