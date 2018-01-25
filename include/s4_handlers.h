/**
 * @file s4_handlers.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef S4_HANDLERS_H__
#define S4_HANDLERS_H__

#include "httpstack.h"
#include "httpstack_utils.h"
#include "subscriber_manager.h"

/// Base AoRTimeoutTask class for tasks that implement AoR timeout callbacks
/// from specific timer services.
class AoRTimeoutTask : public HttpStackUtils::Task
{
public:
  struct Config
  {
    Config(SubscriberManager* sm) :
      _sm(sm)
    {}
    SubscriberManager* _sm;
  };

  AoRTimeoutTask(HttpStack::Request& req,
                 const Config* cfg,
                 SAS::TrailId trail) :
    HttpStackUtils::Task(req, trail), _cfg(cfg)
  {};

  virtual void run() = 0;

protected:
  /**
   * @brief Call to Subscriber Manager to handle the timeout of this AoR.
   *
   * @param aor_id[in]    The AoR ID
   */
  void process_aor_timeout(const std::string& aor_id);

protected:
  const Config* _cfg;
};

#endif
