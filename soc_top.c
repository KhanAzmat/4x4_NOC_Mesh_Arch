#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c0_master/c0_controller.h"
#include "platform_init/system_setup.h"
#include "mesh_noc/mesh_router.h" /* include implementation */


void test_plic_functionality(mesh_platform_t* platform);

int main(int argc, char** argv)
{
    if (getenv("TRACE")) noc_trace_enabled = 1;
    mesh_platform_t platform = {0};
    platform_setup(&platform);
    
    // printf("\n=== Testing Enhanced PLIC System ===\n");
    
    // // Test 1: Legacy PLIC compatibility
    // printf("\n--- Legacy PLIC Test ---\n");
    // test_plic_functionality(&platform);
    
    // // Test 2: Enhanced bidirectional communication demo
    // printf("\n--- Enhanced Bidirectional PLIC Demo ---\n");
    // demo_bidirectional_plic_communication(&platform);
    
    // // Optional: Run full test suite
    c0_run_test_suite(&platform);
    
    // printf("\n=== PLIC System Tests Complete ===\n");
    return 0;
}
