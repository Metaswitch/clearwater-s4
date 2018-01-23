/**
 * @file s4_handlers.cpp
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
}
#include "s4_handlers.h"

void AoRTimeoutTask::process_aor_timeout(std::string aor_id)
{
  TRC_DEBUG("Handling timer pop for AoR id: %s", aor_id.c_str());
}
