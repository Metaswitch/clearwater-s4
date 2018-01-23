/**
 * @file aor.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#ifndef AOR_H__
#define AOR_H__

#include <string>
#include <list>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "associated_uris.h"
#include <boost/optional.hpp>

/// JSON serialization constants.
/// These live here, as the core logic of serialization lives in the AoR
/// to_json methods, but the SDM also uses some of them.
static const char* const JSON_BINDINGS = "bindings";
static const char* const JSON_CID = "cid";
static const char* const JSON_CSEQ = "cseq";
static const char* const JSON_EXPIRES = "expires";
static const char* const JSON_PRIORITY = "priority";
static const char* const JSON_PARAMS = "params";
static const char* const JSON_PATH_HEADERS = "path_headers";
static const char* const JSON_TIMER_ID = "timer_id";
static const char* const JSON_PRIVATE_ID = "private_id";
static const char* const JSON_EMERGENCY_REG = "emergency_reg";
static const char* const JSON_SUBSCRIPTIONS = "subscriptions";
static const char* const JSON_REQ_URI = "req_uri";
static const char* const JSON_FROM_URI = "from_uri";
static const char* const JSON_FROM_TAG = "from_tag";
static const char* const JSON_TO_URI = "to_uri";
static const char* const JSON_TO_TAG = "to_tag";
static const char* const JSON_ROUTES = "routes";
static const char* const JSON_NOTIFY_CSEQ = "notify_cseq";
static const char* const JSON_SCSCF_URI = "scscf-uri";

/// @class Binding
///
/// A single registered address.
class Binding
{
public:
  Binding(std::string address_of_record);

  /// The address of record, e.g. "sip:name@example.com".
  std::string _address_of_record;

  /// This is the binding ID. SDM-REFACTOR-TODO: Actually it might not always be.
  /// See get_binding_id() in registrarsproutlet.cpp
  /// The registered contact URI, e.g.,
  /// "sip:2125551212@192.168.0.1:55491;transport=TCP;rinstance=fad34fbcdea6a931"
  std::string _uri;

  /// The Call-ID: of the registration.  Per RFC3261, this is the same for
  /// all registrations from a given UAC to this registrar (for this AoR).
  /// E.g., "gfYHoZGaFaRNxhlV0WIwoS-f91NoJ2gq"
  std::string _cid;

  /// Contains any path headers (in order) that were present on the
  /// register.  Empty if there were none. This is the full path header,
  /// including the disply name, URI and any header parameters.
  std::list<std::string> _path_headers;

  /// Contains the URI part of any path headers (in order) that were
  /// present on the register. Empty if there were none.
  std::list<std::string> _path_uris;

  /// The CSeq value of the REGISTER request.
  int _cseq;

  /// The time (in seconds since the epoch) at which this binding should
  /// expire.  Based on the expires parameter of the Contact: header.
  int _expires;

  /// The Contact: header q parameter (qvalue), times 1000.  This is used
  /// to prioritise the registrations (highest value first), per RFC3261
  /// s10.2.1.2.
  int _priority;

  /// Any other parameters found in the Contact: header, stored as key ->
  /// value.  E.g., "+sip.ice" -> "".
  std::map<std::string, std::string> _params;

  /// The private ID this binding was registered with.
  std::string _private_id;

  /// Whether this is an emergency registration.
  bool _emergency_registration;

  /// Returns the ID of this binding.
  std::string get_id() const { return _uri; }

  /// Serialize the binding as a JSON object.
  ///
  /// @param writer - a rapidjson writer to write to.
  void to_json(rapidjson::Writer<rapidjson::StringBuffer>& writer) const;

  // Deserialize a binding from a JSON object.
  //
  // @param b_obj - The binding as a JSON object.
  //
  // @return      - Nothing. If this function fails (because the JSON is not
  //                semantically valid) this method throws JsonFormError.
  void from_json(const rapidjson::Value& b_obj);
};

/// Typedef the map Bindings and the pair BindingPair. First is sometimes the
/// contact URI, but not always. Second is a pointer to a Binding.
typedef std::map<std::string, Binding*> Bindings;
typedef std::pair<std::string, Binding*> BindingPair;

/// @class Subscription
///
/// Represents a subscription to registration events for the AoR.
class Subscription
{
public:
  Subscription(): _refreshed(false), _expires(0) {};

  /// The Contact URI for the subscription dialog (used as the Request URI
  /// of the NOTIFY)
  std::string _req_uri;

  /// The From URI for the subscription dialog (used in the to header of
  /// the NOTIFY)
  std::string _from_uri;

  /// The From tag for the subscription dialog.
  std::string _from_tag;

  /// The To URI for the subscription dialog.
  std::string _to_uri;

  /// The To tag for the subscription dialog.
  std::string _to_tag;

  /// The call ID for the subscription dialog.
  std::string _cid;

  /// Whether the subscription has been refreshed since the last NOTIFY.
  bool _refreshed;

  /// The list of Record Route URIs from the subscription dialog.
  std::list<std::string> _route_uris;

  /// The time (in seconds since the epoch) at which this subscription
  /// should expire.
  int _expires;

  /// Returns the ID of this subscription.
  std::string get_id() const { return _to_tag; }

  /// Serialize the subscription as a JSON object.
  ///
  /// @param writer - a rapidjson writer to write to.
  void to_json(rapidjson::Writer<rapidjson::StringBuffer>& writer) const;

  // Deserialize a subscription from a JSON object.
  //
  // @param s_obj - The subscription as a JSON object.
  //
  // @return      - Nothing. If this function fails (because the JSON is not
  //                semantically valid) this method throws JsonFormError.
  void from_json(const rapidjson::Value& s_obj);
};

/// Typedef the map Subscriptions and the pair SubscriptionPair. First is
/// sometimes the To tag, but not always. Second is a pointer to a
/// Subscription.
typedef std::map<std::string, Subscription*> Subscriptions;
typedef std::pair<std::string, Subscription*> SubscriptionPair;

class PatchObject
{
public:
  // Constructor
  PatchObject();

  /// Destructor
  ~PatchObject();

  /// Make sure copy is deep!
  PatchObject(const PatchObject& other);
  PatchObject& operator= (PatchObject const& other);

  /// Public functions to get the member variables
  inline const Bindings get_update_bindings() const { return _update_bindings; }
  inline const std::vector<std::string> get_remove_bindings() const { return _remove_bindings; }
  inline const Subscriptions get_update_subscriptions() const { return _update_subscriptions; }
  inline const std::vector<std::string> get_remove_subscriptions() const { return _remove_subscriptions; }
  inline const boost::optional<AssociatedURIs> get_associated_uris() const { return _associated_uris; }
  inline const int get_minimum_cseq() const { return _minimum_cseq; }
  inline const bool get_increment_cseq() const { return _increment_cseq; }

  /// Public functions to set the member variables
  inline void set_update_bindings(Bindings bindings) { _update_bindings = bindings; }
  inline void set_remove_bindings(std::vector<std::string> bindings) { _remove_bindings = bindings; }
  inline void set_update_subscriptions(Subscriptions subscriptions) { _update_subscriptions = subscriptions; }
  inline void set_remove_subscriptions(std::vector<std::string> subscriptions) { _remove_subscriptions = subscriptions; }
  inline void set_associated_uris(AssociatedURIs associated_uris) { _associated_uris = associated_uris; }
  inline void set_minimum_cseq(int minimum) { _minimum_cseq = minimum; }
  inline void set_increment_cseq(bool increment) { _increment_cseq = increment; }

private:
  // Common code between copy and assignment
  void common_constructor(const PatchObject& other);

  /// Clear all the bindings and subscriptions from this object.
  void clear();

  ///
  Bindings _update_bindings;

  ///
  std::vector<std::string> _remove_bindings;

  ///
  Subscriptions _update_subscriptions;

  ///
  std::vector<std::string> _remove_subscriptions;

  ///
  boost::optional<AssociatedURIs> _associated_uris;

  ///
  int _minimum_cseq;

  ///
  bool _increment_cseq;
};

/// @class AoR
///
/// Addresses that are registered for this address of record.
class AoR
{
public:
  /// Default Constructor.
  AoR(std::string sip_uri);

  /// Destructor.
  ~AoR();

  /// Make sure copy is deep!
  AoR(const AoR& other);

  // Make sure assignment is deep!
  AoR& operator= (AoR const& other);

  // Common code between copy and assignment
  void common_constructor(const AoR& other);

  /// Clear all the bindings and subscriptions from this object.
  void clear(bool clear_emergency_bindings);

  /// Retrieve a binding by Binding ID, creating an empty one if necessary.
  /// The created binding is completely empty, even the Contact URI field.
  Binding* get_binding(const std::string& binding_id);

  /// Removes any binding that had the given ID.  If there is no such binding,
  /// does nothing.
  void remove_binding(const std::string& binding_id);

  /// Retrieve a subscription by To tag, creating an empty one if necessary.
  Subscription* get_subscription(const std::string& to_tag);

  /// Remove a subscription for the specified To tag.  If there is no
  /// corresponding subscription does nothing.
  void remove_subscription(const std::string& to_tag);

  // Remove the bindings from an AOR object
  void clear_bindings();

  /// Retrieve all the bindings.
  inline const Bindings& bindings() const { return _bindings; }

  /// Retrieve all the subscriptions.
  inline const Subscriptions& subscriptions() const { return _subscriptions; }

  // Return the number of bindings in the AoR.
  inline uint32_t get_bindings_count() const { return _bindings.size(); }

  // Return the number of subscriptions in the AoR.
  inline uint32_t get_subscriptions_count() const { return _subscriptions.size(); }

  // Return the expiry time of the binding or subscription due to expire next.
  int get_next_expires();

  /// get_last_expires
  ///
  /// This returns the expiry time of the binding or subscription due to expire
  /// last.
  /// The expiry time is relative, so if this was called on an AoR containing a
  /// single binding due to expire in 1 minute it would return 60.
  /// This can be called on a empty AoR, where it will return 0.
  ///
  /// @return Return the expiry time of the binding or subscription due to
  ///         expire last.
  int get_last_expires() const;

  /// Copy all site agnostic values from one AoR to this AoR. This copies basically
  /// everything, but importantly not the CAS. It doesn't remove any bindings
  /// or subscriptions that may have been in the existing AoR but not in the copied
  /// AoR.
  ///
  /// @param source_aor           Source AoR for the copy
  void copy_aor(const AoR& source_aor);

  /// Patch an AoR with a partial update. The update covers adding or removing
  /// individual bindings or subscriptions, replacing the AssociatedURIs,
  /// incrementing the CSeq and setting the CSeq to at least a minimum value.
  /// Any combination of the above is supported. This method can't be used to
  /// change the timer ID or S-CSCF uri of the AoR.
  ///
  /// @param po PatchObject to patch the AoR with
  void patch_aor(const PatchObject& po);

  /// CSeq value for event notifications for this AoR.  This is initialised
  /// to one when the AoR record is first set up and incremented every time
  /// the record is updated while there are active subscriptions.  (It is
  /// sufficient to use the same CSeq for each NOTIFY sent on each active
  /// because there is no requirement that the first NOTIFY in a dialog has
  /// CSeq=1, and once a subscription dialog is established it should
  /// receive every NOTIFY for the AoR.)
  int _notify_cseq;

  // Chronos Timer ID
  std::string _timer_id;

  /// S-CSCF URI name for this AoR. This is used on the SAR if the
  /// registration expires. This field should not be changed once the
  /// registration has been created.
  std::string _scscf_uri;

  /// Map holding the bindings for a particular AoR indexed by binding ID.
  Bindings _bindings;

  /// Map holding the subscriptions for this AoR, indexed by the To tag
  /// generated when the subscription dialog was established.
  Subscriptions _subscriptions;

  // Associated URIs class, to hold the associated URIs for this IRS.
  AssociatedURIs _associated_uris;

  /// CAS value for this AoR record.  Used when updating an existing record.
  /// Zero for a new record that has not yet been written to a store.
  uint64_t _cas;

  // SIP URI for this AoR
  std::string _uri;

  /// Store code is allowed to manipulate bindings and subscriptions directly.
  friend class AoRStore;
  friend class SubscriberDataManager;
};

/// @class AoRPair
///
/// Class to hold a pair of AoRs. The original AoR holds the AoR retrieved
/// from the store, the current AoR holds any changes made to the AoR before
/// it's put back in the store
class AoRPair
{
public:
  AoRPair(std::string aor_id)
  {
    _orig_aor = new AoR(aor_id);
    _current_aor = new AoR(aor_id);
  }

  AoRPair(AoR* orig_aor, AoR* current_aor):
    _orig_aor(orig_aor),
    _current_aor(current_aor)
  {}

  ~AoRPair()
  {
    delete _orig_aor; _orig_aor = NULL;
    delete _current_aor; _current_aor = NULL;
  }

  /// Get the current AoR
  AoR* get_current() { return _current_aor; }

  /// Does the current AoR contain any bindings?
  bool current_contains_bindings()
  {
    return ((_current_aor != NULL) &&
            (!_current_aor->_bindings.empty()));
  }

  /// Does the current AoR contain any subscriptions?
  bool current_contains_subscriptions()
  {
    return ((_current_aor != NULL) &&
            (!_current_aor->subscriptions().empty()));
  }

  /// Utility functions to compare Bindings and Subscriptions in the original AoR
  /// and current AoR, and return the set of those created/updated or removed.
  Bindings get_updated_bindings();
  Subscriptions get_updated_subscriptions();
  Bindings get_removed_bindings();
  Subscriptions get_removed_subscriptions();

private:
  AoR* _orig_aor;
  AoR* _current_aor;

  /// Get the original AoR
  AoR* get_orig() { return _orig_aor; }

  /// The subscriber data manager is allowed to access the original AoR
  friend class SubscriberDataManager;
};

/// Convert an AoR to a PatchObject.
///
/// @param aor - AoR to convert to a PatchObject
/// @param po  - PatchObject to be populated with the AoR info
void convert_aor_to_patch(const AoR& aor, PatchObject& po);

#endif
