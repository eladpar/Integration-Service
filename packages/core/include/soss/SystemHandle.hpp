/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#ifndef SOSS__SYSTEMHANDLE_HPP
#define SOSS__SYSTEMHANDLE_HPP

#include <soss/detail/SystemHandle-head.hpp>

#include <soss/Message.hpp>

#include <yaml-cpp/yaml.h>

#include <set>
#include <string>
#include <vector>
#include <functional>

namespace soss {

//==============================================================================
/// Call this macro in a .cpp file of your plugin library so that soss can find
/// your SystemHandle implementation when your plugin library gets dynamically
/// loaded. Example:
///
///     SOSS_REGISTER_SYSTEM("my_middleware", my::middleware::SystemHandle)
///
/// The first argument should be a string representing the name of the
/// middleware. This should match the name in the `system:` dictionary of your
/// soss configuration file. Each middleware should have a unique name.
///
/// The second argument should be the literal type (not a string) of the class
/// that implements SystemHandle in your plugin library.
#define SOSS_REGISTER_SYSTEM(middleware_name_str, SystemType) \
  DETAIL_SOSS_REGISTER_SYSTEM(middleware_name_str, SystemType)

//==============================================================================
struct RequiredTypes
{
  std::set<std::string> messages;
  std::set<std::string> services;
};

//==============================================================================
/// SystemHandle is the base interface class for all middleware systems. All
/// middleware systems that want to interact with soss must implement at least
/// this interface. Depending on the type of middleware, it should also
/// implement the derived classes using multiple inheritance:
///
/// - TopicSubscriberSystem
/// - TopicPublisherSystem
/// - ServiceClientSystem
/// - ServiceProviderSystem

//Shorthand for DynamicTypes management
namespace xtypes = dds::core::xtypes;

using TypeRegistry = std::map<std::string, xtypes::DynamicType::Ptr>;

class SystemHandle
{
public:

  SystemHandle() = default;

  /// \brief Configure the soss handle for this system
  ///
  /// \param[in] types
  ///   The set of types (messages and services) that this middleware needs to
  ///   support. The system handle must register this types to the TypeRegistry.
  ///
  /// \param[in] configuration
  ///   The configuration specific to this system handle, as described in the
  ///   user-provided yaml input file.
  ///
  /// \param[in] type_register
  ///      The set of type definitions that this middleware is able to support.
  ///
  /// \return true if configuration was successful, otherwise false
  virtual bool configure(
      const RequiredTypes& types,
      const YAML::Node& configuration,
      TypeRegistry& type_registry) = 0;

  /// Is the system handle still working
  virtual bool okay() const = 0;

  /// Implicit conversion, same as okay()
  inline operator bool() const { return okay(); }

  /// \brief Tell the system handle to spin once, e.g. read through its
  /// subscriptions.
  ///
  /// \returns true if the system handle is still working
  virtual bool spin_once() = 0;

  // SystemHandle objects should not be copied or moved
  SystemHandle(const SystemHandle&) = delete;
  SystemHandle& operator=(const SystemHandle&) = delete;
  SystemHandle(SystemHandle&&) = delete;
  SystemHandle& operator=(SystemHandle&&) = delete;

  virtual ~SystemHandle() = default;
};

//==============================================================================
class TopicSubscriberSystem : public virtual SystemHandle
{
public:

  using SubscriptionCallback = std::function<void(const xtypes::DynamicData& message)>;

  /// \brief Have this node subscribe to a topic
  ///
  /// \param[in] topic_name
  ///   Name of the topic to subscribe to
  ///
  /// \param[in] message_type
  ///   Message type that this topic should expect
  ///
  /// \param[in] callback
  ///   The callback that should be triggered when a message comes in
  ///
  /// \param[in] configuration
  ///   A yaml node containing any middleware-specific configuration information
  ///   for this subscription. This may be an empty node.
  ///
  /// \returns true if subscription was successful
  virtual bool subscribe(
      const std::string& topic_name,
      const xtypes::DynamicType& message_type,
      SubscriptionCallback callback,
      const YAML::Node& configuration) = 0;
};

//==============================================================================
/// TopicPublisher is the abstract interface for objects that can act as
/// publisher proxies. These objects should be generated by
/// TopicPublisherSystem::advertise(~) in the next class.
class TopicPublisher
{
public:

  /// \brief Publish to a topic
  ///
  /// \param[in] topic_name
  ///   Name of the topic to publish
  ///
  /// \param[in] message
  ///   DynamicData that's being published
  virtual bool publish(const xtypes::DynamicData& message) = 0;

  virtual ~TopicPublisher() = default;
};

//==============================================================================
class TopicPublisherSystem : public virtual SystemHandle
{
public:

  /// \brief Advertise the ability to publish to a topic.
  ///
  /// \param[in] topic_name
  ///   Name of the topic to advertise
  ///
  /// \param[in] message_type
  ///  Message type that this node will publish
  ///
  /// \param[in] configuration
  ///   A yaml node containing any middleware-specific configuration information
  ///   for this publisher. This may be an empty node.
  ///
  /// \returns true if the advertisement was successful
  virtual std::shared_ptr<TopicPublisher> advertise(
      const std::string& topic_name,
      const xtypes::DynamicType& message_type,
      const YAML::Node& configuration) = 0;
};

//==============================================================================
class TopicSystem
    : public virtual TopicPublisherSystem,
      public virtual TopicSubscriberSystem { };

//==============================================================================
/// ServiceClient is the abstract interface for objects that can act as client
/// proxies. This class is different from ServiceClientSystem (below), because
/// ServiceClientSystem is the interface for SystemHandle types that are able to
/// support client proxies, whereas ServiceClient is the interface for the
/// client proxy objects themselves.
class ServiceClient
{
public:

  /// \brief Receive the response of a service request
  ///
  /// \attention Services are assumed to all be asynchronous (non-blocking), so
  /// this function may be called by multiple threads at once. ServiceClientNode
  /// implementers must make sure that they can handle multiple simultaneous
  /// calls to this function.
  ///
  /// \param[in] call_handle
  ///   The handle that was given to the call by this ServiceClientNode. The
  ///   usage of the handle is determined by the ServiceClientNode
  ///   implementation. Typically receive_response(~) will cast this handle into
  ///   a useful object type that contains information on where to send the
  ///   service response message.
  ///
  /// \param[in] response
  ///   The message that represents the response from the service
  ///
  virtual void receive_response(
      std::shared_ptr<void> call_handle,
      const xtypes::DynamicData& response) = 0;

  virtual ~ServiceClient() = default;
};

//==============================================================================
class ServiceClientSystem : public virtual SystemHandle
{
public:

  /// Signature of the callback that gets triggered when a client has made a
  /// request.
  using RequestCallback =
    std::function<void(
      const xtypes::DynamicData& request,
      ServiceClient& client,
      std::shared_ptr<void> call_handle)>;

  /// \brief Create a proxy for a client
  ///
  /// \param[in] service_name
  ///   Name of the service for this client to listen to
  ///
  /// \param[in] service_type
  ///   Type of service to expect
  ///
  /// \param[in] callback
  ///   The callback that should be used when a request comes in from the
  ///   middleware
  ///
  /// \param[in] configuration
  ///   A yaml node containing any middleware-specific configuration information
  ///   for this service client. This may be an empty node.
  ///
  /// \returns true if a client proxy could be made
  virtual bool create_client_proxy(
      const std::string& service_name,
      const xtypes::DynamicType& service_type,
      RequestCallback callback,
      const YAML::Node& configuration) = 0;
};

//==============================================================================
class ServiceProvider
{
public:

  /// \brief Call a service
  ///
  /// \attention It is important that this function
  /// (1) is non-blocking and
  /// (2) calls client.receive_response(~) when the service finishes.
  ///
  /// \param[in] request
  ///   Request message for the service
  ///
  /// \param[in,out] client
  ///   The proxy for the client that is making the request
  ///
  /// \param[in] call_handle
  ///   A handle for the call. This usage of this handle is determined by the
  ///   ServiceClientNode implementation. The ServiceProviderNode should not
  ///   attempt to cast or modify it in any way; it should only be passed back
  ///   to the ServiceClientNode later when receive_response(~) is called.
  ///
  virtual void call_service(
      const xtypes::DynamicData& request,
      ServiceClient& client,
      std::shared_ptr<void> call_handle) = 0;

  virtual ~ServiceProvider() = default;
};

//==============================================================================
class ServiceProviderSystem : public virtual SystemHandle
{
public:

  /// \brief Create a proxy for a service
  ///
  /// \param[in] service_name
  ///   Name of the service to offer
  ///
  /// \param[in] service_type
  ///   Type of service being offered
  ///
  /// \param[in] configuration
  ///   A yaml node containing any middleware-specific configuration information
  ///   for this service provider. This may be an empty node.
  ///
  /// \returns true if the node can offer this service
  virtual std::shared_ptr<ServiceProvider> create_service_proxy(
      const std::string& service_name,
      const xtypes::DynamicType& service_type,
      const YAML::Node& configuration) = 0;
};

//==============================================================================
class ServiceSystem
    : public virtual ServiceClientSystem,
      public virtual ServiceProviderSystem { };

//==============================================================================
class FullSystem
    : public virtual TopicSystem,
      public virtual ServiceSystem { };

} // namespace soss

#endif // SOSS__SYSTEMHANDLE_HPP
