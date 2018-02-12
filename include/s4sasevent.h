/**
 * @file s4sasevent.h S4-specific SAS event IDs
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef S4SASEVENT_H__
#define S4SASEVENT_H__

#include "sasevent.h"

namespace SASEvent
{
  //----------------------------------------------------------------------------
  // S4 events.
  //----------------------------------------------------------------------------
  const int REGSTORE_GET_FOUND = S4_BASE + 0x000000;
  const int REGSTORE_GET_NEW = S4_BASE + 0x000001;
  const int REGSTORE_GET_FAILURE = S4_BASE + 0x000002;
  const int REGSTORE_SET_START = S4_BASE + 0x000003;
  const int REGSTORE_SET_SUCCESS = S4_BASE + 0x000004;
  const int REGSTORE_SET_FAILURE = S4_BASE + 0x000005;
  const int REGSTORE_DESERIALIZATION_FAILED = S4_BASE + 0x000006;

} //namespace SASEvent

#endif

