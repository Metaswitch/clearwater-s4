/**
 * @file mock_s4.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_S4_H__
#define MOCK_S4_H__

#include "gmock/gmock.h"
#include "s4.h"


class MockS4 : public S4
{
  MockS4();
  virtual ~MockS4();

  MOCK_METHOD4(handle_get, HTTPCode(const std::string& id,
                                    AoR** aor,
                                    uint64_t& version,
                                    SAS::TrailId trail));

  MOCK_METHOD3(handle_delete, HTTPCode(const std::string& id,
                                       uint64_t version,
                                       SAS::TrailId trail));

  MOCK_METHOD3(handle_put, HTTPCode(const std::string& id,
                                    const AoR& aor,
                                    SAS::TrailId trail));

  MOCK_METHOD4(handle_patch, HTTPCode(const std::string& id,
                                      const PatchObject& patch_object,
                                      AoR** aor,
                                      SAS::TrailId trail));

  MOCK_METHOD2(handle_timer_pop, void(const std::string& aor_id,
                                      SAS::TrailId trail));

  MOCK_METHOD2(mimic_timer_pop, void(const std::string& aor_id,
                                     SAS::TrailId trail));
};

#endif
