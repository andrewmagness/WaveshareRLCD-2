#ifndef SPORTS_SCORES_H
#define SPORTS_SCORES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dashboard_screen.h"

void sports_scores_init(void);
int sports_scores_get_games(game_info_t *games, int max_to_get);

#ifdef __cplusplus
}
#endif

#endif // SPORTS_SCORES_H
