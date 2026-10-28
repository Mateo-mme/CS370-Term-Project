/* Mocked-hardware: the mock backend reflects writes in reads and reports no
 * edges, without touching any GPIO. */
#include "gpio_backend.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    gpio_backend *b = gpio_mock_open();
    assert(b);

    assert(b->read(b, PIN_PIR) == 0);
    assert(b->write(b, PIN_TRIG, 1) == 0);
    assert(b->read(b, PIN_TRIG) == 1);
    assert(b->read(b, 1000) == -1);

    gpio_edge e;
    assert(b->wait_edge(b, 0, &e) == 0);

    b->close(b);
    puts("test_mock_backend: ok");
    return 0;
}
