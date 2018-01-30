/**
 * @file s4.cpp
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <time.h>

#include "log.h"
#include "utils.h"
#include "s4.h"
#include "astaire_aor_store.h"
#include "chronosconnection.h"
#include "s4_chronoshandlers.h"

S4::S4(std::string id,
       ChronosConnection* chronos_connection,
       std::string callback_uri,
       AoRStore* aor_store,
       std::vector<S4*> remote_s4s) :
  _id(id),
  _chronos_timer_request_sender(new ChronosTimerRequestSender(chronos_connection)),
  _chronos_callback_uri(callback_uri),
  _aor_store(aor_store),
  _remote_s4s(remote_s4s)
{
}

S4::S4(std::string id,
       AoRStore* aor_store) :
  _id(id),
  _chronos_timer_request_sender(NULL),
  _chronos_callback_uri(""),
  _aor_store(aor_store),
  _remote_s4s({})
{
}

S4::~S4()
{
  delete _chronos_timer_request_sender;
}

void S4::initialise(BaseSubscriberManager* subscriber_manager)
{
  TRC_DEBUG("Setting reference to subscriber manager in local S4");

  if (!_remote_s4s.empty())
  {
    _subscriber_manager = subscriber_manager;
  }
}

HTTPCode S4::handle_get(const std::string& id,
                        AoR** aor,
                        uint64_t& version,
                        SAS::TrailId trail)
{
  TRC_DEBUG("Handling GET for %s on %s", id.c_str(), _id.c_str());

  HTTPCode rc;
  bool retry_get = true;

  while (retry_get == true)
  {
    // Delete the AoR on each iteration, and set the retry flag to false.
    // The flag is only set to true if there's data contention on the write.
    delete *aor; *aor = NULL;
    retry_get = false;

    Store::Status store_rc = get_aor(id, aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      TRC_DEBUG("Store error when getting subscriber %s on %s",
                id.c_str(), _id.c_str());
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      TRC_DEBUG("Subscriber not found when getting subscriber %s on %s",
                id.c_str(), _id.c_str());

      // If we don't have any bindings, try the remote stores.
      rc = HTTP_NOT_FOUND;

      for (S4* remote_s4 : _remote_s4s)
      {
        AoR* remote_aor = NULL;
        uint64_t unused_version;
        HTTPCode remote_rc = remote_s4->handle_get(id,
                                                   &remote_aor,
                                                   unused_version,
                                                   trail);

        if (remote_rc == HTTP_OK)
        {
          // The remote store has an entry for this AoR and it has bindings -
          // copy the information across.
          remote_aor->_cas = 0;

          Store::Status store_rc = write_aor(id, *remote_aor, trail);

          if (store_rc == Store::Status::ERROR)
          {
            TRC_DEBUG("Store error when adding subscriber %s to %s",
                      id.c_str(), _id.c_str());
            delete remote_aor; remote_aor = NULL;
            rc = HTTP_SERVER_ERROR;
          }
          else if (store_rc == Store::Status::OK)
          {
            TRC_DEBUG("Successfully added the subscriber %s to %s",
                      id.c_str(), _id.c_str());
            version = remote_aor->_cas;
            *aor = remote_aor;
            rc = HTTP_OK;
          }
          else if (store_rc == Store::Status::DATA_CONTENTION)
          {
            TRC_DEBUG("Contention when adding subscriber %s to %s",
                      id.c_str(), _id.c_str());
            delete remote_aor; remote_aor = NULL;
            retry_get = true;
          }

          break;
        }
      }
    }
    else
    {
      TRC_DEBUG("Successfully retrieved subscriber %s from %s",
                 id.c_str(), _id.c_str());
      version = (*aor)->_cas;
      rc = HTTP_OK;
    }
  }

  return rc;
}

HTTPCode S4::handle_delete(const std::string& id,
                           uint64_t version,
                           SAS::TrailId trail)
{
  TRC_DEBUG("Handling local DELETE for %s on %s", id.c_str(), _id.c_str());

  // Get the AoR from the data store - this only looks in the local store.
  AoR* aor = NULL;
  Store::Status store_rc = get_aor(id, &aor, trail);

  HTTPCode rc;

  if (store_rc == Store::Status::ERROR)
  {
    TRC_DEBUG("Store error when getting subscriber %s on %s during a DELETE",
              id.c_str(), _id.c_str());
    rc = HTTP_SERVER_ERROR;
  }
  else if (store_rc == Store::Status::NOT_FOUND)
  {
    TRC_DEBUG("Subscriber %s isn't on %s, unable to delete it",
              id.c_str(), _id.c_str());
    rc = HTTP_PRECONDITION_FAILED;
  }
  else
  {
    if (aor->_cas == version)
    {
      // Clear the AoR.
      aor->clear(true);

      // Write the empty AoR back to the store.
      Store::Status store_rc = write_aor(id, *aor, trail);

      if (store_rc == Store::Status::OK)
      {
        TRC_DEBUG("Successfully deleted subscriber %s from %s",
                   id.c_str(), _id.c_str());
        rc = HTTP_NO_CONTENT;

        // Subscriber has been deleted from the local site, so send the DELETE
        // out to the remote sites. The response to the SM is always going to be
        // OK independently of whether any remote DELETEs are successful.
        replicate_delete_cross_site(id, trail);
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        TRC_DEBUG("Contention when deleting subscriber %s from %s",
                  id.c_str(), _id.c_str());
        rc = HTTP_PRECONDITION_FAILED;
      }
      else
      {
        TRC_DEBUG("Store error when deleting subscriber %s from %s",
                  id.c_str(), _id.c_str());
        rc = HTTP_SERVER_ERROR;
      }
    }
    else
    {
      // The version isn't current. This suggests that the client is attempting
      // to delete the subscriber without knowing the up to date information.
      // Reject this.
      TRC_DEBUG("Mismatched version. Delete version (%d), stored version (%d)",
                version, aor->_cas);
      rc = HTTP_PRECONDITION_FAILED;
    }
  }

  delete aor; aor = NULL;
  return rc;
}

void S4::handle_remote_delete(const std::string& id,
                              SAS::TrailId trail)
{
  TRC_DEBUG("Handling DELETE for %s on %s", id.c_str(), _id.c_str());

  // Get the AoR from the data store - this only looks in the local store.
  bool retry_delete = true;

  while (retry_delete)
  {
    // Set the retry flag to false. The flag is only set to true if there's
    // data contention on the write.
    retry_delete = false;

    AoR* aor = NULL;
    Store::Status store_rc = get_aor(id, &aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      TRC_DEBUG("Store error when getting subscriber %s on %s during a DELETE",
                 id.c_str(), _id.c_str());
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      TRC_DEBUG("Subscriber %s isn't on %s, no need to delete it",
                 id.c_str(), _id.c_str());
    }
    else
    {
      // Clear the AoR.
      aor->clear(true);

      // Write the empty AoR back to the store.
      Store::Status store_rc = write_aor(id, *aor, trail);

      if (store_rc == Store::Status::OK)
      {
        TRC_DEBUG("Successfully deleted subscriber %s from %s",
                   id.c_str(), _id.c_str());
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        TRC_DEBUG("Contention when deleting subscriber %s from %s",
                  id.c_str(), _id.c_str());
        retry_delete = true;
      }
      else
      {
        TRC_DEBUG("Store error when deleting subscriber %s from %s",
                  id.c_str(), _id.c_str());
      }
    }

    delete aor; aor = NULL;
  }
}

HTTPCode S4::handle_put(const std::string& id,
                        const AoR& aor,
                        SAS::TrailId trail)
{
  TRC_DEBUG("Adding subscriber %s to %s", id.c_str(), _id.c_str());

  HTTPCode rc = HTTP_OK;

  // Attempt to write the data to the local store. We don't do a get first as
  // we expect the subscriber shouldn't exist. If the subscriber already
  // exists this will fail with data contention, and we'll return an error code
  Store::Status store_rc = write_aor(id, (AoR&)aor, trail);

  if (store_rc == Store::Status::OK)
  {
    TRC_DEBUG("Successfully added subscriber %s to %s",
              id.c_str(), _id.c_str());
    rc = HTTP_OK;

    // Subscriber has been added on the local site, so send the PUTs
    // out to the remote sites. The response to the SM is always going to be
    // OK independently of whether any remote PUTs are successful.
    replicate_put_cross_site(id, aor, trail);
  }
  else
  {
    // Failed to add data - we don't try and add the subscriber to any remote
    // sites.
    TRC_DEBUG("Failed to add subscriber %s to %s", id.c_str(), _id.c_str());

    if (store_rc == Store::Status::ERROR)
    {
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::DATA_CONTENTION)
    {
      rc = HTTP_PRECONDITION_FAILED;
    }
  }

  return rc;
}

HTTPCode S4::handle_patch(const std::string& id,
                          const PatchObject& po,
                          AoR** aor,
                          SAS::TrailId trail)
{
  TRC_DEBUG("Updating subscriber %s on %s", id.c_str(), _id.c_str());

  HTTPCode rc = HTTP_OK;
  bool retry_patch = true;

  while (retry_patch)
  {
    // Delete the AoR on each iteration, and set the retry flag to false.
    // The flag is only set to true if there's data contention on the write.
    retry_patch = false;
    delete *aor; *aor = NULL;

    Store::Status store_rc = get_aor(id, aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      TRC_DEBUG("Store error when getting subscriber %s on %s during a PATCH",
                 id.c_str(), _id.c_str());
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      // The subscriber can't be found - it's not valid to PATCH a non-existent
      // subscriber.
      TRC_DEBUG("Subscriber %s not found on %s during a PATCH",
                 id.c_str(), _id.c_str());
      rc = HTTP_PRECONDITION_FAILED;
    }
    else
    {
      // Update the AoR with the requested changes.
      (*aor)->patch_aor(po);
      Store::Status store_rc = write_aor(id, *(*aor), trail);

      if (store_rc == Store::Status::OK)
      {
        TRC_DEBUG("Updated subscriber %s on %s", id.c_str(), _id.c_str());
        rc = HTTP_OK;

        // Subscriber has been updated on the local site, so send the PATCHs
        // out to the remote sites. The response to the SM is always going to be
        // OK independently of whether any remote PATCHs are successful.
        PatchObject remote_po = PatchObject(po);
        remote_po.set_increment_cseq(false);
        remote_po.set_minimum_cseq((*aor)->_notify_cseq);
        replicate_patch_cross_site(id, remote_po, **aor, trail);
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        // Failed to update the subscriber due to data contention. Retry the
        // update.
        TRC_DEBUG("Failed to update subscriber %s on %s due to contention",
                  id.c_str(),
                  _id.c_str());
        retry_patch = true;
      }
      else
      {
        // Failed to update the subscriber due to a store error. There's no
        // point in retrying. Delete the retrieved AoR to clean it up.
        TRC_DEBUG("Failed to update subscriber %s on %s due to a store error",
                  id.c_str(),
                  _id.c_str());
        delete *aor; *aor = NULL;
        rc = HTTP_SERVER_ERROR;
      }
    }
  }

  return rc;
}

void S4::handle_timer_pop(const std::string& aor_id,
                          SAS::TrailId trail) 
{
  if (_subscriber_manager != NULL)
  {
    TRC_DEBUG("Calling subscriber manager to handle the timer pop");
    _subscriber_manager->handle_timer_pop(aor_id, trail);
  }
}

void S4::mimic_timer_pop(const std::string& aor_id,
                         SAS::TrailId trail) 
{
  TRC_DEBUG("Mimicking a timer pop to subscriber manager");

  // Create a task to send timer pop and put it on worker thread, same as
  // ChronosAoRTimeoutTask.
  PJUtils::run_callback_on_worker_thread(
             new MimicPopHandler(new MimicPopTask(aor_id, this, trail)), false);
}

void S4::replicate_delete_cross_site(const std::string& id,
                                     SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    remote_s4->handle_remote_delete(id, trail);
  }
}

void S4::replicate_put_cross_site(const std::string& id,
                                  const AoR& aor,
                                  SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    HTTPCode rc = remote_s4->handle_put(id, aor, trail);

    if (rc == HTTP_PRECONDITION_FAILED)
    {
      // We've tried to do a PUT to a remote site that already has data. We need
      // to send a PATCH instead.
      TRC_DEBUG("Need to convert PUT to PATCH for %s on %s",
                id.c_str(), _id.c_str());

      PatchObject po;
      convert_aor_to_patch(aor, po);

      AoR* remote_aor = NULL;
      remote_s4->handle_patch(id, po, &remote_aor, trail);
      delete remote_aor; remote_aor = NULL;
    }
  }
}

// Replicate the PATCH to each remote site. We don't care about the return code
// from the remote sites unless it's PRECONDITION FAILED, in which case we want
// to send a PUT instead (to reinstantiate the subscriber).
void S4::replicate_patch_cross_site(const std::string& id,
                                    const PatchObject& po,
                                    const AoR& aor,
                                    SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    AoR* remote_aor = NULL;
    HTTPCode rc = remote_s4->handle_patch(id, po, &remote_aor, trail);
    delete remote_aor; remote_aor = NULL;

    if (rc == HTTP_PRECONDITION_FAILED)
    {
      // We've tried to do a PATCH to a remote site that doesn't have any data.
      // We need to send a PUT.
      TRC_DEBUG("Need to convert PATCH to PUT for %s", _id.c_str());
      AoR* aor_for_put = new AoR(id);
      aor_for_put->copy_aor(aor);
      remote_s4->handle_put(id, *aor_for_put, trail);
      delete aor_for_put; aor_for_put = NULL;
    }
  }
}

Store::Status S4::get_aor(const std::string& id,
                          AoR** aor,
                          SAS::TrailId trail)
{
  Store::Status rc;

  *aor = _aor_store->get_aor_data(id, trail);

  if (aor == NULL || *aor == NULL)
  {
    // Store error when getting data - return an error.
    TRC_DEBUG("Store error when getting the AoR for %s from %s",
               id.c_str(), _id.c_str());
    rc = Store::Status::ERROR;
  }
  else if ((*aor)->bindings().empty())
  {
    // We successfully contacted the store, but we didn't find the AoR. This
    // creates an empty AoR - delete this and return not found.
    TRC_DEBUG("No AoR found for %s from %s", id.c_str(), _id.c_str());
    rc = Store::Status::NOT_FOUND;
    delete *aor; *aor = NULL;
  }
  else
  {
    TRC_DEBUG("Found an AoR for %s from %s", id.c_str(), _id.c_str());
    rc = Store::Status::OK;
  }

  return rc;
}

Store::Status S4::write_aor(const std::string& id,
                            AoR& aor,
                            SAS::TrailId trail)
{
  TRC_DEBUG("Writing AoR to store");

  // If the AoR has no bindings then it should be deleted. Clear up any
  // subscriptions.
  if (aor.bindings().empty() && !aor.subscriptions().empty())
  {
    TRC_DEBUG("Cleaning up AoR");
    aor.clear(true);
  }

  int now = time(NULL);

  printf("\nnext expires %d now %d\n", aor.get_next_expires(), now);

  // Send Chronos timer requests if it's a local store.
  if (_chronos_timer_request_sender)
  {
    TRC_DEBUG("Sending Chronos timer requests for local store");
    _chronos_timer_request_sender->send_timers(id, _chronos_callback_uri, &aor, now, trail);
  }

  // Check if any binding has expired and send mimic timer pop.
  if (!aor.bindings().empty() && aor.get_next_expires() <= now)
  {
    TRC_DEBUG("Some binding has expired");
    mimic_timer_pop(id, trail);
  }  
 
  Store::Status rc = _aor_store->set_aor_data(id,
                                              &aor,
                                              aor.get_last_expires() + 10,
                                              trail);
  if (rc == Store::Status::OK)
  {
    TRC_DEBUG("Successfully written AoR for %s to %s",
              id.c_str(), _id.c_str());
  }
  else
  {
    TRC_DEBUG("Failed to write AoR for %s to %s",
              id.c_str(), _id.c_str());
  }

  return rc;
}

S4::ChronosTimerRequestSender::
     ChronosTimerRequestSender(ChronosConnection* chronos_conn) :
  _chronos_conn(chronos_conn)
{
}

S4::ChronosTimerRequestSender::~ChronosTimerRequestSender()
{
}

void S4::ChronosTimerRequestSender::build_tag_info (
                                       AoR* aor,
                                       std::map<std::string, uint32_t>& tag_map)
{
  // Each timer is built to represent a single registration i.e. an AoR.
  tag_map["REG"] = 1;
  tag_map["BIND"] = aor->get_bindings_count();
  tag_map["SUB"] = aor->get_subscriptions_count();
}

void S4::ChronosTimerRequestSender::send_timers(const std::string& aor_id,
                                                const std::string& callback_uri,
                                                AoR* aor,
                                                int now,
                                                SAS::TrailId trail)
{
  std::map<std::string, uint32_t> tags;
  std::string& timer_id = aor->_timer_id;

  // An AoR with no bindings is invalid, and the timer should be deleted.
  // We do this before getting next_expires to save on processing.
  if (aor->get_bindings_count() == 0)
  {
    if (timer_id != "")
    {
      _chronos_conn->send_delete(timer_id, trail);
    }
    return;
  }

  build_tag_info(aor, tags);
  int next_expires = aor->get_next_expires();

  if (next_expires == 0)
  {
    // LCOV_EXCL_START - No UTs for unhittable code

    // This should never happen, as an empty AoR should never reach
    // get_next_expires
    TRC_DEBUG("get_next_expires returned 0. The expiry of AoR members "
              "is corrupt, or an empty (invalid) AoR was passed in.");

    // LCOV_EXCL_STOP
  }

  // Set the expiry time to be relative to now.
  int expiry = (next_expires > now) ? (next_expires - now) : (now);

  set_timer(aor_id,
            timer_id,
            callback_uri,
            expiry,
            tags,
            trail);
}

void S4::ChronosTimerRequestSender::set_timer(
                                          const std::string& aor_id,
                                          std::string& timer_id,
                                          const std::string& callback_uri,
                                          int expiry,
                                          std::map<std::string, uint32_t> tags,
                                          SAS::TrailId trail)
{
  std::string temp_timer_id = "";
  HTTPCode status;
  std::string opaque = "{\"aor_id\": \"" + aor_id + "\"}";

  // If a timer has been previously set for this binding, send a PUT.
  // Otherwise sent a POST.
  if (timer_id == "")
  {
    status = _chronos_conn->send_post(temp_timer_id,
                                      expiry,
                                      callback_uri,
                                      opaque,
                                      trail,
                                      tags);
  }
  else
  {
    temp_timer_id = timer_id;
    status = _chronos_conn->send_put(temp_timer_id,
                                     expiry,
                                     callback_uri,
                                     opaque,
                                     trail,
                                     tags);
  }

  // Update the timer id. If the update to Chronos failed, that's OK,
  // don't reject the request or update the stored timer id.
  if (status == HTTP_OK)
  {
    timer_id = temp_timer_id;
  }
}
