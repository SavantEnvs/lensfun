// Fuzzes lensfun's XML database parser (lfDatabase::Load from memory, GMarkup based), then walks
// every parsed camera/lens/mount through the library's validators, searches and the XML writer.
#include <cstddef>
#include <cstdint>

#include "lensfun.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    lfDatabase *db = new lfDatabase();
    lfError err = db->Load(reinterpret_cast<const char *>(data), size);
    if (err != LF_NO_ERROR)
    {
        delete db;
        return -1;
    }

    const lfCamera *const *cameras = db->GetCameras();
    for (int i = 0; cameras && cameras[i] && i < 16; i++)
    {
        lfCamera copy(*cameras[i]);
        copy.Check();
        const lfCamera **found = db->FindCamerasExt(cameras[i]->Maker, cameras[i]->Model);
        lf_free(found);
        found = db->FindCameras(cameras[i]->Maker, cameras[i]->Model);
        lf_free(found);
    }

    const lfLens *const *lenses = db->GetLenses();
    for (int i = 0; lenses && lenses[i] && i < 16; i++)
    {
        lfLens copy(*lenses[i]);
        copy.Check();
        copy.GuessParameters();
        const lfLens **found = db->FindLenses(cameras ? cameras[0] : NULL, lenses[i]->Maker,
                                              lenses[i]->Model, LF_SEARCH_LOOSE);
        lf_free(found);
    }

    const lfMount *const *mounts = db->GetMounts();
    for (int i = 0; mounts && mounts[i] && i < 16; i++)
    {
        const lfMount *m = db->FindMount(mounts[i]->Name);
        if (m)
            db->MountName(m->Name);
    }

    char *xml = NULL;
    size_t xml_size = 0;
    if (db->Save(xml, xml_size) == LF_NO_ERROR)
    {
        lfDatabase *db2 = new lfDatabase();
        db2->Load(xml, xml_size);
        delete db2;
    }
    lf_free(xml);

    delete db;
    return 0;
}
