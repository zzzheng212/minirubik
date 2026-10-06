/* Host-only validation harness and iterative IDA* prototype, not target RV32I code.
 * Cube convention follows sysprog21/minirubik solver.c.
 * Build: gcc -std=c99 -O2 -Wall -Wextra -Wpedantic heuristic_test.c -o heuristic_test
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum
{
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};
enum
{
    UNVISITED = UINT8_MAX
};
typedef struct
{
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6}, {0, 1, 2, 4, 5, 6, 3}, {0, 2, 5, 3, 1, 4, 6}};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0}, {0, 0, 0, 1, 2, 1, 2}, {0, 0, 0, 0, 0, 0, 0}};
static const uint8_t inverse[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
static uint16_t permutation[3][PERMUTATIONS];
static uint16_t orientation[3][ORIENTATIONS];
static uint8_t dist_p[PERMUTATIONS], dist_o[ORIENTATIONS];

static state_t quarter_turn(state_t s, unsigned face)
{
    state_t result;
    for (unsigned i = 0; i < CUBIES; ++i)
    {
        unsigned from = source[face][i];
        result.p[i] = s.p[from];
        result.o[i] = (uint8_t)((s.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t s, unsigned move)
{
    for (unsigned n = 0; n < move % 3U + 1U; ++n)
        s = quarter_turn(s, move / 3U);
    return s;
}

static int valid(const state_t *s)
{
    unsigned sum = 0, seen = 0;
    for (unsigned i = 0; i < CUBIES; ++i)
    {
        if (s->p[i] >= CUBIES || s->o[i] >= 3)
            return 0;
        unsigned bit = 1U << s->p[i];
        if (seen & bit)
            return 0;
        seen |= bit;
        sum += s->o[i];
    }
    return sum % 3U == 0;
}

static uint32_t rank_state(const state_t *s)
{
    uint32_t p = 0, o = 0;
    for (unsigned i = 0; i < CUBIES; ++i)
    {
        unsigned smaller = 0;
        for (unsigned j = i + 1; j < CUBIES; ++j)
            if (s->p[j] < s->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (unsigned i = 0; i < 6; ++i)
        o = o * 3U + s->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *s)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    unsigned sum = 0;
    for (unsigned i = 0; i < CUBIES; ++i)
    {
        unsigned q = p / f;
        p %= f;
        s->p[i] = available[q];
        for (unsigned j = q; j + 1 < CUBIES - i; ++j)
            available[j] = available[j + 1];
        if (i < 5)
            f /= 6U - i;
    }
    for (int i = 5; i >= 0; --i)
    {
        s->o[i] = (uint8_t)(o % 3U);
        sum += s->o[i];
        o /= 3U;
    }
    s->o[6] = (uint8_t)((3U - sum % 3U) % 3U);
}

static int check_encoding(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    for (unsigned move = 0; move < MOVES; ++move)
    {
        state_t s = apply_move(apply_move(solved, move), inverse[move]);
        if (memcmp(s.p, solved.p, sizeof s.p) ||
            memcmp(s.o, solved.o, sizeof s.o))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank)
    {
        state_t s;
        unrank_state(rank, &s);
        if (!valid(&s) || rank_state(&s) != rank)
        {
            fprintf(stderr, "Encoding failure at rank %u\n", (unsigned)rank);
            return 0;
        }
    }
    puts("PASS: move inversion from solved and all encoding round trips");
    return 1;
}

static void build_transitions(void)
{
    /* A missed write must not look like the valid solved rank zero. */
    memset(permutation, 0xff, sizeof permutation);
    memset(orientation, 0xff, sizeof orientation);
    state_t s;
    for (unsigned p = 0; p < PERMUTATIONS; ++p)
    {
        unrank_state(p * ORIENTATIONS, &s);
        for (unsigned face = 0; face < 3; ++face)
        {
            state_t next = quarter_turn(s, face);
            permutation[face][p] = (uint16_t)(rank_state(&next) / ORIENTATIONS);
        }
    }
    for (unsigned o = 0; o < ORIENTATIONS; ++o)
    {
        unrank_state(o, &s);
        for (unsigned face = 0; face < 3; ++face)
        {
            state_t next = quarter_turn(s, face);
            orientation[face][o] = (uint16_t)(rank_state(&next) % ORIENTATIONS);
        }
    }
}

/* H2: every transition entry is populated, bounded, and matches the
 * cubie-level operation. This reference shares the source/twist convention.
 */
static int check_transitions(void)
{
    const char *names[3] = {"R", "B", "D"};
    unsigned maximum_p = 0, maximum_o = 0;
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    for (unsigned face = 0; face < 3; ++face)
    {
        for (unsigned p = 0; p < PERMUTATIONS; ++p)
        {
            unsigned actual = permutation[face][p];
            if (actual == UINT16_MAX || actual >= PERMUTATIONS)
            {
                fprintf(stderr, "H2 FAIL: permutation[%u][%u] out of range\n", face, p);
                return 0;
            }
            state_t s;
            unrank_state(p * ORIENTATIONS, &s);
            s = quarter_turn(s, face);
            if (actual != rank_state(&s) / ORIENTATIONS)
            {
                fprintf(stderr, "H2 FAIL: permutation[%u][%u] mismatch\n", face, p);
                return 0;
            }
            if (actual > maximum_p)
                maximum_p = actual;
        }
        for (unsigned o = 0; o < ORIENTATIONS; ++o)
        {
            unsigned actual = orientation[face][o];
            if (actual == UINT16_MAX || actual >= ORIENTATIONS)
            {
                fprintf(stderr, "H2 FAIL: orientation[%u][%u] out of range\n", face, o);
                return 0;
            }
            state_t s;
            unrank_state(o, &s);
            s = quarter_turn(s, face);
            if (actual != rank_state(&s) % ORIENTATIONS)
            {
                fprintf(stderr, "H2 FAIL: orientation[%u][%u] mismatch\n", face, o);
                return 0;
            }
            if (actual > maximum_o)
                maximum_o = actual;
        }
        state_t moved = quarter_turn(solved, face);
        uint32_t expected = rank_state(&moved);
        if (permutation[face][0] != expected / ORIENTATIONS ||
            orientation[face][0] != expected % ORIENTATIONS)
            return 0;
        printf("H2 solved transition %s: permutation=%u, orientation=%u\n",
               names[face], (unsigned)permutation[face][0],
               (unsigned)orientation[face][0]);
    }
    if (maximum_p != PERMUTATIONS - 1 || maximum_o != ORIENTATIONS - 1)
        return 0;
    printf("H2 PASS: permutation transitions, entries=%u, maximum=%u\n",
           3U * PERMUTATIONS, maximum_p);
    printf("H2 PASS: orientation transitions, entries=%u, maximum=%u\n",
           3U * ORIENTATIONS, maximum_o);
    return 1;
}

/* Same BFS for either projection. `step` has three rows of `count` entries. */
static int build_distances(size_t count, const uint16_t step[3][count],
                           uint8_t dist[count])
{
    uint16_t *queue = malloc(count * sizeof *queue);
    if (!queue)
        return 0;
    size_t head = 0, tail = 1;
    memset(dist, UNVISITED, count);
    dist[0] = 0;
    queue[0] = 0;
    while (head < tail)
    {
        uint16_t here = queue[head++];
        for (unsigned face = 0; face < 3; ++face)
        {
            uint16_t next = here;
            for (unsigned turn = 0; turn < 3; ++turn)
            {
                next = step[face][next];
                if (dist[next] != UNVISITED)
                    continue;
                if (tail >= count)
                {
                    free(queue);
                    return 0;
                }
                dist[next] = (uint8_t)(dist[here] + 1U);
                queue[tail++] = next;
            }
        }
    }
    free(queue);
    return tail == count;
}

static int check_distances(const char *name, size_t count,
                           const uint16_t step[3][count], const uint8_t *dist)
{
    unsigned maximum = 0;
    if (dist[0] != 0)
        return 0;
    for (size_t s = 0; s < count; ++s)
    {
        if (dist[s] == UNVISITED || (s && dist[s] == 0))
            return 0;
        int descending = (s == 0);
        if (dist[s] > maximum)
            maximum = dist[s];
        for (unsigned face = 0; face < 3; ++face)
        {
            uint16_t next = (uint16_t)s;
            for (unsigned turn = 0; turn < 3; ++turn)
            {
                next = step[face][next];
                int delta = (int)dist[s] - (int)dist[next];
                if (delta < -1 || delta > 1)
                    return 0;
                if (delta == 1)
                    descending = 1;
            }
        }
        if (!descending)
            return 0;
    }
    printf("PASS: %s, %zu entries, solved=0, maximum=%u, edge checks OK\n",
           name, count, maximum);
    return 1;
}

/* Full-state BFS oracle: distances, NOT inverse move numbers.
 * This large allocation is for host validation only.
 */
static uint8_t *build_exact_distances(void)
{
    uint8_t *dist = malloc(STATES);
    uint32_t *queue = malloc((size_t)STATES * sizeof *queue);
    if (!dist || !queue)
    {
        free(dist);
        free(queue);
        return NULL;
    }
    uint32_t head = 0, tail = 1;
    memset(dist, UNVISITED, STATES);
    dist[0] = 0;
    queue[0] = 0;
    while (head < tail)
    {
        uint32_t here = queue[head++];
        unsigned p = here / ORIENTATIONS, o = here % ORIENTATIONS;
        for (unsigned face = 0; face < 3; ++face)
        {
            unsigned np = p, no = o;
            for (unsigned turn = 0; turn < 3; ++turn)
            {
                np = permutation[face][np];
                no = orientation[face][no];
                uint32_t next = np * ORIENTATIONS + no;
                if (dist[next] != UNVISITED)
                    continue;
                if (tail >= STATES)
                {
                    free(dist);
                    free(queue);
                    return NULL;
                }
                dist[next] = (uint8_t)(dist[here] + 1U);
                queue[tail++] = next;
            }
        }
    }
    free(queue);
    if (tail != STATES)
    {
        free(dist);
        return NULL;
    }
    return dist;
}

static int check_heuristic(const uint8_t *exact)
{
    unsigned diameter = 0, max_gap = 0;
    uint32_t equal = 0, depth11 = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank)
    {
        unsigned p = rank / ORIENTATIONS, o = rank % ORIENTATIONS;
        unsigned h = dist_p[p] > dist_o[o] ? dist_p[p] : dist_o[o];
        if (exact[rank] == UNVISITED || h > exact[rank])
        {
            fprintf(stderr, "FAIL at rank=%u: h=%u, exact=%u\n",
                    (unsigned)rank, h, (unsigned)exact[rank]);
            return 0;
        }
        if (h == exact[rank])
            ++equal;
        if (exact[rank] == 11)
            ++depth11;
        if (exact[rank] > diameter)
            diameter = exact[rank];
        if (exact[rank] - h > max_gap)
            max_gap = exact[rank] - h;
    }
    const state_t sample = {{1, 0, 2, 3, 4, 5, 6}, {0}};
    unsigned sample_d = exact[rank_state(&sample)];
    printf("Full BFS: %u states, diameter=%u, distance-11 states=%u\n",
           (unsigned)STATES, diameter, (unsigned)depth11);
    printf("Vector 21345671111111: exact distance=%u\n", sample_d);
    if (diameter != 11 || depth11 != 2644 || sample_d != 11)
        return 0;
    printf("PASS: h <= exact distance for all %u states\n", (unsigned)STATES);
    printf("Heuristic equals exact distance: %u states (%.2f%%)\n",
           (unsigned)equal, 100.0 * equal / STATES);
    printf("Largest exact-distance minus heuristic gap: %u\n", max_gap);
    return 1;
}

enum
{
    MAX_DEPTH = 11
};
typedef struct
{
    uint8_t path[MAX_DEPTH];
    unsigned length, iterations;
    uint64_t candidates;
} solution_t;

static unsigned heuristic(unsigned p, unsigned o)
{
    return dist_p[p] > dist_o[o] ? dist_p[p] : dist_o[o];
}

/* Explicit DFS frames: ranks and the next move to try at each depth.
 * The exact-distance oracle is deliberately NOT an input to this search.
 */
static int solve_ida(uint16_t root_p, uint16_t root_o, solution_t *answer)
{
    uint16_t stack_p[MAX_DEPTH + 1], stack_o[MAX_DEPTH + 1];
    /* Parent-level intermediates survive searches in deeper frames. */
    uint16_t next_p[MAX_DEPTH + 1], next_o[MAX_DEPTH + 1];
    uint8_t next_move[MAX_DEPTH + 1];
    unsigned bound = heuristic(root_p, root_o);
    memset(answer, 0, sizeof *answer);
    while (bound <= MAX_DEPTH)
    {
        unsigned depth = 0, next_bound = UINT8_MAX;
        ++answer->iterations;
        stack_p[0] = root_p;
        stack_o[0] = root_o;
        next_move[0] = 0;
        for (;;)
        {
            if (stack_p[depth] == 0 && stack_o[depth] == 0)
            {
                answer->length = depth;
                return 1;
            }
            if (next_move[depth] == MOVES || depth == MAX_DEPTH)
            {
                if (depth == 0)
                    break;
                --depth;
                continue;
            }
            unsigned move = next_move[depth]++;
            unsigned face = move / 3U;
            /* Consecutive moves on one face combine or cancel in HTM. */
            if (depth && answer->path[depth - 1] / 3U == face)
            {
                /* Moves are grouped in triples. Skip this entire face. */
                next_move[depth] = (uint8_t)((face + 1U) * 3U);
                continue;
            }
            if (move % 3U == 0)
            {
                next_p[depth] = stack_p[depth];
                next_o[depth] = stack_o[depth];
            }
            next_p[depth] = permutation[face][next_p[depth]];
            next_o[depth] = orientation[face][next_o[depth]];
            unsigned p = next_p[depth], o = next_o[depth];
            ++answer->candidates;
            unsigned f = depth + 1U + heuristic(p, o);
            if (f > bound)
            {
                if (f < next_bound)
                    next_bound = f;
                continue;
            }
            answer->path[depth] = (uint8_t)move;
            ++depth;
            stack_p[depth] = (uint16_t)p;
            stack_o[depth] = (uint16_t)o;
            next_move[depth] = 0;
        }
        if (next_bound == UINT8_MAX)
            return 0;
        bound = next_bound;
    }
    return 0;
}

static int verify_solution(uint32_t rank, const uint8_t *exact, solution_t *s)
{
    if (!solve_ida((uint16_t)(rank / ORIENTATIONS),
                   (uint16_t)(rank % ORIENTATIONS), s))
    {
        fprintf(stderr, "FAIL: IDA* found no solution for rank %u\n", (unsigned)rank);
        return 0;
    }
    state_t replay;
    unrank_state(rank, &replay);
    for (unsigned i = 0; i < s->length; ++i)
    {
        if (s->path[i] >= MOVES)
            return 0;
        replay = apply_move(replay, s->path[i]);
    }
    if (!valid(&replay) || rank_state(&replay) != 0 || s->length != exact[rank])
    {
        fprintf(stderr, "FAIL: replay/optimality for rank %u\n", (unsigned)rank);
        return 0;
    }
    return 1;
}

static int parse_input(const char *input, state_t *s)
{
    if (strlen(input) != 14)
        return 0;
    for (unsigned i = 0; i < CUBIES; ++i)
    {
        if (input[i] < '1' || input[i] > '7' ||
            input[i + CUBIES] < '1' || input[i + CUBIES] > '3')
            return 0;
        s->p[i] = (uint8_t)(input[i] - '1');
        s->o[i] = (uint8_t)(input[i + CUBIES] - '1');
    }
    return valid(s);
}

static int test_search(const uint8_t *exact, int exhaustive)
{
    solution_t answer;
    state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t short_state = apply_move(apply_move(solved, 0), 3);
    state_t hard = {{1, 0, 2, 3, 4, 5, 6}, {0}};
    uint32_t fixed[] = {0, rank_state(&short_state), rank_state(&hard)};
    for (unsigned i = 0; i < 3; ++i)
        if (!verify_solution(fixed[i], exact, &answer))
            return 0;
    unsigned count = exhaustive ? STATES : 128;
    uint64_t hard_sum = 0, hard_min = UINT64_MAX, hard_max = 0;
    uint64_t overall_max = 0;
    uint32_t hard_count = 0, hard_rank = 0, overall_rank = 0;
    uint32_t seed = 20261004;
    for (unsigned i = 0; i < count; ++i)
    {
        seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
        uint32_t rank = exhaustive ? i : seed % STATES;
        if (!verify_solution(rank, exact, &answer))
            return 0;
        if (answer.candidates > overall_max)
        {
            overall_max = answer.candidates;
            overall_rank = rank;
        }
        if (exact[rank] == 11)
        {
            ++hard_count;
            hard_sum += answer.candidates;
            if (answer.candidates < hard_min)
                hard_min = answer.candidates;
            if (answer.candidates > hard_max)
            {
                hard_max = answer.candidates;
                hard_rank = rank;
            }
        }
        if (exhaustive && (i + 1U) % 10000U == 0)
        {
            printf("H3 progress: %u / %u\n", i + 1U, (unsigned)STATES);
            fflush(stdout);
        }
    }
    printf("PASS: IDA* replay and optimal length, 3 fixed + %u %s cases\n",
           count, exhaustive ? "exhaustive" : "deterministic sampled");
    printf("Maximum generated candidates: %llu at rank %u\n",
           (unsigned long long)overall_max, (unsigned)overall_rank);
    if (hard_count)
    {
        state_t worst;
        unrank_state(hard_rank, &worst);
        printf("Distance-11 statistics: count=%u, min=%llu, mean=%.2f, max=%llu\n",
               (unsigned)hard_count, (unsigned long long)hard_min,
               (double)hard_sum / hard_count, (unsigned long long)hard_max);
        printf("Worst distance-11 rank=%u, input=", (unsigned)hard_rank);
        for (unsigned j = 0; j < CUBIES; ++j)
            printf("%u", worst.p[j] + 1U);
        for (unsigned j = 0; j < CUBIES; ++j)
            printf("%u", worst.o[j] + 1U);
        putchar('\n');
    }
    return 1;
}

int main(int argc, char **argv)
{
    int exhaustive = argc == 2 && strcmp(argv[1], "--exhaustive") == 0;
    state_t input;
    if (argc > 2 || (argc == 2 && !exhaustive && !parse_input(argv[1], &input)))
    {
        fputs("usage: heuristic_test [PPPPPPPOOOOOOO | --exhaustive]\n", stderr);
        return 2;
    }
    clock_t begin = clock();
    if (!check_encoding())
    {
        fputs("FAIL: encoding tests\n", stderr);
        return 1;
    }
    build_transitions();
    if (!check_transitions())
    {
        fputs("FAIL: H2 transition checks\n", stderr);
        return 1;
    }
    /* Explicit const-array pointer casts avoid pre-C23 qualifier warnings. */
    const uint16_t (*pt)[PERMUTATIONS] =
        (const uint16_t (*)[PERMUTATIONS])permutation;
    const uint16_t (*ot)[ORIENTATIONS] =
        (const uint16_t (*)[ORIENTATIONS])orientation;
    if (!build_distances(PERMUTATIONS, pt, dist_p) ||
        !build_distances(ORIENTATIONS, ot, dist_o) ||
        !check_distances("permutation", PERMUTATIONS, pt, dist_p) ||
        !check_distances("orientation", ORIENTATIONS, ot, dist_o))
    {
        fputs("FAIL: abstract distance tables\n", stderr);
        return 1;
    }
    puts("Building host-only full BFS oracle...");
    fflush(stdout);
    uint8_t *exact = build_exact_distances();
    if (!exact)
    {
        fputs("FAIL: full BFS or allocation\n", stderr);
        return 1;
    }
    int ok = check_heuristic(exact);
    if (ok)
        ok = test_search(exact, exhaustive);
    if (ok && argc == 2 && !exhaustive)
    {
        solution_t answer;
        ok = verify_solution(rank_state(&input), exact, &answer);
        if (ok)
        {
            static const char *const names[MOVES] =
                {"R", "R2", "R'", "B", "B2", "B'", "D", "D2", "D'"};
            printf("Input: %s\nSolution (%u HTM moves):", argv[1], answer.length);
            for (unsigned i = 0; i < answer.length; ++i)
                printf(" %s", names[answer.path[i]]);
            printf("\nIDA* iterations=%u, generated candidates=%llu\n",
                   answer.iterations, (unsigned long long)answer.candidates);
            puts("PASS: requested solution reaches solved and matches BFS distance");
        }
    }
    free(exact);
    if (!ok)
    {
        fputs("FAIL: validation\n", stderr);
        return 1;
    }
    printf("Table bytes: transitions=%zu, heuristic=%zu\n",
           sizeof permutation + sizeof orientation, sizeof dist_p + sizeof dist_o);
    printf("C clock() elapsed: %.3f seconds (not a Ripes measurement)\n",
           (double)(clock() - begin) / CLOCKS_PER_SEC);
    puts("ALL REQUESTED TESTS PASSED (target performance not tested)");
    return fflush(stdout) != 0 || ferror(stdout);
}
