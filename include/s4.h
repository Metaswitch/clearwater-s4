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
  /// @param id[in]           - Site name of the S4. This is only used in logs.
  /// @param callback_uri[in] - Hostname that resolves to the S4s in the local
  ///                           site. Used as the Chronos callback URI.
  /// @param aor_store[in]    - Pointer to the underlying data store interface
  /// @param remote_s4s[in]   - Pointers to all the remote S4s. If a remote S4
  ///                           is created then this is an empty vector.
  S4(std::string id,
     ChronosConnection* chronos_connection,
     std::string callback_url,
     AoRStore* aor_store,
     std::vector<S4*> remote_s4s);

  /// Destructor.
  ///
  /// S4 doesn't own the memory for its pointer member variables, so there's
  /// nothing to do to clean up.
  virtual ~S4();

  /// This sends a request to S4 to get the data for a subscriber. This looks
  /// in the local store. If the local store returns NOT_FOUND, this asks the
  /// remote S4s. If a remote S4 has data, this writes that data back into the
  /// local store, and stops querying any other remote S4s. If a remote S4
  /// query fails for any reason, this is logged, but otherwise ignored (it
  /// doesn't impact the return code for the client call). If the data is
  /// successfully retrieved then S4 gives the data a version number as well.
  ///
  /// @param id[in]       - The ID of the subscriber. This must be the default
  ///                       public identity.
  /// @param aor[out]     - The retrieved data. This is only valid if the return
  ///                       code is Store::Status::OK. The caller must delete
  ///                       the AoR.
  /// @param version[out] - The version of the retrieved AoR.
  /// @param trail[in]    - The SAS trail ID
  ///
  /// @return Whether any data has been retrieved. This can be one of:
  ///   OK - The AoR was successfully retrieved.
  ///   NOT_FOUND - The subscriber has no stored data on any contactable site.
  ///   SERVER_ERROR - We failed to contact the local store; the subscriber
  ///                  information is unknown.
  virtual HTTPCode handle_get(const std::string& id,
                              AoR** aor,
                              uint64_t& version,
                              SAS::TrailId trail);

  /// This deletes the subscriber from the deployment. The delete takes a
  /// version - if the current subscriber data has a different version than
  /// the passed in version the delete fails. The subscriber is deleted from
  /// all contactable sites, the return code of this function only depends
  /// on whether the subscriber was deleted from the local site though.
  ///
  /// @param id[in]      - The ID of the subscriber. This must be the default
  ///                      public identity.
  /// @param version[in] - The version of the subscriber data that this is
  ///                      asking to delete.
  /// @param trail[in]   - The SAS trail ID
  ///
  /// @return Whether the data has been deleted. This can be one of:
  ///   NO_CONTENT - The AoR was successfully deleted from at least the local
  ///                site (or wasn't present in the first place).
  ///   PRECONDITION_FAILED - The current version of the subscriber's data is
  ///                         different to the version that the client has
  ///                         asked to delete. The versions must be the same.
  ///   SERVER_ERROR - We failed to contact the local store; the subscriber
  ///                  information is unknown.
  virtual HTTPCode handle_delete(const std::string& id,
                                 uint64_t version,
                                 SAS::TrailId trail);

  /// This adds a subscriber to the deployment. This only succeeds if the
  /// subscriber doesn't already exist. The subscriber is added to all
  /// contactable sites, the return code of this function only depends on
  /// whether the subscriber was added to the local site though.
  ///
  /// @param id[in]  - The ID of the subscriber. This must be the default
  ///                  public identity.
  /// @param aor[in] - The AoR object to update the subscriber with.
  /// @param trail   - The SAS trail ID
  ///
  /// @return Whether the data has been deleted. This can be one of:
  ///   OK - The AoR was successfully deleted from at least the local site
  ///       (or wasn't present in the first place).
  ///   PRECONDITION_FAILED - The subscriber already exists.
  ///   SERVER_ERROR - We failed to contact the local store; the subscriber
  ///                  information is unknown.
  virtual HTTPCode handle_put(const std::string& id,
                              const AoR& aor,
                              SAS::TrailId trail);

  /// This updates a subscriber in the deployment. This only succeeds if the
  /// subscriber already exists. The subscriber is updated in all contactable
  /// sites, the return code of this function only depends on whether the
  /// subscriber was updated in the local site though.
  ///
  /// @param id[in]   - The ID of the subscriber. This must be the default
  ///                   public identity.
  /// @param po[in]   - The patch object to patch and update the subscriber
  ///                   with.
  /// @param aor[out] - The retrieved data. This is only valid if the return
  ///                   code is HTTP_OK. The caller must delete the AoR.
  /// @param trail    - The SAS trail ID
  ///
  /// @return Whether the data has been deleted. This can be one of:
  ///   OK - The AoR was successfully deleted from at least the local site
  ///       (or wasn't present in the first place).
  ///   NOT_FOUND - The subscriber doesn't exist, so can't be patched.
  ///   SERVER_ERROR - We failed to contact the local store; the subscriber
  ///                  information is unknown.
  virtual HTTPCode handle_patch(const std::string& id,
                                const PatchObject& po,
                                AoR** aor,
                                SAS::TrailId trail);

  /// This deletes the subscriber from the local site. This should only be
  /// called from another S4, not a client. the deployment.
  ///
  /// @param id[in]    - The ID of the subscriber. This must be the default
  ///                    public identity.
  /// @param trail[in] - The SAS trail ID
  ///
  /// @return Whether the data has been deleted. This can be one of:
  ///   NO_CONTENT - The AoR was successfully deleted from the local site (or
  ///                wasn't present in the first place).
  ///   SERVER_ERROR - We failed to contact the local store; the subscriber
  ///                  information is unknown.
  virtual void handle_remote_delete(const std::string& id,
                                    SAS::TrailId trail);

private:
  /// This replicates a DELETE request from a client to the remote S4s. This
  /// doesn't return anything as the local S4 won't do anything if any
  /// remote delete fails (this function handles the different failure cases
  /// itself).
  ///
  /// @param id[in]    - The ID of the subscriber. This must be the default
  ///                    public identity.
  /// @param trail[in] - The SAS trail ID
  void replicate_delete_cross_site(const std::string& aor_id,
                                   SAS::TrailId trail);

  /// This replicates a PATCH request from a client to the remote S4s. This
  /// doesn't return anything as the local S4 won't do anything if any
  /// remote patch fails (this function handles the different failure cases
  /// itself).
  ///
  /// @param id[in]    - The ID of the subscriber. This must be the default
  ///                    public identity.
  /// @param po[in]    - The patch object to patch and update the subscriber
  ///                    with.
  /// @param trail[in] - The SAS trail ID
  void replicate_patch_cross_site(const std::string& aor_id,
                                  const PatchObject& po,
                                  SAS::TrailId trail);

  /// This replicates a PUT request from a client to the remote S4s. This
  /// doesn't return anything as the local S4 won't do anything if any
  /// remote puts fails (this function handles the different failure cases
  /// itself).
  ///
  /// @param id[in]    - The ID of the subscriber. This must be the default
  ///                    public identity.
  /// @param aor[in]   - The AoR object to update the subscriber with.
  /// @param trail[in] - The SAS trail ID
  void replicate_put_cross_site(const std::string& id,
                                const AoR& aor,
                                SAS::TrailId trail);

  /// This gets data from memcached (calling into the underlying data store),
  /// and returns whether the get was successful. This only calls into the local
  /// store.
  ///
  /// @param id[in]    - The ID of the subscriber to get data for.
  /// @param aor[out]  - The retrieved data. This is only valid if the return
  ///                    code is Store::Status::OK. The caller must delete the
  ///                    AoR.
  /// @param trail[in] - The SAS trail ID
  ///
  /// @return Whether the AoR was successfully written. This can be one of:
  ///   OK - The AoR was successfully retrieved.
  ///   ERROR - Something went wrong - there's no point in retrying. The AoR is
  ///           not valid.
  ///   NOT_FOUND - We successfully contacted the store but there was no entry
  ///               for the subscriber. The AoR is not valid.
  Store::Status get_aor(const std::string& aor_id,
                        AoR** aor,
                        SAS::TrailId trail);

  /// This writes data to memcached (calling into the underlying data store),
  /// and returns whether the write was successful. This only calls into the
  /// local store.
  ///
  /// @param id[in]    - The ID of the subscriber to update.
  /// @param aor[in]   - The AoR object to write.
  /// @param trail[in] - The SAS trail ID.
  ///
  /// @return Whether the AoR was successfully written. This can be one of:
  ///   OK - The AoR was successfully written.
  ///   ERROR - Something went wrong - there's no point in retrying
  ///   DATA_CONTENTION - We successfully contacted the store, but there was a
  ///                     CAS conflict. We can try the write again.
  Store::Status write_aor(const std::string& id,
                          AoR& aor,
                          SAS::TrailId trail);

  /// Gets the ID of this S4. This is only used for logging.
  ///
  /// @return The ID of this S4
  inline std::string get_id() { return _id; }

  /// The ID of this S4.
  std::string _id;
<<<<<<< HEAD
  ChronosTimerRequestSender* _chronos_timer_request_sender;
||||||| merged common ancestors
=======

  /// The callback URI this S4 puts on Chronos timers. This should be a hostname
  /// that resolves to all the local S4s in the local site.
  std::string _chronos_callback_uri;

  /// The interface to Rogers (which owns actually reading and
  /// writing to Rogers, and coverting between an AoR object and the JSON
  /// representation of the AoR object.
>>>>>>> master
  AoRStore* _aor_store;

  /// The remote S4s. This is empty if this S4 is a remote S4 already.
  std::vector<S4*> _remote_s4s;
};

#endif
