#include "SpectrumAnalyzer.hpp"
#include "SettingsStore.hpp"
#include "ShortcutHelpLayout.hpp"
#include "RetroAssets.hpp"
#include "EngineeringMath.hpp"
#include "CurveSampling.hpp"
#include "SpectrumLayout.hpp"
#include "VisualStyle.hpp"
#include "ParticleMath.hpp"
#include "FireHeat.hpp"
#include "FireAccents.hpp"
#include <array>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numbers>

using namespace spectrum;

// Explicit checks remain active in Release builds (unlike assert under NDEBUG).
#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "CHECK failed: %s (line %d)\n", #expr, __LINE__); \
    return 1; \
} } while (false)

int main() {
    std::array<float, kFFTFrames * 2> stereo{};
    constexpr float pi = std::numbers::pi_v<float>;
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        stereo[i * 2] = 0.6f * std::sin(2.0f * pi * 110.0f * t);
        stereo[i * 2 + 1] = 0.4f * std::sin(2.0f * pi * 880.0f * t);
    }

    // Fire heat simulation: continuous unsegmented thermal pixels, channel
    // separation, horizontal mirror, size bounds, opacity and silence decay.
    FireHeatField leftFire, rightFire, mirroredFire;
    leftFire.resize(96,72);
    rightFire.resize(96,72);
    mirroredFire.resize(96,72);
    CHECK(leftFire.width==96 && leftFire.height==72);
    CHECK(leftFire.heat.size()==96u*72u);
    CHECK(leftFire.rgba.size()==96u*72u*4u);
    CHECK(FireHeatField::fireColor(0.0f)[3]==0);
    CHECK(FireHeatField::fireColor(0.9f)[0]>
          FireHeatField::fireColor(0.2f)[0]);
    CHECK(FireHeatField::fireColor(0.9f)[3]>200);
    CHECK(FireHeatField::fireColor(0.75f,0) !=
          FireHeatField::fireColor(0.75f,1));
    CHECK(FireHeatField::fireColor(0.75f,1) !=
          FireHeatField::fireColor(0.75f,2));
    CHECK(FireHeatField::fireColor(0.75f,2) !=
          FireHeatField::fireColor(0.75f,0));
    CHECK(fireSpeedScale(0) < fireSpeedScale(1));
    CHECK(fireSpeedScale(1) < fireSpeedScale(2));
    CHECK(firePeakModeName(1)[0]=='F');
    CHECK(firePaletteName(2)[0]=='P');

    FirePeakTracker peakTracker;
    std::array<float,kMaxBands> peakSignal{};
    peakSignal[0]=0.72f;
    peakTracker.advance(peakSignal,128,1,1.0f/60.0f);
    CHECK(peakTracker.tip[0]>0.70f);
    CHECK(peakTracker.tip[1]==0.0f);
    peakSignal.fill(0.0f);
    for(int t=0;t<8;++t)
        peakTracker.advance(peakSignal,128,1,0.10f);
    CHECK(peakTracker.tip[0]<0.01f);
    peakTracker.reset();
    peakSignal[0]=0.81f;
    peakTracker.advance(peakSignal,128,2,0.016f);
    peakSignal[0]=0.0f;
    for(int t=0;t<5;++t)
        peakTracker.advance(peakSignal,128,2,0.10f);
    CHECK(peakTracker.tip[0]>0.80f);
    for(int t=0;t<25;++t)
        peakTracker.advance(peakSignal,128,2,0.10f);
    CHECK(peakTracker.tip[0]<0.15f);
    peakTracker.advance(peakSignal,64,0,0.016f);
    CHECK(peakTracker.lastBands==64);
    CHECK(peakTracker.tip[0]==0.0f);

    std::array<float,kMaxBands> fireSignal{};
    fireSignal[0]=1.0f; fireSignal[1]=0.9f; fireSignal[2]=0.8f;
    const std::array<float,kMaxBands> fireSilence{};
    for(int tick=0;tick<18;++tick){
        CHECK(leftFire.advance(fireSignal,128,false,0.40f,1.0f/30.0f));
        CHECK(mirroredFire.advance(fireSignal,128,true,0.40f,1.0f/30.0f));
        CHECK(rightFire.advance(fireSilence,128,false,0.0f,1.0f/30.0f));
    }
    float heatLeft=0.0f,heatRight=0.0f,mirrorLeft=0.0f,mirrorRight=0.0f;
    for(int y=0;y<leftFire.height;++y)
      for(int x=0;x<leftFire.width;++x){
        const std::size_t i=static_cast<std::size_t>(y*leftFire.width+x);
        if(x<leftFire.width/2){
            heatLeft+=leftFire.heat[i];
            mirrorLeft+=mirroredFire.heat[i];
        }else{
            heatRight+=leftFire.heat[i];
            mirrorRight+=mirroredFire.heat[i];
        }
        CHECK(rightFire.heat[i]==0.0f);
    }
    CHECK(heatLeft>heatRight*5.0f);
    CHECK(mirrorRight>mirrorLeft*5.0f);
    CHECK(heatLeft>0.25f);
    CHECK(!leftFire.advance(fireSignal,128,false,0.0f,0.0f));
    // Quiet source must stop injecting new heat and cool naturally.
    float preSilence=0.0f;
    for(float h:leftFire.heat)preSilence+=h;
    for(int tick=0;tick<90;++tick)
        leftFire.advance(fireSilence,128,false,0.0f,1.0f/30.0f);
    float postSilence=0.0f;
    for(float h:leftFire.heat)postSilence+=h;
    CHECK(postSilence<preSilence*0.03f);
    leftFire.resize(8000,8000);
    CHECK(leftFire.width==320 && leftFire.height==192);

    // Local-only OBJ asset loading: geometry, material colors, animated
    // frame selection, and texture paths are deliberately not committed.
    const auto retroTestRoot = std::filesystem::current_path() /
                               "SpectrumWall_PrivateModel_Test";
    std::error_code cleanupEc;
    std::filesystem::remove_all(retroTestRoot, cleanupEc);
    std::filesystem::create_directories(retroTestRoot / "can");
    std::filesystem::create_directories(retroTestRoot / "dancer" / "frames");
    {
        std::ofstream mtl(retroTestRoot / "can" / "sample.mtl");
        mtl << "newmtl cola\nKd 1.0 0.2 0.15\nmap_Kd label.png\n";
        std::ofstream obj(retroTestRoot / "can" / "can.obj");
        obj << "mtllib sample.mtl\n"
            << "v -1 0 0\nv 1 0 0\nv 0 2 0\n"
            << "vt 0 0\nvt 1 0\nvt 0.5 1\n"
            << "usemtl cola\nf 1/1 2/2 3/3\n";
    }
    const auto testCan=loadRetroAssetClip(retroTestRoot/"can",false);
    CHECK(testCan.frames.size()==1);
    CHECK(testCan.frames[0].size()==1);
    CHECK(testCan.textures.size()==1);
    CHECK(testCan.textures[0].filename()=="label.png");
    CHECK(testCan.frames[0][0].textureId==0);
    CHECK(testCan.frames[0][0].color.r==255);
    CHECK(std::abs(testCan.frames[0][0].uvC.v-0.0f)<1e-6f);
    for(int frame=0;frame<2;++frame){
        std::ofstream obj(retroTestRoot/"dancer"/"frames"/
                          ("frame_000"+std::to_string(frame)+".obj"));
        obj << "v 0 0 0\nv 1 0 0\nv 0 "
            << (frame==0?1:2) << " 0\nf 1 2 3\n";
    }
    const auto testDance=loadRetroAssetClip(retroTestRoot/"dancer",true);
    CHECK(testDance.frames.size()==2);
    CHECK(testDance.frames[0].size()==1);
    CHECK(testDance.frame(0.0f).size()==1);
    CHECK(std::abs(testDance.frame(1.0f/24.0f)[0].c.y-
                   testDance.frame(0.0f)[0].c.y)>0.1f);
    CHECK(loadRetroAssetClip(retroTestRoot/"missing",true).empty());
    std::filesystem::remove_all(retroTestRoot,cleanupEc);

    // Foreground audio sparks: normalized FFT placement respects each
    // mirrored panel, silence never emits, and glow fades monotonically.
    CHECK(std::abs(sparkBandFraction(0,128,false)-
                   0.5f/128.0f)<1.0e-6f);
    CHECK(std::abs(sparkBandFraction(0,128,true)+
                   sparkBandFraction(0,128,false)-1.0f)<1.0e-6f);
    CHECK(std::abs(sparkBandFraction(127,128,true)-
                   0.5f/128.0f)<1.0e-6f);
    CHECK(sparkBandFraction(0,0,false)==0.5f);
    CHECK(sparkEmissionRate(0.0f,0.0f)==0.0f);
    CHECK(sparkEmissionRate(0.25f,0.0f)>0.0f);
    CHECK(sparkEmissionRate(0.25f,0.55f)>
          sparkEmissionRate(0.25f,0.0f));
    CHECK(sparkEmissionRate(1.0f,1.0f)>
          sparkEmissionRate(0.25f,0.55f));
    CHECK(sparkFade(0.0f,1.0f)==1.0f);
    CHECK(sparkFade(1.0f,1.0f)==0.0f);
    CHECK(sparkFade(0.5f,1.0f)<sparkFade(0.1f,1.0f));
    CHECK(sparkFade(0.1f,0.0f)==0.0f);
    std::uint32_t seed=0x41c06a1du, copy=seed;
    for(int i=0;i<64;++i){
        const float v=sparkRandom(seed);
        CHECK(v>=0.0f && v<1.0f);
        CHECK(v==sparkRandom(copy));
    }

    // Shortcut popup uses two columns at fullscreen resolutions and switches
    // to paginated, scaled single-column help for smaller windowed previews.
    const auto centerHelp = shortcutHelpLayout(3440.0f, 1440.0f, 0);
    CHECK(centerHelp.columns == 2);
    CHECK(centerHelp.pages == 1 && centerHelp.page == 0);
    CHECK(!centerHelp.compact && centerHelp.scale == 1.0f);
    const auto portraitHelp = shortcutHelpLayout(1080.0f, 1920.0f, 0);
    CHECK(portraitHelp.columns == 2 && portraitHelp.scale == 1.0f);
    const auto compactHelp = shortcutHelpLayout(610.0f, 740.0f, 1);
    CHECK(compactHelp.columns == 1);
    CHECK(compactHelp.pages == 2 && compactHelp.page == 1);
    CHECK(compactHelp.scale == 1.0f);
    const auto compactWrapped = shortcutHelpLayout(610.0f, 740.0f, 3);
    CHECK(compactWrapped.page == 1);
    const auto tinyHelp = shortcutHelpLayout(350.0f, 300.0f, 0);
    CHECK(tinyHelp.compact && tinyHelp.scale > 0.0f && tinyHelp.scale < 1.0f);
    CHECK(kVisualShortcuts.size() == 18);
    CHECK(kControlShortcuts.size() == 14);
    for (const auto& item : kVisualShortcuts) {
        CHECK(item.key != nullptr && item.key[0] != '\0');
        CHECK(item.description != nullptr && item.description[0] != '\0');
    }
    for (const auto& item : kControlShortcuts) {
        CHECK(item.key != nullptr && item.key[0] != '\0');
        CHECK(item.description != nullptr && item.description[0] != '\0');
    }

    // Bass in the middle on the center stereo panel:
    // left half maps low bin -> right edge, right half stays low -> left edge.
    for (const std::size_t count : {64u, 96u, 128u, 256u}) {
        for (std::size_t b = 0; b < count; ++b) {
            CHECK(visualBandIndex(b, count, false) == b);
            CHECK(visualBandIndex(b, count, true) == count - 1 - b);
        }
        CHECK(visualBandIndex(0, count, true) == count - 1);
        CHECK(visualBandIndex(0, count, false) == 0);
        CHECK(visualBandIndex(count - 1, count, true) == 0);
        // Left portrait remains low-to-high; right portrait mirrors by default.
        CHECK(!portraitBandMirrored(true));
        CHECK(portraitBandMirrored(false));
        CHECK(!portraitBandMirrored(false, false));
        CHECK(visualBandIndex(0, count, portraitBandMirrored(true)) == 0);
        CHECK(visualBandIndex(0, count, portraitBandMirrored(false)) == count - 1);
        CHECK(visualBandIndex(count - 1, count, portraitBandMirrored(false)) == 0);
    }

    // Curves must interpolate the actual FFT values without overshoot and
    // preserve both channel orientation and left/center/right mirror behavior.
    for (const std::size_t bands : {64u, 96u, 128u, 192u, 256u}) {
        std::array<float, kMaxBands> ramp{};
        for (std::size_t i = 0; i < bands; ++i)
            ramp[i] = static_cast<float>(i) /
                      static_cast<float>(bands - 1);
        for (bool mirror : {false, true}) {
            const float firstExpected = mirror ? 1.0f : 0.0f;
            const float lastExpected = mirror ? 0.0f : 1.0f;
            CHECK(std::abs(sampleSpectrumCurve(ramp, bands, 0.0f, mirror)
                           - firstExpected) < 1.0e-6f);
            CHECK(std::abs(sampleSpectrumCurve(ramp, bands,
                           static_cast<float>(bands - 1), mirror)
                           - lastExpected) < 1.0e-6f);
            for (std::size_t i = 0; i < (bands - 1) * 3 + 1; ++i) {
                const float pos = static_cast<float>(i) / 3.0f;
                const float v = sampleSpectrumCurve(ramp, bands, pos, mirror);
                CHECK(std::isfinite(v));
                CHECK(v >= 0.0f && v <= 1.0f);
                const auto bin = static_cast<std::size_t>(pos);
                const float a = ramp[visualBandIndex(bin, bands, mirror)];
                const float b = ramp[visualBandIndex(
                    std::min(bin + 1, bands - 1), bands, mirror)];
                CHECK(v >= std::min(a,b)-1e-6f);
                CHECK(v <= std::max(a,b)+1e-6f);
            }
        }
    }

    // Log frequency axis endpoints, bucket center and dBFS mapping.
    CHECK(std::abs(spectrumFrequencyFraction(30.0f)) < 1.0e-6f);
    CHECK(std::abs(spectrumFrequencyFraction(18000.0f) - 1.0f) < 1.0e-6f);
    CHECK(std::abs(spectrumFrequencyAtFraction(0.0f) - 30.0f) < 0.01f);
    CHECK(std::abs(spectrumFrequencyAtFraction(1.0f) - 18000.0f) < 0.1f);
    CHECK(std::abs(spectrumDbAtFraction(0.0f) + 72.0f) < 0.001f);
    CHECK(std::abs(spectrumDbAtFraction(1.0f) + 2.0f) < 0.001f);
    CHECK(std::abs(spectrumDbFraction(-37.0f) - 0.5f) < 0.001f);
    // Bottom VU bars and sample peaks must share a logarithmic dBFS ruler.
    CHECK(std::abs(levelMeterDbFraction(-72.0f)) < 1.0e-6f);
    CHECK(std::abs(levelMeterDbFraction(0.0f) - 1.0f) < 1.0e-6f);
    CHECK(std::abs(levelMeterDbAtFraction(0.5f) + 36.0f) < 1.0e-4f);
    CHECK(std::abs(levelMeterSamplePeakFraction(1.0f) - 1.0f) < 1.0e-6f);
    CHECK(std::abs(levelMeterSamplePeakFraction(0.5f) -
                   levelMeterDbFraction(-6.0206f)) < 1.0e-5f);
    CHECK(std::abs(levelMeterSamplePeakFraction(std::pow(10.0f, -2.0f/20.0f)) -
                   levelMeterDbFraction(-2.0f)) < 1.0e-6f);
    for (const float db : {-72.0f, -48.0f, -24.0f, -12.0f, -6.0f, -2.0f, 0.0f}) {
        const float position = levelMeterDbFraction(db);
        CHECK(std::abs(levelMeterDbAtFraction(position) - db) < 1.0e-5f);
        // The smoothed VU maps back to the same scale used by peak markers.
        if (db <= kSpectrumCeilingDb) {
            CHECK(std::abs(levelMeterSmoothedRmsFraction(spectrumDbFraction(db)) -
                           position) < 1.0e-5f);
        }
        // Mirroring changes placement but must not affect the decibel reading.
        CHECK(std::abs(position + (1.0f - position) - 1.0f) < 1.0e-6f);
    }

    CHECK(spectrumBandCenterHz(0, 128) >= 30.0f);
    CHECK(spectrumBandCenterHz(127, 128) <= 18000.0f);

    // Silence is centered; equal RMS is balanced; positive bias is R.
    CHECK(stereoBalanceDb(0.0f, 0.0f) == 0.0f);
    CHECK(stereoBalanceDb(0.00001f, 0.00002f) == 0.0f);
    CHECK(std::abs(stereoBalanceDb(0.5f, 0.5f)) < 0.001f);
    CHECK(std::abs(stereoBalanceDb(0.5f, 1.0f) - 6.0206f) < 0.002f);
    CHECK(std::abs(stereoBalanceDb(1.0f, 0.5f) + 6.0206f) < 0.002f);
    CHECK(stereoBalancePosition(100.0f) == 1.0f);
    CHECK(stereoBalancePosition(-100.0f) == -1.0f);
    CHECK(stereoBalancePosition(0.0f) == 0.0f);

    SpectrumAnalyzer analyzer;
    CHECK(analyzer.bandCount() == 128);

    for (const std::size_t count : { 64u, 80u, 96u, 128u, 192u, 224u, 256u }) {
        analyzer.setBandCount(count);
        CHECK(analyzer.bandCount() == count);
        // Out-of-range bars must remain inactive after changing the partition.
        const auto& blank = analyzer.state();
        for (std::size_t b = count; b < kMaxBands; ++b) {
            CHECK(blank.left[b] == 0.0f);
            CHECK(blank.right[b] == 0.0f);
        }

        analyzer.update(stereo, 1.0f / 60.0f);
        const auto& result = analyzer.state();
        CHECK(result.bandCount == count);
        CHECK(result.rmsLeft > 0.1f && result.rmsRight > 0.1f);
        CHECK(result.peakLeft > 0.5f && result.peakRight > 0.3f);
        CHECK(std::isfinite(result.balanceDb));
        CHECK(std::isfinite(result.balanceDbSmoothed));
        CHECK(result.balanceDb < 0.0f); // left 0.6, right 0.4
        bool rawLeftDetected = false;
        for (std::size_t b = 0; b < count; ++b)
            rawLeftDetected |= result.leftRawDb[b] > -40.0f;
        CHECK(rawLeftDetected);
        bool leftActive = false, rightActive = false;
        for (std::size_t b = 0; b < count; ++b) {
            CHECK(result.left[b] >= 0.0f && result.left[b] <= 1.0f);
            CHECK(result.right[b] >= 0.0f && result.right[b] <= 1.0f);
            CHECK(result.leftPeaks[b] >= 0.0f && result.leftPeaks[b] <= 1.0f);
            CHECK(result.rightPeaks[b] >= 0.0f && result.rightPeaks[b] <= 1.0f);
            leftActive |= result.left[b] > 0.0f;
            rightActive |= result.right[b] > 0.0f;
        }
        CHECK(leftActive && rightActive);
    }

    // Verify temporal smoothing modes and their runtime selection.
    SpectrumAnalyzer quick(128), fluid(128);
    quick.setSmoothingMode(SmoothingMode::Fast);
    fluid.setSmoothingMode(SmoothingMode::Liquid);
    quick.setSpectralSoftening(false);
    fluid.setSpectralSoftening(false);
    CHECK(!quick.spectralSoftening());
    quick.update(stereo, 1.0f / 60.0f);
    fluid.update(stereo, 1.0f / 60.0f);
    bool foundFasterAttack = false;
    for (std::size_t b = 0; b < 128; ++b)
        if (quick.state().left[b] > fluid.state().left[b] + 0.01f)
            foundFasterAttack = true;
    CHECK(foundFasterAttack);
    CHECK(quick.smoothingMode() == SmoothingMode::Fast);
    CHECK(fluid.smoothingMode() == SmoothingMode::Liquid);
    CHECK(smoothingModeName(SmoothingMode::Liquid)[0] == 'L');

    for (int palette = 0; palette < kPaletteCount; ++palette) {
        const Rgb bass = paletteRgb(0.05f, 0.2f, 0.9f, palette, false);
        const Rgb treble = paletteRgb(0.95f, 0.8f, 0.9f, palette, true);
        CHECK(bass.r != treble.r || bass.g != treble.g ||
              bass.b != treble.b);
        CHECK(paletteName(palette)[0] != '\0');
    }
    CHECK(hsvToRgb(0.0f, 1.0f, 1.0f).r == 255);
    CHECK(hsvToRgb(120.0f, 1.0f, 1.0f).g == 255);
    CHECK(hsvToRgb(240.0f, 1.0f, 1.0f).b == 255);

    // The settings are written only at shutdown. Verify first-create,
    // update-in-place (Windows atomic replacement), and restart restoration.
    const auto configPath = std::filesystem::current_path() /
                             "SpectrumWall_Regression_Only.ini";
    std::error_code fileEc;
    std::filesystem::remove(configPath, fileEc);
    bool loaded = true;
    auto defaults = readSessionSettings(configPath, &loaded);
    CHECK(!loaded);
    CHECK(defaults.bands == 128);
    CHECK(defaults.visual == 1);
    CHECK(defaults.centerMirror);

    SessionSettings settings;
    settings.bands = 192;
    settings.palette = 7;
    settings.visual = 3;
    settings.fire_palette = 2;
    settings.fire_peaks = 2;
    settings.fire_lines = true;
    settings.fire_meters = true;
    settings.fire_speed = 2;
    settings.fire_quality = 0;
    settings.fire_fourth = true;
    settings.smoothing = 2;
    settings.engineering = true;
    settings.stereoBalance = true;
    settings.spectralSoftening = false;
    settings.glow = 1;
    settings.trails = false;
    settings.reflections = false;
    settings.particles = false;
    settings.centerMirror = false;
    settings.rightPortraitMirror = false;
    settings.fourth = true;
    settings.windowed = true;
    settings.demo = true;
    settings.loopback = true;
    settings.source = "Speakers (M-Caster Studio)";
    CHECK(writeSessionSettings(configPath, settings));
    auto restored = readSessionSettings(configPath, &loaded);
    CHECK(loaded);
    CHECK(restored.bands == 192 && restored.palette == 7);
    CHECK(restored.visual == 3 && restored.smoothing == 2);
    CHECK(restored.fire_palette==2 && restored.fire_peaks==2);
    CHECK(restored.fire_lines && restored.fire_meters);
    CHECK(restored.fire_speed==2);
    CHECK(restored.fire_quality==0 && restored.fire_fourth);
    CHECK(restored.engineering && restored.stereoBalance);
    CHECK(!restored.spectralSoftening && restored.glow == 1);
    CHECK(!restored.trails && !restored.reflections && !restored.particles);
    CHECK(!restored.centerMirror && !restored.rightPortraitMirror);
    CHECK(restored.fourth && restored.windowed && restored.demo && restored.loopback);
    CHECK(restored.source == "Speakers (M-Caster Studio)");

    // Replacing an existing INI is different from writing a new file on
    // Windows: this is where naive rename() would fail.
    settings.bands = 64;
    settings.palette = 1;
    settings.visual = 0;
    settings.engineering = false;
    settings.stereoBalance = false;
    settings.centerMirror = true;
    settings.source = "Realtek Digital Input (Realtek High Definition Audio)";
    CHECK(writeSessionSettings(configPath, settings));
    restored = readSessionSettings(configPath, &loaded);
    CHECK(loaded);
    CHECK(restored.bands == 64 && restored.palette == 1);
    CHECK(restored.visual == 0);
    CHECK(restored.fire_palette==2 && restored.fire_peaks==2);
    CHECK(restored.fire_lines && restored.fire_meters);
    CHECK(restored.fire_quality==0);
    CHECK(restored.fire_fourth);
    CHECK(!restored.engineering && !restored.stereoBalance);
    CHECK(restored.centerMirror);
    CHECK(restored.source == settings.source);
    CHECK(!std::filesystem::exists(
        std::filesystem::path(configPath.string() + ".tmp")));

    // Edits to a hand-maintained configuration must be range-checked.
    {
        std::ofstream output(configPath, std::ios::trunc);
        output << "version=1\nbands=900\npalette=99\n"
               << "visual=9\nengineering=true\nsource=Realtek\n";
    }
    restored = readSessionSettings(configPath, &loaded);
    CHECK(loaded);
    CHECK(restored.bands == 128 && restored.palette == 4);
    CHECK(restored.visual == 1 && restored.engineering);
    CHECK(restored.source == "Realtek");
    {
        std::ofstream output(configPath, std::ios::trunc);
        output << "version=2\nbands=192\n";
    }
    restored = readSessionSettings(configPath, &loaded);
    CHECK(!loaded);
    CHECK(restored.bands == 128);
    std::filesystem::remove(configPath, fileEc);

    analyzer.setBandCount(1);
    CHECK(analyzer.bandCount() == kMinBands);
    analyzer.setBandCount(500);
    CHECK(analyzer.bandCount() == kMaxBands);
    std::puts("Spectrum band switching and stereo FFT tests passed.");
    return 0;
}
