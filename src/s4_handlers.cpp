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

#include "s4_handlers.h"

void AoRTimeoutTask::process_aor_timeout(const std::string& aor_id)
{
  TRC_DEBUG("Handling timer pop for AoR id: %s", aor_id.c_str());

  return _cfg->_s4->handle_timer_pop(aor_id, trail());
}
