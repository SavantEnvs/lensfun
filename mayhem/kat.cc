// Known-answer probe for mayhem/test.sh: distorts fixed coordinates with real lenses from the
// shipped database and prints them, so test.sh can compare exact values (from the upstream tests).
#include <cstdio>
#include <cstdlib>

#include "lensfun.h"

static int probe(lfDatabase *db, const char *query, float focal, float crop, const char *expect_model)
{
    const lfLens **lenses = db->FindLenses(NULL, NULL, query);
    if (!lenses)
        return 1;
    printf("model=%s\n", lenses[0]->Model);
    (void)expect_model;
    lfModifier mod(lenses[0], focal, crop, 1500, 1000, LF_PF_F32, false);
    mod.EnableDistortionCorrection();
    const float xs[] = {0, 751, 810, 1270};
    const float ys[] = {0, 497, 937, 100};
    for (int i = 0; i < 4; i++)
    {
        float c[2];
        if (!mod.ApplyGeometryDistortion(xs[i], ys[i], 1, 1, c))
            return 1;
        printf("dist %.3f %.3f -> %.3f %.3f\n", xs[i], ys[i], c[0], c[1]);
    }
    lf_free(lenses);
    return 0;
}

int main(int argc, char **argv)
{
    lfDatabase db;
    if (db.Load(argc > 1 ? argv[1] : "data/db") != LF_NO_ERROR)
        return 1;
    if (probe(&db, "pEntax 50-200 ED", 80.89f, 1.534f, NULL))
        return 1;
    if (probe(&db, "Canon PowerShot G12", 10.89f, 4.6f, NULL))
        return 1;
    return 0;
}
