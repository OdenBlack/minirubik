/*
 * Host-side correctness checker for solver.c and IDA_solver.c.
 * Place this file beside both source files, then build with a C99 compiler.
 *
 * Modes:
 *   verify_ida --pdb   Check PDB coverage and solved entries (H2)
 *   verify_ida --h1    Also check heuristic admissibility for every state (H1)
 *   verify_ida --h3    Also compare every IDA* solution with exact BFS distance (H3)
 *   verify_ida --all   Run H1, H2, and H3
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verify_silent_printf(const char *format, ...);
static int verify_silent_putchar(int ch);

/* Include the reference solver in this translation unit to access its
 * static BFS table builder, ranking, unranking, and move functions.
 */
#define main oracle_solver_main
#include "solver.c"
#undef main

/* IDA_solver.c has several identifiers in common with solver.c. Rename its
 * identifiers while including it so both implementations can coexist here.
 * Silence only its per-state move printing; the checker prints its own report.
 */
#define C ida_C
#define O_STATES ida_O_STATES
#define P_STATES ida_P_STATES
#define MAX_DEPTH ida_MAX_DEPTH
#define MOVES ida_MOVES
#define state_t ida_state_t
#define o_pdb ida_o_pdb
#define p_pdb ida_p_pdb
#define solution_path ida_solution_path
#define move_names ida_move_names
#define source ida_source
#define twist ida_twist
#define quarter_turn ida_quarter_turn
#define rank_permutation ida_rank_permutation
#define rank_orientation ida_rank_orientation
#define heuristic ida_heuristic
#define init_p_pdb ida_init_p_pdb
#define init_o_pdb ida_init_o_pdb
#define parse_state ida_parse_state
#define is_solved ida_is_solved
#define ida_dfs ida_ida_dfs
#define solve_state ida_solve_state
#define self_test ida_self_test
#define main ida_embedded_main
#define printf verify_silent_printf
#define putchar verify_silent_putchar
#include "IDA_solver_improv_v3.c"
#undef putchar
#undef printf
#undef main
#undef self_test
#undef solve_state
#undef ida_dfs
#undef is_solved
#undef parse_state
#undef init_o_pdb
#undef init_p_pdb
#undef heuristic
#undef rank_orientation
#undef rank_permutation
#undef quarter_turn
#undef twist
#undef source
#undef move_names
#undef solution_path
#undef p_pdb
#undef o_pdb
#undef state_t
#undef MOVES
#undef MAX_DEPTH
#undef P_STATES
#undef O_STATES
#undef C

static int verify_silent_printf(const char *format, ...)
{
    (void) format;
    return 0;
}

static int verify_silent_putchar(int ch)
{
    (void) ch;
    return 0;
}

enum { MAX_ORACLE_DISTANCE = 20, MAX_REPORTED_FAILURES = 10 };

static uint8_t *exact_distance;

static ida_state_t to_ida_state(const state_t *state)
{
    ida_state_t result;
    memcpy(result.p, state->p, sizeof result.p);
    memcpy(result.o, state->o, sizeof result.o);
    return result;
}

/* Follow solver.c's BFS-generated toward-solved moves to recover the exact
 * distance for every rank. The BFS table itself stores moves, not distances.
 */
static int build_exact_distances(const uint8_t *toward_solved,
                                 uint8_t *distances,
                                 uint8_t *diameter)
{
    uint8_t max_distance = 0;
    state_t state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint32_t current_rank = rank;
        unsigned steps = 0;
        unrank_state(rank, &state);

        while (current_rank != 0) {
            uint8_t move;
            if (steps >= MAX_ORACLE_DISTANCE)
                return 0;
            move = toward_solved[current_rank];
            if (move >= MOVES)
                return 0;
            state = apply_move(state, move);
            current_rank = rank_state(&state);
            ++steps;
        }

        distances[rank] = (uint8_t) steps;
        if (steps > max_distance)
            max_distance = (uint8_t) steps;
    }

    *diameter = max_distance;
    return 1;
}

static int check_pdbs(void)
{
    unsigned p_unvisited = 0, o_unvisited = 0;
    unsigned p_max = 0, o_max = 0;

    if (ida_p_pdb[0] != 0 || ida_o_pdb[0] != 0) {
        fprintf(stderr, "H2 failed: a solved PDB entry is not zero\n");
        return 0;
    }

    for (unsigned i = 0; i < ida_P_STATES; ++i) {
        if (ida_p_pdb[i] == UINT8_MAX)
            ++p_unvisited;
        else if (ida_p_pdb[i] > p_max)
            p_max = ida_p_pdb[i];
    }
    for (unsigned i = 0; i < ida_O_STATES; ++i) {
        if (ida_o_pdb[i] == UINT8_MAX)
            ++o_unvisited;
        else if (ida_o_pdb[i] > o_max)
            o_max = ida_o_pdb[i];
    }

    printf("PDB permutation: %u entries, %u unvisited, max distance %u\n",
           (unsigned) ida_P_STATES, p_unvisited, p_max);
    printf("PDB orientation: %u entries, %u unvisited, max distance %u\n",
           (unsigned) ida_O_STATES, o_unvisited, o_max);

    if (p_unvisited || o_unvisited) {
        fprintf(stderr, "H2 failed: one or more PDB entries remain unvisited\n");
        return 0;
    }
    return 1;
}

static int check_h1(void)
{
    unsigned violations = 0;
    state_t oracle_state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint8_t h, d;
        ida_state_t state;
        unrank_state(rank, &oracle_state);
        state = to_ida_state(&oracle_state);
        h = ida_heuristic(&state);
        d = exact_distance[rank];
        if (h > d) {
            if (violations < MAX_REPORTED_FAILURES)
                fprintf(stderr, "H1 violation: rank=%u h=%u exact=%u\n",
                        (unsigned) rank, (unsigned) h, (unsigned) d);
            ++violations;
        }
    }

    if (violations) {
        fprintf(stderr, "H1 failed: %u inadmissible states\n", violations);
        return 0;
    }
    puts("H1 passed: h(state) <= exact distance for all states.");
    return 1;
}

static int check_h3(void)
{
    unsigned failures = 0;
    state_t oracle_state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        int found, expected = exact_distance[rank];
        ida_state_t state;
        unrank_state(rank, &oracle_state);
        state = to_ida_state(&oracle_state);
        found = ida_solve_state(state);

        if (found != expected) {
            if (failures < MAX_REPORTED_FAILURES)
                fprintf(stderr,
                        "H3 length mismatch: rank=%u got=%d exact=%d\n",
                        (unsigned) rank, found, expected);
            ++failures;
            continue;
        }

        /* Also apply the returned path, so the exhaustive H3 pass catches an
         * invalid path even if its reported length happens to be correct.
         */
        for (int i = 0; i < found; ++i) {
            uint8_t move = ida_solution_path[i];
            if (move >= ida_MOVES) {
                  ++failures;
                break;
            }
            state = ida_quarter_turn(state, (uint8_t) (move / 3U));
            for (uint8_t turn = 0; turn < (uint8_t) (move % 3U); ++turn)
                state = ida_quarter_turn(state, (uint8_t) (move / 3U));
        }
        if (!ida_is_solved(&state)) {
            if (failures < MAX_REPORTED_FAILURES)
                fprintf(stderr, "H3 invalid path: rank=%u\n", (unsigned) rank);
            ++failures;
        }

        if ((rank + 1U) % 100000U == 0)
            printf("H3 progress: %u / %u states\n",
                   (unsigned) (rank + 1U), (unsigned) STATES);
    }

    if (failures) {
        fprintf(stderr, "H3 failed: %u failing states\n", failures);
        return 0;
    }
    puts("H3 passed: every state has an optimal solution path.");
    return 1;
}

int main(int argc, char **argv)
{
    int run_h1 = 0, run_h3 = 0, ok = 1;
    uint8_t bfs_diameter = 0, oracle_diameter = 0;
    uint8_t *toward_solved;

    if (argc != 2 ||
        (strcmp(argv[1], "--pdb") && strcmp(argv[1], "--h1") &&
         strcmp(argv[1], "--h3") && strcmp(argv[1], "--all"))) {
        fprintf(stderr, "usage: %s --pdb|--h1|--h3|--all\n",
                argc > 0 && argv[0] ? argv[0] : "verify_ida");
        return 2;
    }
    run_h1 = !strcmp(argv[1], "--h1") || !strcmp(argv[1], "--all");
    run_h3 = !strcmp(argv[1], "--h3") || !strcmp(argv[1], "--all");

    ida_init_o_pdb();
    ida_init_p_pdb();
    if (!check_pdbs())
        ok = 0;

    if (run_h1 || run_h3) {
        toward_solved = build_table(&bfs_diameter);
        if (!toward_solved) {
            fprintf(stderr, "Could not build the reference BFS table.\n");
            return 1;
        }
        if (bfs_diameter != 11) {
            fprintf(stderr, "Reference BFS diameter was %u, expected 11.\n",
                    (unsigned) bfs_diameter);
            ok = 0;
        }

        exact_distance = (uint8_t *) malloc(STATES);
        if (!exact_distance) {
            fprintf(stderr, "Could not allocate exact-distance array.\n");
            free(toward_solved);
            return 1;
        }
        if (!build_exact_distances(toward_solved, exact_distance,
                                   &oracle_diameter)) {
            fprintf(stderr, "Could not derive distances from the BFS table.\n");
            free(exact_distance);
            free(toward_solved);
            return 1;
        }
        free(toward_solved);

        printf("Oracle: %u states, diameter %u\n",
               (unsigned) STATES, (unsigned) oracle_diameter);
        if (oracle_diameter != 11 || oracle_diameter != bfs_diameter) {
            fprintf(stderr, "Oracle distance check failed.\n");
            ok = 0;
        }
    }

    if (run_h1 && !check_h1())
        ok = 0;
    if (run_h3 && !check_h3())
        ok = 0;

    free(exact_distance);
    return ok ? 0 : 1;
}
