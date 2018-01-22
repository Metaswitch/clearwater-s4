/**
 * @file s4.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#ifndef S4_H__
#define S4_H__

#include <string>
#include <list>
#include <map>
#include <stdio.h>
#include <stdlib.h>

#include "sas.h"
#include "associated_uris.h"
#include "astaire_aor_store.h"
#include "httpclient.h"
#include "chronosconnection.h"

class S4
{
public:
  /// @class S4::ChronosTimerRequestSender
  ///
  /// Class responsible for sending any requests to Chronos about
  /// registration/subscription expiry
  ///
  /// @param chronos_conn    The underlying chronos connection
  class ChronosTimerRequestSender
  {
  public:
    ChronosTimerRequestSender(ChronosConnection* chronos_conn);

    virtual ~ChronosTimerRequestSender();

    /// Create and send any appropriate Chronos requests
    ///
    /// @param aor_id       The AoR ID
    /// @param aor_pair     The AoR pair to send Chronos requests for
    /// @param now          The current time
    /// @param trail        SAS trail
    virtual void send_timers(const std::string& aor_id,
                             AoR* aor,
                             int now,
                             SAS::TrailId trail);

    /// S4 is the only class that can use ChronosTimerRequestSender
    friend class S4;

  private:
    ChronosConnection* _chronos_conn;

    /// Build the tag info map from an AoR
    virtual void build_tag_info(AoR* aor,
                                std::map<std::string, uint32_t>& tag_map);

    /// Create the Chronos Timer request
    ///
    /// @param aor_id       The AoR ID
    /// @param timer_id     The Timer ID
    /// @param expiry       Timer length
    /// @param tags         Any tags to add to the Chronos timer
    /// @param trail        SAS trail
    virtual void set_timer(const std::string& aor_id,
                           std::string& timer_id,
                           int expiry,
                           std::map<std::string, uint32_t> tags,
                           SAS::TrailId trail);
  };

  /**
   * @brief S4 constructor
   *
   * @param id
   * @param chronos_connection - Chronos connection used to set timers for
   *                             expiring registrations and subscriptions.
   * @param aor_store         - Pointer to the underlying data store interface
   * @param remote_s4s        - vector of pointer to remote S4 stores
   */
  S4(std::string id,
     ChronosConnection* chronos_connection,
     AoRStore* aor_store,
     std::vector<S4*> remote_s4s);

  /// Destructor.
  virtual ~S4();

  virtual HTTPCode handle_get(std::string aor_id,
                              AoR** aor,
                              uint64_t& version,
                              SAS::TrailId trail);
  virtual HTTPCode handle_local_delete(std::string aor_id,
                                       uint64_t cas,
                                       SAS::TrailId trail);
  virtual HTTPCode handle_remote_delete(std::string aor_id,
                                        SAS::TrailId trail);
  virtual HTTPCode handle_put(std::string aor_id,
                              AoR* aor,
                              SAS::TrailId id);

  virtual HTTPCode handle_patch(std::string aor_id,
                                PatchObject* po,
                                AoR** aor,
                                SAS::TrailId trail);

  std::string get_id() { return _id; }
private:
  void replicate_delete_cross_site(std::string aor_id,
                                   SAS::TrailId trail);
  void replicate_patch_cross_site(std::string aor_id,
                                  PatchObject* po,
                                  SAS::TrailId trail);
  void replicate_put_cross_site(std::string aor_id,
                                AoR* aor,
                                SAS::TrailId trail);

  Store::Status get_aor(const std::string& aor_id,
                        AoR** aor,
                        SAS::TrailId trail);
  Store::Status write_aor(const std::string& aor_id,
                          AoR* aor,
                          SAS::TrailId trail);

  std::string _id;
  ChronosTimerRequestSender* _chronos_timer_request_sender;
  AoRStore* _aor_store;
  std::vector<S4*> _remote_s4s;
};

#endif
