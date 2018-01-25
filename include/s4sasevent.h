/**
 * @file s4sasevent.h S4-specific SAS event IDs
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef S4SASEVENT_H__
#define S4SASEVENT_H__

#include "sasevent.h"

// @todo Define S$_BASE in cpp-common and renumber the events in the resource
// bundle.
const int S4_BASE = 0x810000;

namespace SASEvent
{
  //----------------------------------------------------------------------------
  // S4 events.
  //----------------------------------------------------------------------------
  const int REGSTORE_GET_FOUND = S4_BASE + 0x000070;
  const int REGSTORE_GET_NEW = S4_BASE + 0x000071;
  const int REGSTORE_GET_FAILURE = S4_BASE + 0x000072;
  const int REGSTORE_SET_START = S4_BASE + 0x000073;
  const int REGSTORE_SET_SUCCESS = S4_BASE + 0x000074;
  const int REGSTORE_SET_FAILURE = S4_BASE + 0x000075;
  const int REGSTORE_DESERIALIZATION_FAILED = S4_BASE + 0x000076;

} //namespace SASEvent

#endif

