/* AI-assisted validation support, compiled only for the host. */
#define main student_main
#include "source/test_modified.c"
#undef main

int main(void)
{
    if (!check_encoding())
        return 1;
    build_transitions();
    const uint16_t (*pt)[PERMUTATIONS] =
        (const uint16_t (*)[PERMUTATIONS]) permutation;
    const uint16_t (*ot)[ORIENTATIONS] =
        (const uint16_t (*)[ORIENTATIONS]) orientation;
    if (!build_distances(PERMUTATIONS, pt, dist_p) ||
        !build_distances(ORIENTATIONS, ot, dist_o))
        return 1;
    if (!check_distances("permutation", PERMUTATIONS, pt, dist_p) ||
        !check_distances("orientation", ORIENTATIONS, ot, dist_o))
        return 1;
    unsigned maxp = 0, maxo = 0;
    for (unsigned f = 0; f < 3; ++f) {
        for (unsigned p = 0; p < PERMUTATIONS; ++p) {
            unsigned n = permutation[f][p];
            if (n >= PERMUTATIONS)
                return 1;
            if (n > maxp)
                maxp = n;
            unsigned r = p;
            for (unsigned k = 0; k < 4; ++k)
                r = permutation[f][r];
            if (r != p)
                return 1;
        }
        for (unsigned o = 0; o < ORIENTATIONS; ++o) {
            unsigned n = orientation[f][o];
            if (n >= ORIENTATIONS)
                return 1;
            if (n > maxo)
                maxo = n;
            unsigned r = o;
            for (unsigned k = 0; k < 4; ++k)
                r = orientation[f][r];
            if (r != o)
                return 1;
        }
        printf("H2 solved transition face %u: p=%u o=%u\n", f,
               permutation[f][0], orientation[f][0]);
    }
    printf("H2 transition maxima p=%u o=%u\n", maxp, maxo);
    if (maxp != 5039 || maxo != 728)
        return 1;
    FILE *fp = fopen("host-tables.bin", "wb");
    if (!fp)
        return 1;
    if (fwrite(permutation, 1, sizeof permutation, fp) != sizeof permutation ||
        fwrite(orientation, 1, sizeof orientation, fp) != sizeof orientation ||
        fwrite(dist_p, 1, sizeof dist_p, fp) != sizeof dist_p ||
        fwrite(dist_o, 1, sizeof dist_o, fp) != sizeof dist_o)
        return 1;
    if (fclose(fp))
        return 1;
    uint8_t *exact = build_exact_distances();
    if (!exact || !check_heuristic(exact))
        return 1;
    fp = fopen("exact.bin", "wb");
    if (!fp || fwrite(exact, 1, STATES, fp) != STATES)
        return 1;
    free(exact);
    if (fclose(fp))
        return 1;
    puts(
        "HOST AUDIT PASS; assembly table byte comparison performed "
        "separately.");
    return 0;
}
