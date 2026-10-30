// Offline test harness: runs one effect on a PPM image and writes a PPM preview.
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "../src/effects.hpp"
using namespace pp;

static bool readPPM(const char *fn, Img &img) {
    FILE *f = fopen(fn, "rb"); if (!f) return false;
    int w, h, mx; if (fscanf(f, "P6 %d %d %d", &w, &h, &mx) != 3) return false; fgetc(f);
    img = Img(w, h); std::vector<unsigned char> buf((size_t)w * h * 3);
    if (fread(buf.data(), 1, buf.size(), f) != buf.size()) return false; fclose(f);
    for (size_t i = 0; i < (size_t)w * h; ++i) { for (int c = 0; c < 3; ++c) img.px[i*4+c] = buf[i*3+c] / 255.f; img.px[i*4+3] = (getenv("ALPHAKEY") && img.px[i*4]+img.px[i*4+1]+img.px[i*4+2] < 0.02f) ? 0.f : 1.f; if (img.px[i*4+3] == 0.f) { img.px[i*4]=img.px[i*4+1]=img.px[i*4+2]=0.f; } }
    return true;
}
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: harness EffectId in.ppm out.ppm t [dur] [key=val ...]\n"); return 1; }
    const EffectDef *def = nullptr;
    for (auto &e : allEffects()) if (std::string(e.id) == argv[1]) def = &e;
    if (!def) { fprintf(stderr, "unknown effect\n"); return 1; }
    Img in; if (!readPPM(argv[2], in)) { fprintf(stderr, "read fail\n"); return 1; }
    PV pv;
    for (auto &p : def->params) pv.v[p.id] = {p.def[0], p.def[1], p.def[2]};
    TimeInfo ti; ti.t = atof(argv[4]); ti.dur = argc > 5 ? atof(argv[5]) : 5.0; ti.tRemain = std::max(0.04, ti.dur - ti.t); ti.fps = 25;
    for (int i = 6; i < argc; ++i) {
        std::string s = argv[i]; size_t eq = s.find('='); if (eq == std::string::npos) continue;
        std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        std::array<double,3> a = pv.v[k]; std::stringstream ss(v); std::string tok; int j = 0;
        while (std::getline(ss, tok, ',') && j < 3) a[j++] = atof(tok.c_str());
        pv.v[k] = a;
    }
    Img out(in.w, in.h);
    def->fn(in, out, pv, ti);
    FILE *f = fopen(argv[3], "wb"); fprintf(f, "P6\n%d %d\n255\n", out.w, out.h);
    for (int y = 0; y < out.h; ++y) for (int x = 0; x < out.w; ++x) {
        const float *p = out.at(x, y);
        float chk = (((x / 24) + (y / 24)) & 1) ? 0.35f : 0.55f;
        for (int c = 0; c < 3; ++c) { float v = p[c] + chk * (1.f - p[3]); unsigned char b = (unsigned char)(sat(v) * 255.f + 0.5f); fputc(b, f); }
    }
    fclose(f); return 0;
}
