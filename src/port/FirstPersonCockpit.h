#ifndef FIRST_PERSON_COCKPIT_H
#define FIRST_PERSON_COCKPIT_H

#include <libultraship.h>
#include <common_structs.h>
#include "camera.h"

#ifdef __cplusplus
extern "C" {
#endif

void FirstPersonCockpit_Init(void);
void FirstPersonCockpit_Render(Player* player, Camera* camera, s8 playerId, s8 screenId);

#ifdef __cplusplus
}
#endif

#endif // FIRST_PERSON_COCKPIT_H
