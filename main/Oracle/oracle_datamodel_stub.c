#include "diagnostic_handler.h"
#include "robin_types.h"

#include <string.h>

static Robin_container_t g_robin_stub;
Robin_container_t *robin __attribute__ ((weak)) = &g_robin_stub;
Diagnostic_Container_t diag __attribute__ ((weak, aligned (4)));

static void __attribute__ ((constructor))
oracle_stub_init (void)
{
    memset (&g_robin_stub, 0, sizeof (g_robin_stub));
    memset (&diag, 0, sizeof (diag));
}
