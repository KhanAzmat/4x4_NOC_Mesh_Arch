#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c0_master/c0_controller.h"
#include "platform_init/system_setup.h"
#include "mesh_noc/mesh_router.h" /* include implementation */

int main(int argc, char** argv)
{
    if (getenv("TRACE")) noc_trace_enabled = 1;
    mesh_platform_t platform = {0};
    platform_setup(&platform);
    c0_run_test_suite(&platform);
    return 0;
}
