

#include "s_bridge/shared_defs.h"

void delete_user_app(user_app_t *ua) {
    for (size_t i = 0; i < FOS_MAX_APP_AREAS; i++) {
        if (ua->areas[i].occupied && ua->areas[i].given_size > 0) {
            al_free(ua->al, (void *)(ua->areas[i].given));
        }
    }

    if (ua->args_block_size > 0) {
        al_free(ua->al, (void *)(ua->args_block));
    }

    al_free(ua->al, ua);
}
