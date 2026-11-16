// Fuzzes lensfun's image modifier: builds a lens with fuzzer-chosen distortion/TCA/vignetting/crop
// calibration, enables the corrections, and applies them to a small image's coordinates and pixels.
#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "lensfun.h"

static float real(FuzzedDataProvider &fdp, float lo, float hi)
{
    return fdp.ConsumeFloatingPointInRange<float>(lo, hi);
}

static lfLensCalibAttributes attrs(FuzzedDataProvider &fdp)
{
    lfLensCalibAttributes a;
    a.CropFactor = real(fdp, 0.5f, 10.0f);
    a.AspectRatio = real(fdp, 0.5f, 2.0f);
    return a;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    lfLens *lens = new lfLens();
    lens->Type = static_cast<lfLensType>(fdp.ConsumeIntegralInRange<int>(LF_UNKNOWN, LF_FISHEYE_THOBY));
    lens->CenterX = real(fdp, -0.5f, 0.5f);
    lens->CenterY = real(fdp, -0.5f, 0.5f);
    lens->CropFactor = real(fdp, 0.5f, 10.0f);
    lens->AspectRatio = real(fdp, 0.5f, 2.0f);

    int ncal = fdp.ConsumeIntegralInRange<int>(0, 3);
    for (int i = 0; i < ncal; i++)
    {
        lfLensCalibDistortion d;
        memset(&d, 0, sizeof d);
        d.Model = static_cast<lfDistortionModel>(fdp.ConsumeIntegralInRange<int>(LF_DIST_MODEL_NONE, LF_DIST_MODEL_ACM));
        d.Focal = real(fdp, 1.0f, 600.0f);
        d.RealFocal = real(fdp, 1.0f, 600.0f);
        d.RealFocalMeasured = fdp.ConsumeBool();
        for (float &t : d.Terms)
            t = real(fdp, -2.0f, 2.0f);
        d.CalibAttr = attrs(fdp);
        lens->AddCalibDistortion(&d);

        lfLensCalibTCA t;
        memset(&t, 0, sizeof t);
        t.Model = static_cast<lfTCAModel>(fdp.ConsumeIntegralInRange<int>(LF_TCA_MODEL_NONE, LF_TCA_MODEL_ACM));
        t.Focal = real(fdp, 1.0f, 600.0f);
        for (float &x : t.Terms)
            x = real(fdp, -2.0f, 2.0f);
        t.CalibAttr = attrs(fdp);
        lens->AddCalibTCA(&t);

        lfLensCalibVignetting v;
        memset(&v, 0, sizeof v);
        v.Model = static_cast<lfVignettingModel>(fdp.ConsumeIntegralInRange<int>(LF_VIGNETTING_MODEL_NONE, LF_VIGNETTING_MODEL_ACM));
        v.Focal = real(fdp, 1.0f, 600.0f);
        v.Aperture = real(fdp, 1.0f, 32.0f);
        v.Distance = real(fdp, 0.1f, 1000.0f);
        for (float &x : v.Terms)
            x = real(fdp, -2.0f, 2.0f);
        v.CalibAttr = attrs(fdp);
        lens->AddCalibVignetting(&v);
    }

    const int width = fdp.ConsumeIntegralInRange<int>(1, 48);
    const int height = fdp.ConsumeIntegralInRange<int>(1, 48);
    const float focal = real(fdp, 1.0f, 600.0f);
    const float crop = real(fdp, 0.5f, 10.0f);
    const lfPixelFormat pf = static_cast<lfPixelFormat>(fdp.ConsumeIntegralInRange<int>(LF_PF_U8, LF_PF_F64));
    const bool reverse = fdp.ConsumeBool();

    lfModifier *mod = new lfModifier(lens, focal, crop, width, height, pf, reverse);

    const int want = fdp.ConsumeIntegral<uint8_t>();
    if (want & 1) mod->EnableDistortionCorrection();
    if (want & 2) mod->EnableTCACorrection();
    if (want & 4) mod->EnableVignettingCorrection(real(fdp, 1.0f, 32.0f), real(fdp, 0.1f, 1000.0f));
    if (want & 8) mod->EnableScaling(real(fdp, 0.1f, 4.0f));
    if (want & 16)
        mod->EnableProjectionTransform(static_cast<lfLensType>(fdp.ConsumeIntegralInRange<int>(LF_UNKNOWN, LF_FISHEYE_THOBY)));
    if (want & 32)
    {
        float px[8], py[8];
        int n = fdp.ConsumeIntegralInRange<int>(4, 8);
        for (int i = 0; i < n; i++)
        {
            px[i] = real(fdp, -1000.0f, 1000.0f);
            py[i] = real(fdp, -1000.0f, 1000.0f);
        }
        mod->EnablePerspectiveCorrection(px, py, n, real(fdp, -1.0f, 1.0f));
    }

    std::vector<float> coords(static_cast<size_t>(width) * 2 * 3);
    const int rows = height;
    for (int y = 0; y < rows; y += 1 + rows / 4)
    {
        mod->ApplyGeometryDistortion(0.0f, static_cast<float>(y), width, 1, coords.data());
        mod->ApplySubpixelDistortion(0.0f, static_cast<float>(y), width, 1, coords.data());
        mod->ApplySubpixelGeometryDistortion(0.0f, static_cast<float>(y), width, 1, coords.data());
    }

    static const int pixel_bytes[] = {1, 2, 4, 4, 8};
    const size_t stride = static_cast<size_t>(width) * 3 * pixel_bytes[pf];
    std::vector<uint8_t> pixels(stride * height);
    for (auto &b : pixels)
        b = fdp.ConsumeIntegral<uint8_t>();
    mod->ApplyColorModification(pixels.data(), 0.0f, 0.0f, width, height,
                                LF_CR_3(RED, GREEN, BLUE), static_cast<int>(stride));

    delete mod;
    delete lens;
    return 0;
}
