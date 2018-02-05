/**
 * @file aor.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <limits.h>

#include "log.h"
#include "aor.h"
#include "json_parse_utils.h"
#include "rapidjson/error/en.h"

/// Default constructor.
AoR::AoR(std::string sip_uri) :
  _notify_cseq(1),
  _timer_id(""),
  _scscf_uri(""),
  _bindings(),
  _subscriptions(),
  _associated_uris(),
  _cas(0),
  _uri(sip_uri)
{
}

/// Destructor.
AoR::~AoR()
{
  clear(true);
}


/// Copy constructor.
AoR::AoR(const AoR& other)
{
  common_constructor(other);
}

// Make sure assignment is deep!
AoR& AoR::operator= (AoR const& other)
{
  if (this != &other)
  {
    clear(true);
    common_constructor(other);
  }

  return *this;
}

bool AoR::operator==(const AoR& other) const
{
  bool bindings_equal = false;

  if (_bindings.size() == other._bindings.size())
  {
    for (BindingPair binding : _bindings)
    {
      if (other._bindings.find(binding.first) == other._bindings.end())
      {
        break;
      }
      else if (*(binding.second) != *(other._bindings.at(binding.first)))
      {
        break;
      }
    }

    bindings_equal = true;
  }

  bool subscriptions_equal = false;

  if (_subscriptions.size() == other._subscriptions.size())
  {
    for (SubscriptionPair subscription : _subscriptions)
    {
      if (other._subscriptions.find(subscription.first) == other._subscriptions.end())
      {
        break;
      }
      else if (*(subscription.second) != *(other._subscriptions.at(subscription.first)))
      {
        break;
      }
    }

    subscriptions_equal = true;
  }

  if ((_notify_cseq != other._notify_cseq) ||
      (_timer_id != other._timer_id) ||
      (_scscf_uri != other._scscf_uri) ||
      (!bindings_equal) ||
      (!subscriptions_equal) ||
      (_associated_uris != other._associated_uris) ||
      (_cas != other._cas))
  {
    return false;
  }

  return true;
}

bool AoR::operator!=(const AoR& other) const
{
  return !(operator==(other));
}

void AoR::common_constructor(const AoR& other)
{
  for (Bindings::const_iterator i = other._bindings.begin();
       i != other._bindings.end();
       ++i)
  {
    Binding* bb = new Binding(*i->second);
    _bindings.insert(std::make_pair(i->first, bb));
  }

  for (Subscriptions::const_iterator i = other._subscriptions.begin();
       i != other._subscriptions.end();
       ++i)
  {
    Subscription* ss = new Subscription(*i->second);
    _subscriptions.insert(std::make_pair(i->first, ss));
  }

  _associated_uris = AssociatedURIs(other._associated_uris);
  _notify_cseq = other._notify_cseq;
  _timer_id = other._timer_id;
  _cas = other._cas;
  _uri = other._uri;
  _scscf_uri = other._scscf_uri;
}

/// Clear all the bindings and subscriptions from this object.
void AoR::clear(bool clear_emergency_bindings)
{
  for (Bindings::iterator i = _bindings.begin();
       i != _bindings.end();
       )
  {
    if ((clear_emergency_bindings) || (!i->second->_emergency_registration))
    {
      delete i->second;
      _bindings.erase(i++);
    }
    else
    {
      ++i;
    }
  }

  if (clear_emergency_bindings)
  {
    _bindings.clear();
  }

  for (Subscriptions::iterator i = _subscriptions.begin();
       i != _subscriptions.end();
       ++i)
  {
    delete i->second;
  }

  _subscriptions.clear();
  _associated_uris.clear_uris();
}


/// Retrieve a binding by binding identifier, creating an empty one if
/// necessary.  The created binding is completely empty, even the Contact URI
/// field.
Binding* AoR::get_binding(const std::string& binding_id)
{
  Binding* b;
  Bindings::const_iterator i = _bindings.find(binding_id);
  if (i != _bindings.end())
  {
    b = i->second;
  }
  else
  {
    // No existing binding with this id, so create a new one.
    b = new Binding(_uri);
    b->_expires = 0;
    _bindings.insert(std::make_pair(binding_id, b));
  }
  return b;
}


/// Removes any binding that had the given ID.  If there is no such binding,
/// does nothing.
void AoR::remove_binding(const std::string& binding_id)
{
  Bindings::iterator i = _bindings.find(binding_id);
  if (i != _bindings.end())
  {
    delete i->second;
    _bindings.erase(i);
  }
}

/// Retrieve a subscription by To tag, creating an empty subscription if
/// necessary.
Subscription* AoR::get_subscription(const std::string& to_tag)
{
  Subscription* s;
  Subscriptions::const_iterator i = _subscriptions.find(to_tag);
  if (i != _subscriptions.end())
  {
    s = i->second;
  }
  else
  {
    // No existing subscription with this tag, so create a new one.
    s = new Subscription;
    _subscriptions.insert(std::make_pair(to_tag, s));
  }
  return s;
}


/// Removes the subscription with the specified tag.  If there is no such
/// subscription, does nothing.
void AoR::remove_subscription(const std::string& to_tag)
{
  Subscriptions::iterator i = _subscriptions.find(to_tag);
  if (i != _subscriptions.end())
  {
    delete i->second;
    _subscriptions.erase(i);
  }
}

/// Remove all the bindings from an AOR object
void AoR::clear_bindings()
{
  for (Bindings::const_iterator i = _bindings.begin();
       i != _bindings.end();
       ++i)
  {
    delete i->second;
  }

  // Clear the bindings map.
  _bindings.clear();
}


Binding::Binding(std::string address_of_record) :
  _address_of_record(address_of_record),
  _cseq(0),
  _expires(0),
  _priority(0),
  _emergency_registration(false)
{}

/// Copy constructor.
Binding::Binding(const Binding& other)
{
  _address_of_record = other._address_of_record;
  _uri = other._uri;
  _cid = other._cid;
  _path_headers = other._path_headers;
  _path_uris = other._path_uris;
  _cseq = other._cseq;
  _expires = other._expires;
  _priority = other._priority;
  _params = other._params;
  _private_id = other._private_id;
  _emergency_registration = other._emergency_registration;
}

// Make sure assignment is deep!
Binding& Binding::operator= (Binding const& other)
{
  if (this != &other)
  {
    _address_of_record = other._address_of_record;
    _uri = other._uri;
    _cid = other._cid;
    _path_headers = other._path_headers;
    _path_uris = other._path_uris;
    _cseq = other._cseq;
    _expires = other._expires;
    _priority = other._priority;
    _params = other._params;
    _private_id = other._private_id;
    _emergency_registration = other._emergency_registration;
  }

  return *this;
}

bool Binding::operator==(const Binding& other) const
{
  if ((_address_of_record != other._address_of_record) ||
      (_uri != other._uri) ||
      (_cid != other._cid) ||
      (_path_headers != other._path_headers) ||
      (_path_uris != other._path_uris) ||
      (_cseq != other._cseq) ||
      (_expires != other._expires) ||
      (_priority != other._priority) ||
      (_params != other._params) ||
      (_private_id != other._private_id) ||
      (_emergency_registration != other._emergency_registration))
  {
    return false;
  }

  return true;
}

bool Binding::operator!=(const Binding& other) const
{
  return !(operator==(other));
}

void Binding::
  to_json(rapidjson::Writer<rapidjson::StringBuffer>& writer) const
{
  writer.StartObject();
  {
    writer.String(JSON_URI); writer.String(_uri.c_str());
    writer.String(JSON_CID); writer.String(_cid.c_str());
    writer.String(JSON_CSEQ); writer.Int(_cseq);
    writer.String(JSON_EXPIRES); writer.Int(_expires);
    writer.String(JSON_PRIORITY); writer.Int(_priority);

    writer.String(JSON_PARAMS);
    writer.StartObject();
    {
      for (std::map<std::string, std::string>::const_iterator p = _params.begin();
           p != _params.end();
           ++p)
      {
        writer.String(p->first.c_str()); writer.String(p->second.c_str());
      }
    }
    writer.EndObject();

    writer.String(JSON_PATH_HEADERS);
    writer.StartArray();
    {
      for (std::list<std::string>::const_iterator p = _path_headers.begin();
           p != _path_headers.end();
           ++p)
      {
        writer.String(p->c_str());
      }
    }
    writer.EndArray();

    writer.String(JSON_PRIVATE_ID); writer.String(_private_id.c_str());
    writer.String(JSON_EMERGENCY_REG); writer.Bool(_emergency_registration);
  }
  writer.EndObject();
}

void Binding::from_json(const rapidjson::Value& b_obj)
{

  JSON_GET_STRING_MEMBER(b_obj, JSON_URI, _uri);
  JSON_GET_STRING_MEMBER(b_obj, JSON_CID, _cid);
  JSON_GET_INT_MEMBER(b_obj, JSON_CSEQ, _cseq);
  JSON_GET_INT_MEMBER(b_obj, JSON_EXPIRES, _expires);
  JSON_GET_INT_MEMBER(b_obj, JSON_PRIORITY, _priority);

  JSON_ASSERT_CONTAINS(b_obj, JSON_PARAMS);
  JSON_ASSERT_OBJECT(b_obj[JSON_PARAMS]);
  const rapidjson::Value& params_obj = b_obj[JSON_PARAMS];

  for (rapidjson::Value::ConstMemberIterator params_it = params_obj.MemberBegin();
       params_it != params_obj.MemberEnd();
       ++params_it)
  {
    JSON_ASSERT_STRING(params_it->value);
    _params[params_it->name.GetString()] = params_it->value.GetString();
  }

  if (b_obj.HasMember(JSON_PATH_HEADERS))
  {
    JSON_ASSERT_ARRAY(b_obj[JSON_PATH_HEADERS]);
    const rapidjson::Value& path_headers_arr = b_obj[JSON_PATH_HEADERS];

    for (rapidjson::Value::ConstValueIterator path_headers_it = path_headers_arr.Begin();
         path_headers_it != path_headers_arr.End();
         ++path_headers_it)
    {
      JSON_ASSERT_STRING(*path_headers_it);
      _path_headers.push_back(path_headers_it->GetString());
    }
  }

  JSON_GET_STRING_MEMBER(b_obj, JSON_PRIVATE_ID, _private_id);
  JSON_GET_BOOL_MEMBER(b_obj, JSON_EMERGENCY_REG, _emergency_registration);
}

/// Copy constructor.
Subscription::Subscription(const Subscription& other)
{
  _req_uri = other._req_uri;
  _from_uri = other._from_uri;
  _from_tag = other._from_tag;
  _to_uri = other._to_uri;
  _to_tag = other._to_tag;
  _cid = other._cid;
  _refreshed = other._refreshed;
  _route_uris = other._route_uris;
  _expires = other._expires;
}

// Make sure assignment is deep!
Subscription& Subscription::operator= (Subscription const& other)
{
  if (this != &other)
  {
    _req_uri = other._req_uri;
    _from_uri = other._from_uri;
    _from_tag = other._from_tag;
    _to_uri = other._to_uri;
    _to_tag = other._to_tag;
    _cid = other._cid;
    _refreshed = other._refreshed;
    _route_uris = other._route_uris;
    _expires = other._expires;
  }

  return *this;
}

bool Subscription::operator==(const Subscription& other) const
{
  // Only comparing associated URIs and barring, not wildcard mappings
  if ((_req_uri != other._req_uri) ||
      (_from_uri != other._from_uri) ||
      (_from_tag != other._from_tag) ||
      (_to_uri != other._to_uri) ||
      (_to_tag != other._to_tag) ||
      (_cid != other._cid) ||
      (_refreshed != other._refreshed) ||
      (_route_uris != other._route_uris) ||
      (_expires != other._expires))
  {
    return false;
  }

  return true;
}

bool Subscription::operator!=(const Subscription& other) const
{
  return !(operator==(other));
}

void Subscription::
  to_json(rapidjson::Writer<rapidjson::StringBuffer>& writer) const
{
  writer.StartObject();
  {
    writer.String(JSON_REQ_URI); writer.String(_req_uri.c_str());
    writer.String(JSON_FROM_URI); writer.String(_from_uri.c_str());
    writer.String(JSON_FROM_TAG); writer.String(_from_tag.c_str());
    writer.String(JSON_TO_URI); writer.String(_to_uri.c_str());
    writer.String(JSON_TO_TAG); writer.String(_to_tag.c_str());
    writer.String(JSON_CID); writer.String(_cid.c_str());

    writer.String(JSON_ROUTES);
    writer.StartArray();
    {
      for (std::list<std::string>::const_iterator r = _route_uris.begin();
           r != _route_uris.end();
           ++r)
      {
        writer.String(r->c_str());
      }
    }
    writer.EndArray();

    writer.String(JSON_EXPIRES); writer.Int(_expires);
  }
  writer.EndObject();
}

void Subscription::from_json(const rapidjson::Value& s_obj)
{
  JSON_GET_STRING_MEMBER(s_obj, JSON_REQ_URI, _req_uri);
  JSON_GET_STRING_MEMBER(s_obj, JSON_FROM_URI, _from_uri);
  JSON_GET_STRING_MEMBER(s_obj, JSON_FROM_TAG, _from_tag);
  JSON_GET_STRING_MEMBER(s_obj, JSON_TO_URI, _to_uri);
  JSON_GET_STRING_MEMBER(s_obj, JSON_TO_TAG, _to_tag);
  JSON_GET_STRING_MEMBER(s_obj, JSON_CID, _cid);

  JSON_ASSERT_CONTAINS(s_obj, JSON_ROUTES);
  JSON_ASSERT_ARRAY(s_obj[JSON_ROUTES]);
  const rapidjson::Value& routes_arr = s_obj[JSON_ROUTES];

  for (rapidjson::Value::ConstValueIterator routes_it = routes_arr.Begin();
       routes_it != routes_arr.End();
       ++routes_it)
  {
    JSON_ASSERT_STRING(*routes_it);
    _route_uris.push_back(routes_it->GetString());
  }

  JSON_GET_INT_MEMBER(s_obj, JSON_EXPIRES, _expires);
}

int AoR::get_last_expires() const
{
  // Set a temp int to 0 to compare expiry times to.
  int last_expires = 0;

  for (BindingPair b : _bindings)
  {
    if (b.second->_expires > last_expires)
    {
      last_expires = b.second->_expires;
    }
  }

  for (SubscriptionPair s : _subscriptions)
  {
    if (s.second->_expires > last_expires)
    {
      last_expires = s.second->_expires;
    }
  }

  return last_expires;
}

// Utility function to return the expiry time of the binding or subscription due
// to expire next. If the function finds no expiry times in the bindings or
// subscriptions it returns 0. This function should never be called on an empty AoR,
// so a 0 is indicative of something wrong with the _expires values of AoR members.
int AoR::get_next_expires()
{
  // Set a temp int to INT_MAX to compare expiry times to.
  int _next_expires = INT_MAX;

  for (Bindings::const_iterator b = _bindings.begin();
       b != _bindings.end();
       ++b)
  {
    if (b->second->_expires < _next_expires)
    {
      _next_expires = b->second->_expires;
    }
  }
  for (Subscriptions::const_iterator s = _subscriptions.begin();
       s != _subscriptions.end();
       ++s)
  {
    if (s->second->_expires < _next_expires)
    {
      _next_expires = s->second->_expires;
    }
  }

  // If nothing has altered the _next_expires, the AoR is empty and invalid.
  // Return 0 to indicate there is nothing to expire.
  if (_next_expires == INT_MAX)
  {
    return 0;
  }
  // Otherwise we return the value found.
  return _next_expires;
}

void AoR::copy_aor(const AoR& source_aor)
{
  for (Bindings::const_iterator i = source_aor.bindings().begin();
       i != source_aor.bindings().end();
       ++i)
  {
    Binding* src = i->second;
    Binding* dst = get_binding(i->first);
    *dst = *src;
  }

  for (Subscriptions::const_iterator i = source_aor.subscriptions().begin();
       i != source_aor.subscriptions().end();
       ++i)
  {
    Subscription* src = i->second;
    Subscription* dst = get_subscription(i->first);
    *dst = *src;
  }

  _associated_uris = AssociatedURIs(source_aor._associated_uris);
  _notify_cseq = source_aor._notify_cseq;
  _timer_id = source_aor._timer_id;
  _uri = source_aor._uri;
  _scscf_uri = source_aor._scscf_uri;
}

// Patch the AoR. This does the following:
//
// TODO
void AoR::patch_aor(const PatchObject& po)
{
  TRC_DEBUG("Patching the AoR for %s", _uri.c_str());

  for (BindingPair b : po.get_update_bindings())
  {
    TRC_DEBUG("Updating the binding %s", b.first.c_str());

    for (BindingPair b2 : _bindings)
    {
      if (b.first == b2.first)
      {
        _bindings.erase(b2.first);
        delete b2.second;
        break;
      }
    }

    Binding* copy_b = new Binding(*(b.second));
    _bindings.insert(std::make_pair(b.first, copy_b));
  }

  for (std::string b_id : po.get_remove_bindings())
  {
    TRC_DEBUG("Removing the binding %s", b_id.c_str());

    for (BindingPair b : _bindings)
    {
      if (b_id == b.first)
      {
        _bindings.erase(b.first);
        delete b.second;
        break;
      }
    }
  }

  for (SubscriptionPair s : po.get_update_subscriptions())
  {
    TRC_DEBUG("Updating the subscription %s", s.first.c_str());

    for (SubscriptionPair s2 : _subscriptions)
    {
      if (s.first == s2.first)
      {
        _subscriptions.erase(s2.first);
        delete s2.second;
        break;
      }
    }

    Subscription* copy_s = new Subscription(*(s.second));
    _subscriptions.insert(std::make_pair(s.first, copy_s));
  }

  for (std::string s_id : po.get_remove_subscriptions())
  {
    TRC_DEBUG("Removing the subscription %s", s_id.c_str());

    for (SubscriptionPair s : _subscriptions)
    {
      if (s_id == s.first)
      {
        _subscriptions.erase(s.first);
        delete s.second;
        break;
      }
    }
  }

  if (po.get_associated_uris())
  {
    TRC_DEBUG("Updating the Associated URIs");
    _associated_uris = po.get_associated_uris().get();
  }

  if (po.get_increment_cseq())
  {
    _notify_cseq++;
  }

  if ((po.get_minimum_cseq() != 0) && (_notify_cseq < po.get_minimum_cseq()))
  {
    _notify_cseq = po.get_minimum_cseq();
  }
}

PatchObject::PatchObject() :
  _update_bindings({}),
  _remove_bindings({}),
  _update_subscriptions({}),
  _remove_subscriptions({}),
  _associated_uris(boost::optional<AssociatedURIs>{}),
  _minimum_cseq(0),
  _increment_cseq(false)
{}

PatchObject::~PatchObject()
{
  clear();
}

/// Copy constructor.
PatchObject::PatchObject(const PatchObject& other)
{
  common_constructor(other);
}

// Make sure assignment is deep!
PatchObject& PatchObject::operator= (PatchObject const& other)
{
  if (this != &other)
  {
    clear();
    common_constructor(other);
  }

  return *this;
}

void PatchObject::common_constructor(const PatchObject& other)
{
  for (BindingPair binding : other.get_update_bindings())
  {
    _update_bindings.insert(
                 std::make_pair(binding.first, new Binding(*(binding.second))));
  }

  for (std::string binding : other.get_remove_bindings())
  {
    _remove_bindings.push_back(binding);
  }

  for (SubscriptionPair subscription : other.get_update_subscriptions())
  {
    _update_subscriptions.insert(
                      std::make_pair(subscription.first,
                                     new Subscription(*(subscription.second))));
  }

  for (std::string subscription : other.get_remove_subscriptions())
  {
    _remove_subscriptions.push_back(subscription);
  }

  if (other.get_associated_uris())
  {
    _associated_uris = other.get_associated_uris().get();
  }

  _minimum_cseq = other.get_minimum_cseq();
  _increment_cseq = other.get_increment_cseq();
}

void PatchObject::clear()
{
  for (BindingPair b : _update_bindings)
  {
    delete b.second; b.second = NULL;
  }

  for (SubscriptionPair s : _update_subscriptions)
  {
    delete s.second; s.second = NULL;
  }
}

/// Convert an AoR to a PatchObject.
///
/// @param aor - AoR to convert to a PatchObject
/// @param po  - PatchObject to be populated with the AoR info
void convert_aor_to_patch(const AoR& aor, PatchObject& po)
{
  Bindings patch_bindings;

  for (BindingPair binding : aor.bindings())
  {
    patch_bindings.insert(
                 std::make_pair(binding.first, new Binding(*(binding.second))));
  }

  Subscriptions patch_subscriptions;

  for (SubscriptionPair subscription : aor.subscriptions())
  {
    patch_subscriptions.insert(
                      std::make_pair(subscription.first,
                                     new Subscription(*(subscription.second))));
  }

  AssociatedURIs associated_uris = aor._associated_uris;
  int minimum = aor._notify_cseq;

  po.set_update_bindings(patch_bindings);
  po.set_update_subscriptions(patch_subscriptions);
  po.set_associated_uris(associated_uris);
  po.set_minimum_cseq(minimum);
}
