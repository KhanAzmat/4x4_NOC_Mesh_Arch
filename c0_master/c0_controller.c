#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "c0_controller.h"
#include "hal_tests/test_framework.h"

void c0_run_test_suite(mesh_platform_t* platform)
{
    printf("\n== Mesh‑NoC HAL Validation ==\n");
    run_all_tests(platform);
}
