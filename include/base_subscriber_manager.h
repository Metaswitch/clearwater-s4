/**
 * @file base_subscriber_manager.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef BASE_SUBSCRIBER_MANAGER_H__
#define BASE_SUBSCRIBER_MANAGER_H__

#include "sas.h"
#include "httpclient.h"
#include "subscriber_data_utils.h"

/// Base class for Subscriber Manager in Sprout. It's created solely for S4 to
/// get a reference to SM (which should handle the timer pop).
class BaseSubscriberManager
{
public:
  virtual ~BaseSubscriberManager() {};

  virtual void handle_timer_pop(const std::string& aor_id,
                                SAS::TrailId trail) = 0;

  virtual HTTPCode deregister_subscriber(const std::string& public_id,
                                         const SubscriberDataUtils::EventTrigger& event_trigger,
                                         SAS::TrailId trail) = 0;
};

#endif
