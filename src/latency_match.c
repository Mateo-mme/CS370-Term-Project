#include "latency_match.h"

void latency_match(const stim_edge *stim, size_t n_stim, const recv_event *ev, size_t n_ev,
                   int64_t *latency_ns, size_t *match_idx, match_result *res)
{
    *res = (match_result){0};
    size_t j = 0;

    while (n_stim > 0 && j < n_ev && ev[j].handled_ns < stim[0].t_ns) {
        res->early++;
        j++;
    }

    for (size_t i = 0; i < n_stim; i++) {
        uint64_t window_end = i + 1 < n_stim ? stim[i + 1].t_ns : UINT64_MAX;
        latency_ns[i] = -1;
        if (match_idx)
            match_idx[i] = SIZE_MAX;

        /* Events in this window that don't match are spurious, except the
         * first same-level one, which is the response. */
        while (j < n_ev && ev[j].handled_ns < window_end) {
            if (latency_ns[i] < 0 && ev[j].level == stim[i].level &&
                ev[j].handled_ns >= stim[i].t_ns) {
                latency_ns[i] = (int64_t)(ev[j].handled_ns - stim[i].t_ns);
                if (match_idx)
                    match_idx[i] = j;
                res->matched++;
            } else {
                res->spurious++;
            }
            j++;
        }
        if (latency_ns[i] < 0)
            res->missed++;
    }
    res->spurious += n_ev - j;
}
