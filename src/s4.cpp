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

// SDM-REFACTOR-TODO:
// 1. Commonise logic in the handles? Or at least make more similar.
// 2. TRC statements
// 3. SAS logging (poss already covered by memcached logs)
// 4. Lifetimes of AoRs?
// 5. Implement PATCH
// 6. Implement PATCH/PUT conversion, and protect against loops
// 7. Implement max_expiry
// 8. Full UT

S4::S4(std::string id,
       AoRStore* aor_store,
       std::vector<S4*> remote_s4s) :
  _id(id),
  _aor_store(aor_store),
  _remote_s4s(remote_s4s)
{
}

S4::~S4()
{
}

HTTPCode S4::handle_get(std::string aor_id,
                        AoR** aor,
                        uint64_t& version,
                        SAS::TrailId trail)
{
  HTTPCode rc;
  bool retry_get = true;

  while (retry_get == true)
  {
    retry_get = false;
    Store::Status store_rc = get_aor(aor_id, aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      // If we don't have any bindings, try the remote stores.
      rc = HTTP_NOT_FOUND;

      for (S4* remote_s4 : _remote_s4s)
      {
        TRC_DEBUG("Handling remote GET on %s", remote_s4->get_id().c_str());
        AoR* remote_aor = NULL;
        uint64_t unused_version;
        HTTPCode remote_rc = remote_s4->handle_get(aor_id,
                                                   &remote_aor,
                                                   unused_version,
                                                   trail);

        if (remote_rc == HTTP_OK)
        {
          // The remote store has an entry for this AoR and it has bindings -
          // copy the information across.
          remote_aor->_cas = 0;

          Store::Status store_rc = write_aor(aor_id, remote_aor, trail);

          if (store_rc == Store::Status::ERROR)
          {
            // We haven't been able to write the data back to memcached.
            delete remote_aor; remote_aor = NULL;
            rc = HTTP_SERVER_ERROR;
          }
          else if (store_rc == Store::Status::OK)
          {
            version = remote_aor->_cas;
            *aor = remote_aor;
            rc = HTTP_OK;
          }
          else if (store_rc == Store::Status::DATA_CONTENTION)
          {
            delete remote_aor; remote_aor = NULL;
            retry_get = true;
          }

          break;
        }
      }
    }
    else
    {
      version = (*aor)->_cas;
      rc = HTTP_OK;
    }
  }

  return rc;
}

HTTPCode S4::handle_local_delete(std::string aor_id,
                                 uint64_t version,
                                 SAS::TrailId trail)
{
  TRC_DEBUG("Handling local DELETE for %s on %s", aor_id.c_str(), _id.c_str());

  // Get the AoR from the data store - this only looks in the local store.
  AoR* aor = NULL;
  Store::Status store_rc = get_aor(aor_id, &aor, trail);

  HTTPCode rc;

  if (store_rc == Store::Status::ERROR)
  {
    rc = HTTP_SERVER_ERROR;
  }
  else if (store_rc == Store::Status::NOT_FOUND)
  {
    rc = HTTP_PRECONDITION_FAILED;
  }
  else
  {
    if (aor->_cas == version)
    {
      // Clear the AoR.
      aor->clear(true);

      // Write the empty AoR back to the store.
      Store::Status store_rc = write_aor(aor_id, aor, trail);

      if (store_rc == Store::Status::OK)
      {
        rc = HTTP_NO_CONTENT;

        // Subscriber has been deleted from the local site, so send the DELETE
        // out to the remote sites. The response to the SM is always going to be
        // OK independently of whether any remote DELETEs are successful.
        replicate_delete_cross_site(aor_id, trail);
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        rc = HTTP_PRECONDITION_FAILED;
      }
      else
      {
        rc = HTTP_SERVER_ERROR;
      }
    }
    else
    {
      rc = HTTP_PRECONDITION_FAILED;
    }
  }

  delete aor; aor = NULL;
  return rc;
}

HTTPCode S4::handle_remote_delete(std::string aor_id,
                                  SAS::TrailId trail)
{
  TRC_DEBUG("Handling local DELETE for %s on %s", aor_id.c_str(), _id.c_str());

  // Get the AoR from the data store - this only looks in the local store.
  AoR* aor = NULL;

  HTTPCode rc;

  do
  {
    delete aor; aor = NULL;
    Store::Status store_rc = get_aor(aor_id, &aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      rc = HTTP_NOT_FOUND;
    }
    else
    {
      // Clear the AoR.
      aor->clear(true);

      // Write the empty AoR back to the store.
      Store::Status store_rc = write_aor(aor_id, aor, trail);

      if (store_rc == Store::Status::OK)
      {
        rc = HTTP_OK;
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        rc = HTTP_PRECONDITION_FAILED;
      }
      else
      {
        rc = HTTP_SERVER_ERROR;
      }
    }
  }
  while (rc == HTTP_PRECONDITION_FAILED);

  delete aor; aor = NULL;
  return rc;
}

HTTPCode S4::handle_put(std::string aor_id,
                        AoR* new_aor,
                        SAS::TrailId trail)
{
  HTTPCode rc = HTTP_OK;

  // Get the AoR from the data store - this only looks in the local store.
  AoR* aor = NULL;
  Store::Status store_rc = get_aor(aor_id, &aor, trail);

  if (store_rc == Store::Status::ERROR)
  {
    rc = HTTP_SERVER_ERROR;
  }
  else if (store_rc == Store::Status::OK)
  {
    rc = HTTP_PRECONDITION_FAILED;
  }
  else
  {
    new_aor->_cas = 0;

    Store::Status store_rc = write_aor(aor_id, new_aor, trail);

    if (store_rc == Store::Status::OK)
    {
      replicate_put_cross_site(aor_id, new_aor, trail);
      rc = HTTP_OK;
    }
    else if (store_rc == Store::Status::ERROR)
    {
      // Failed to add data - we don't try and add the subscriber to
      // any remote sites.
      TRC_DEBUG("Failed to add subscriber %s to %s",
                aor_id.c_str(),
                _id.c_str());
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::DATA_CONTENTION)
    {
      // Failed to add data - we don't try and add the subscriber to
      // any remote sites.
      TRC_DEBUG("Failed to add subscriber %s to %s",
                aor_id.c_str(),
                _id.c_str());
      rc = HTTP_PRECONDITION_FAILED;
    }
  }

  delete aor; aor = NULL;
  return rc;
}

HTTPCode S4::handle_patch(std::string aor_id,
                          PatchObject* po,
                          AoR** aor,
                          SAS::TrailId trail)
{
  HTTPCode rc = HTTP_OK;
  bool retry_patch = true;

  while (retry_patch)
  {
    retry_patch = false;

    delete *aor; *aor = NULL;
    Store::Status store_rc = get_aor(aor_id, aor, trail);

    if (store_rc == Store::Status::ERROR)
    {
      rc = HTTP_SERVER_ERROR;
    }
    else if (store_rc == Store::Status::NOT_FOUND)
    {
      rc = HTTP_PRECONDITION_FAILED;
    }
    else
    {
      // Update the AoR with the requested changes.
      (*aor)->patch_aor(po);
      Store::Status store_rc = write_aor(aor_id, *aor, trail);

      if (store_rc == Store::Status::OK)
      {
        replicate_patch_cross_site(aor_id, po, trail);
        rc = HTTP_OK;
      }
      else if (store_rc == Store::Status::DATA_CONTENTION)
      {
        // Failed to updateadd data - we don't try and update the subscriber in
        // any remote sites.
        TRC_DEBUG("Failed to update subscriber %s to %s",
                  aor_id.c_str(),
                  _id.c_str());
        retry_patch = true;
      }
      else
      {
        // Failed to updateadd data - we don't try and update the subscriber in
        // any remote sites.
        TRC_DEBUG("Failed to update subscriber %s to %s",
                  aor_id.c_str(),
                  _id.c_str());
        delete *aor; *aor = NULL;
        rc = HTTP_SERVER_ERROR;
      }
    }
  }

  return rc;
}

void S4::replicate_delete_cross_site(std::string aor_id,
                                     SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    remote_s4->handle_remote_delete(aor_id, trail);
  }
}

void S4::replicate_put_cross_site(std::string aor_id,
                                  AoR* aor,
                                  SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    HTTPCode rc = remote_s4->handle_put(aor_id, aor, trail);

    if (rc == HTTP_PRECONDITION_FAILED)
    {
      // We've tried to do a PUT to a remote site that already has data. We need
      // to send a PATCH instead.
      TRC_DEBUG("Need to convert PUT to PATCH for %s", _id.c_str());

      PatchObject* po = new PatchObject();
      aor->convert_aor_to_patch(po);
      AoR* remote_aor = NULL;

      remote_s4->handle_patch(aor_id, po, &remote_aor, trail);

      delete po; po = NULL;
      delete remote_aor; remote_aor = NULL;
    }
  }
}

void S4::replicate_patch_cross_site(std::string aor_id,
                                    PatchObject* po,
                                    SAS::TrailId trail)
{
  for (S4* remote_s4 : _remote_s4s)
  {
    AoR* remote_aor = NULL;
    HTTPCode rc = remote_s4->handle_patch(aor_id, po, &remote_aor, trail);
    delete remote_aor; remote_aor = NULL;

    if (rc == HTTP_PRECONDITION_FAILED)
    {
      // We've tried to do a PATCH to a remote site that doesn't have any data.
      // We need to send a PUT.
      TRC_DEBUG("Need to convert PATCH to PUT for %s", _id.c_str());
      AoR* aor = new AoR(aor_id);
      aor->convert_patch_to_aor(po);
      remote_s4->handle_put(aor_id, aor, trail);
      delete aor; aor = NULL;
    }
  }
}

Store::Status S4::get_aor(const std::string& aor_id,
                          AoR** aor,
                          SAS::TrailId trail)
{
  Store::Status rc;

  *aor = _aor_store->get_aor_data(aor_id, trail);

  if (aor == NULL || *aor == NULL)
  {
    // Store error when getting data.
    TRC_DEBUG("Store error when getting the AoR for %s from %s",
               aor_id.c_str(), _id.c_str());
    rc = Store::Status::ERROR;
  }
  else if ((*aor)->bindings().empty())
  {
    // We successfully contacted the store, but we didn't find the AoR. This
    // creates an empty AoR - delete this and return not found.
    TRC_DEBUG("No AoR found for %s from %s", aor_id.c_str(), _id.c_str());
    rc = Store::Status::NOT_FOUND;
    delete *aor; *aor = NULL;
  }
  else
  {
    TRC_DEBUG("Found an AoR for %s from %s", aor_id.c_str(), _id.c_str());
    rc = Store::Status::OK;
  }

  return rc;
}

Store::Status S4::write_aor(const std::string& aor_id,
                            AoR* aor,
                            SAS::TrailId trail)
{
  int next_expires;
  int last_expires;
  aor->get_next_and_last_expires(next_expires, last_expires);

  TRC_DEBUG("Expires are %d, %d", next_expires, last_expires);
  // SDM-REFACTOR-TODO: Set Chronos timer.

  return _aor_store->set_aor_data(aor_id,
                                  aor,
                                  last_expires,
                                  trail);
}
