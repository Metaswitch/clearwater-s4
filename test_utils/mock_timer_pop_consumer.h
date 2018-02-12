/**
 * @file mock_timer_pop_consumer.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_TIMER_POP_CONSUMER_H__
#define MOCK_TIMER_POP_CONSUMER_H__

#include "gmock/gmock.h"

class MockTimerPopConsumer : public S4::TimerPopConsumer
{
public:
  MOCK_METHOD2(handle_timer_pop, void(const std::string&, SAS::TrailId));
};

#endif

