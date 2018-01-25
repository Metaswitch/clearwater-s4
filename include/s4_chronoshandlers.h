/**
 * @file s4_chronoshandlers.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef S4_CHRONOSHANDLERS_H__
#define S4_CHRONOSHANDLERS_H__

#include "s4_handlers.h"

/**
 * @brief S4 handler for dealing with Chronos timer pop
 */
class ChronosAoRTimeoutTaskHandler;

class ChronosAoRTimeoutTask : public AoRTimeoutTask
{
public:
  /**
   * @brief 
   *
   * @param req
   * @param cfg
   * @param trail
   */
  ChronosAoRTimeoutTask(HttpStack::Request& req,
                        const Config* cfg,
                        SAS::TrailId trail) :
    AoRTimeoutTask::AoRTimeoutTask(req, cfg, trail)
  {};

  void run();

protected:
  /**
   * @brief 
   *
   * @param body
   *
   * @return 
   */
  HTTPCode parse_response(std::string body);

  /**
   * @brief 
   */
  void handle_response();

  std::string _aor_id;

  friend class ChronosAoRTimeoutTaskHandler;
};

#endif
