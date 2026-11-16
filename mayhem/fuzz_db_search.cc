// Fuzzes lensfun's camera/lens search (fuzzy string matching, mount compatibility) over a small
// fixed database embedded below; the fuzzer controls only the query strings and flags.
#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "lensfun.h"

static const char kDb[] = R"XML(<lensdatabase version="2">
<mount><name>Pentax KAF2</name><compat>Pentax KAF</compat><compat>Pentax KA</compat></mount>
<mount><name>Pentax KAF</name><compat>Pentax KA</compat></mount>
<mount><name>Canon EF</name><compat>Canon EF-S</compat></mount>
<mount><name>Nikon F AF</name></mount>
<camera><maker>Pentax Corporation</maker><maker lang="en">Pentax</maker>
 <model>Pentax K100D</model><model lang="en">K100D</model><mount>Pentax KAF2</mount>
 <cropfactor>1.531</cropfactor></camera>
<camera><maker>Canon</maker><model>Canon EOS 5D Mark II</model><model lang="en">EOS 5D Mark II</model>
 <mount>Canon EF</mount><cropfactor>1</cropfactor></camera>
<camera><maker>Nikon Corporation</maker><maker lang="en">Nikon</maker>
 <model>Nikon D90</model><model lang="en">D90</model><mount>Nikon F AF</mount>
 <cropfactor>1.523</cropfactor></camera>
<lens><maker>Pentax</maker><model>smc Pentax-DA 50-200mm f/4-5.6 DA ED</model><mount>Pentax KAF2</mount>
 <cropfactor>1.53</cropfactor><calibration>
 <distortion model="poly3" focal="50" k1="-0.004038"/><distortion model="poly3" focal="200" k1="0.007685"/>
 </calibration></lens>
<lens><maker>Pentax</maker><model>smc Pentax-FA 28mm f/2.8 AL</model><mount>Pentax KAF2</mount>
 <cropfactor>1.53</cropfactor></lens>
<lens><maker>Canon</maker><model>Canon EF 24-70mm f/2.8L USM</model><mount>Canon EF</mount>
 <cropfactor>1</cropfactor></lens>
<lens><maker>Nikon</maker><model>Nikkor AF-S 70-300mm f/4.5-5.6G VR</model><model lang="en">70-300 VR</model>
 <mount>Nikon F AF</mount><cropfactor>1</cropfactor></lens>
<lens><maker>Sigma</maker><model>Sigma 18-35mm f/1.8 DC HSM | A</model><mount>Pentax KAF2</mount>
 <mount>Canon EF</mount><mount>Nikon F AF</mount><cropfactor>1.5</cropfactor></lens>
</lensdatabase>
)XML";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    std::string cam_maker = fdp.ConsumeRandomLengthString(64);
    std::string cam_model = fdp.ConsumeRandomLengthString(64);
    std::string lens_maker = fdp.ConsumeRandomLengthString(64);
    std::string lens_model = fdp.ConsumeRandomLengthString(128);
    int cam_flags = fdp.ConsumeIntegralInRange<int>(0, 3);
    int lens_flags = fdp.ConsumeIntegralInRange<int>(0, 3);

    lfDatabase *db = new lfDatabase();
    if (db->Load(kDb, sizeof(kDb) - 1) != LF_NO_ERROR)
        __builtin_trap();

    const lfCamera **cams = db->FindCamerasExt(cam_maker.c_str(), cam_model.c_str(), cam_flags);
    const lfCamera **cams2 = db->FindCameras(cam_maker.c_str(), cam_model.c_str());
    const lfLens **lenses = db->FindLenses(cams ? cams[0] : NULL, lens_maker.c_str(),
                                           lens_model.c_str(), lens_flags);
    const lfLens **lenses2 = db->FindLenses(NULL, NULL, lens_model.c_str(), lens_flags);
    if (lenses)
        lfLens(*lenses[0]).Check();

    lf_free(lenses2);
    lf_free(lenses);
    lf_free(cams2);
    lf_free(cams);
    delete db;
    return 0;
}
