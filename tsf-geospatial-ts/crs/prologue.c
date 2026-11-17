/** @file
 * @brief crs group prologue
 *
 * Nothing to set up or tear down: each test opens its own RPC server
 * and scratch directory, so a test can run alone.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "crs/prologue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("crs group prologue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
