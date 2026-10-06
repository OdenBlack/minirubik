/*
 * Host-side exporter for every state whose exact HTM distance is 11.
 * Place this file beside the original solver.c, then compile this file only:
 *   gcc -O3 -std=c99 -Wall -Wextra -Wpedantic export_distance11.c -o export_distance11.exe
 *   ./export_distance11.exe distance11_states.txt
 */

/* Include the oracle in this translation unit to access its static helpers. */
#define main oracle_solver_main
#include "solver.c"
#undef main

/* Write the external format: seven cubie digits, then seven orientation digits. */
static void state_to_input(const state_t *state, char text[15])
{
    for (int i = 0; i < CUBIES; ++i) {
        text[i] = (char)('1' + state->p[i]);
        text[i + CUBIES] = (char)('1' + state->o[i]);
    }
    text[14] = '\0';
}

/* Return 1 on success, or 0 if building, following, or writing the table fails. */
static int export_distance11_states(const char *filename)
{
    enum { TARGET_DISTANCE = 11, EXPECTED_COUNT = 2644 };
    uint8_t diameter = 0;
    uint8_t *toward_solved = build_table(&diameter);
    FILE *fp = NULL;
    unsigned exported = 0;
    int ok = 0;

    if (!toward_solved) {
        fputs("Could not build the reference BFS table.\n", stderr);
        return 0;
    }
    if (diameter != TARGET_DISTANCE) {
        fprintf(stderr, "BFS diameter is %u, expected 11.\n",
                (unsigned)diameter);
        goto cleanup;
    }

    fp = fopen(filename, "w");
    if (!fp) {
        perror(filename);
        goto cleanup;
    }

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        state_t initial, current;
        uint32_t current_rank = rank;
        unsigned distance = 0;

        unrank_state(rank, &initial);
        current = initial;

        /* Each BFS-recorded move reduces the exact distance by one. */
        while (current_rank != 0) {
            uint8_t move = toward_solved[current_rank];
            if (move >= MOVES || distance >= diameter) {
                fprintf(stderr, "Invalid oracle path at rank %u.\n",
                        (unsigned)rank);
                goto cleanup;
            }
            current = apply_move(current, move);
            current_rank = rank_state(&current);
            ++distance;
        }

        if (distance == TARGET_DISTANCE) {
            char text[15];
            state_to_input(&initial, text);
            if (fprintf(fp, "%s\n", text) < 0) {
                fputs("Could not write the state list.\n", stderr);
                goto cleanup;
            }
            ++exported;
        }
    }

    if (fclose(fp) != 0) {
        fp = NULL;
        fputs("Could not finish writing the state list.\n", stderr);
        goto cleanup;
    }
    fp = NULL;

    if (exported != EXPECTED_COUNT) {
        fprintf(stderr, "Exported %u states, expected 2644.\n", exported);
        goto cleanup;
    }

    printf("Oracle: %u states, diameter %u\n", (unsigned)STATES,
           (unsigned)diameter);
    printf("Exported %u distance-11 states to %s\n", exported, filename);
    ok = !output_failed();

cleanup:
    if (fp && fclose(fp) != 0)
        fputs("Could not close the output file.\n", stderr);
    free(toward_solved);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr, "usage: %s [output.txt]\n", argv[0]);
        return 2;
    }
    return export_distance11_states(argc == 2 ? argv[1] : "distance11_states.txt")
               ? 0
               : 1;
}
