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
#include "pjutils.h"

class ChronosAoRTimeoutTaskHandler;

class ChronosAoRTimeoutTask : public AoRTimeoutTask
{
public:
  ChronosAoRTimeoutTask(HttpStack::Request& req,
                        const Config* cfg,
                        SAS::TrailId trail) :
    AoRTimeoutTask::AoRTimeoutTask(req, cfg, trail)
  {};

  void run();

protected:
  
  /// @brief Parse Chronos timer pop request as JSON to retrieve aor_id
  ///  
  ///  @param body     body of the Chronos timer pop request
  ///
  ///  @return Whether the request body has been parsed as JSON. This may be:
  ///    OK          - successfully stored aor_id from request
  ///    BAD_REQUEST - Failed to parse opaque data as JSON, or
  ///                  the opaque data is missing aor_id
  HTTPCode parse_request(const std::string& body);

  /// @brief Deal with the timer pop request
  void handle_request();

  std::string _aor_id;

  friend class ChronosAoRTimeoutTaskHandler;
};

/// S4 handler for dealing with Chronos timer pop
class ChronosAoRTimeoutTaskHandler : public PJUtils::Callback
{
private:
  ChronosAoRTimeoutTask* _task;

public:
  ChronosAoRTimeoutTaskHandler(ChronosAoRTimeoutTask* task) :
    _task(task)
  {
  }

  virtual void run()
  {
    _task->handle_request();

    delete _task;
  }
};

/// Class definition comes after MimicTimerPopTask
class MimicTimerPopHandler;

/// The task to mimic a timer pop for (implicitly) expired bindings by callling
/// S4 to handle_timer_pop.
class MimicTimerPopTask
{
public:
  MimicTimerPopTask(std::string aor_id,
               S4* s4,
               SAS::TrailId trail) :
    _aor_id(aor_id),
    _s4(s4),
    _trail(trail)
  {
  }

protected:
  void handle_request();

  std::string _aor_id;
  S4* _s4;
  SAS::TrailId _trail;

  friend class MimicTimerPopHandler;
};

/// This handler puts the MimicTimerPopTask on workers thread.
class MimicTimerPopHandler : public PJUtils::Callback
{
private:
  MimicTimerPopTask* _task;

public:
  MimicTimerPopHandler(MimicTimerPopTask* task) :
    _task(task)
  {
  }

  virtual void run()
  {
    _task->handle_request();

    delete _task;
  }
};

#endif
