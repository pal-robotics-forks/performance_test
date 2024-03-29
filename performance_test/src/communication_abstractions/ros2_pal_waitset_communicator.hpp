// Copyright 2017 Apex.AI, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMMUNICATION_ABSTRACTIONS__ROS2_PAL_COMMUNICATOR_HPP_
#define COMMUNICATION_ABSTRACTIONS__ROS2_PAL_COMMUNICATOR_HPP_

#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "../experiment_configuration/topics.hpp"
#include "../experiment_configuration/qos_abstraction.hpp"
#include <rclcpp/wait_set.hpp>
#include "ros2_communicator.hpp"
#include "resource_manager.hpp"

namespace performance_test
{

/// Communication plugin for ROS 2 using waitsets for the subscription side.
template<class Topic>
class ROS2PALWaitsetCommunicator : public ROS2Communicator<Topic>
{
public:
  /// The data type to publish and subscribe to.
  using DataType = typename ROS2Communicator<Topic>::DataType;

  /// Constructor which takes a reference \param lock to the lock to use.
  explicit ROS2PALWaitsetCommunicator(SpinLock & lock)
  : ROS2Communicator<Topic>(lock),
    m_subscription(nullptr) {}

  /// Reads received data from ROS 2 using waitsets
  void update_subscription() override
  {
    bool use_intra = this->m_ec.intraprocess();
    if (!m_subscription) {
      auto options = rclcpp::SubscriptionOptionsWithAllocator<TLSFAllocator<void>>();
//      auto options = rclcpp::SubscriptionOptions();
      options.allocator = this->m_alloc;
      if (use_intra)
      {
        options.use_intra_process_comm = rclcpp::IntraProcessSetting::Enable;
      }

      // The memory strategy holds 1 instance of messages to be reused
//      auto memory_strategy =
//        std::make_shared<MessagePoolMemoryStrategy<DataType, 1>>();
      auto memory_strategy =
        std::make_shared<rclcpp::message_memory_strategy::MessageMemoryStrategy<DataType, TLSFAllocator<void>>>(this->m_alloc);
      m_subscription = this->m_node->template create_subscription<DataType>(
        Topic::topic_name() + this->m_ec.sub_topic_postfix(), this->m_ROS2QOSAdapter,
        [this](typename Topic::MessageUniquePtr data) {this->callback(std::move(data));}, options, memory_strategy);
#ifdef PERFORMANCE_TEST_POLLING_SUBSCRIPTION_ENABLED

      if (this->m_ec.expected_num_pubs() > 0) {
//        m_subscription->wait_for_matched(this->m_ec.expected_num_pubs(),
//                                         this->m_ec.expected_wait_for_matched_timeout());
      }
#endif
      m_waitset = std::make_unique<rclcpp::WaitSet>();
      m_waitset->add_subscription(m_subscription);
    }
    rclcpp::MessageInfo msg_info;
    const auto wait_ret = m_waitset->wait(std::chrono::milliseconds(100));
    if (wait_ret.kind() == rclcpp::Ready) {
      if (use_intra)
      {
        // Process intra process messages through the waitable
        m_subscription->get_intra_process_waitable()->execute();
      }
      else
      {
        using MessageAllocTraits =
          rclcpp::allocator::AllocRebind<DataType, TLSFAllocator<void>>;
        using MessageAlloc = typename MessageAllocTraits::allocator_type;
        using MessageDeleter = rclcpp::allocator::Deleter<MessageAlloc, DataType>;
        using MessageUniquePtr = std::unique_ptr<DataType, MessageDeleter>;
        MessageDeleter message_deleter;
        MessageAlloc message_alloc = *ResourceManager::get().get_allocator();
        auto ptr = MessageAllocTraits::allocate(message_alloc, 1);
        MessageAllocTraits::construct(message_alloc, ptr);
        MessageUniquePtr msg(ptr, message_deleter);
        rclcpp::allocator::set_allocator_for_deleter(&message_deleter, &message_alloc);

        bool success = m_subscription->take(*msg, msg_info);

        if (success) {
          this->template callback(std::move(msg));
        }
      }
  }
  }

private:
  std::shared_ptr<::rclcpp::Subscription<DataType, TLSFAllocator<void>, rclcpp::message_memory_strategy::MessageMemoryStrategy<
                                                                          DataType, TLSFAllocator<void>>>> m_subscription;
  std::unique_ptr<rclcpp::WaitSet> m_waitset;
};

}  // namespace performance_test

#endif  // COMMUNICATION_ABSTRACTIONS__ROS2_WAITSET_COMMUNICATOR_HPP_
