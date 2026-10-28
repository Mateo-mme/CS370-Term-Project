/* Software-only: verifies the test harness and that core modules link. */
#include "gpio_backend.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    gpio_backend *b = gpio_mock_open();
    assert(b);
    b->close(b);
    puts("test_smoke: ok");
    return 0;
}
