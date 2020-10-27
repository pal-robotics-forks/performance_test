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

#ifndef COMMUNICATION_ABSTRACTIONS__BUFFER_COMMUNICATOR_HPP_
#define COMMUNICATION_ABSTRACTIONS__BUFFER_COMMUNICATOR_HPP_
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <rclcpp/wait_set.hpp>

#include "static_circular_buffer.h"
#include "../experiment_configuration/topics.hpp"
#include "communicator.hpp"
#include "resource_manager.hpp"
#include "../experiment_configuration/qos_abstraction.hpp"
namespace performance_test
{

/// Communication plugin for ROS 2 using waitsets for the subscription side.
template<class Topic>
class BufferCommunicator : public Communicator
{
public:
  /// The data type to publish and subscribe to.
  using DataType = typename Topic::RosType;

  /// Constructor which takes a reference \param lock to the lock to use.
  explicit BufferCommunicator(SpinLock & lock)
  : Communicator(lock){}

  template<class T>
  void callback(const T & data)
  {
    const std::lock_guard<decltype(this->get_lock())> lockg(this->get_lock());
    static_assert(std::is_same<DataType,
                               typename std::remove_cv<typename std::remove_reference<T>::type>::type>::value,
                  "Parameter type passed to callback() does not match");
    if (m_prev_timestamp >= data.time) {
      throw std::runtime_error(
        "Data consistency violated. Received sample with not strictly older timestamp");
    }

    if (m_ec.roundtrip_mode() == ExperimentConfiguration::RoundTripMode::RELAY) {
      unlock();
      publish(data, std::chrono::nanoseconds(data.time));
      lock();
    } else {
      m_prev_timestamp = data.time;
      update_lost_samples_counter(data.id);
      add_latency_to_statistics(data.time);
    }
    increment_received();
  }

  void publish(typename Topic::MessageUniquePtr data, const std::chrono::nanoseconds time)
  {
    lock();
    data->id = next_sample_id();
    data->time = time.count();
    increment_sent();  // We increment before publishing so we don't have to lock twice.
    unlock();
    m_buffer.push_back() = std::shared_ptr<DataType>(std::move(data));
  }
  void publish(const DataType & data, const std::chrono::nanoseconds time)
  {
    *m_data_copy = data;
    publish(*m_data_copy, time);
  }

  /// Reads received data from ROS 2 using waitsets
  void update_subscription()
  {
    std::shared_ptr<DataType> msg = m_buffer.front();
    m_buffer.pop_front();
    if (msg.get()) {
      this->template callback(*msg);
    }
  }

  std::size_t data_received()
  {
    return num_received_samples() * sizeof(DataType);
  }


private:
  static StaticCircularBuffer<std::shared_ptr<DataType>> m_buffer;
  std::unique_ptr<DataType> m_data_copy;
};
template <class Topic>
StaticCircularBuffer<std::shared_ptr<typename Topic::RosType>> BufferCommunicator<Topic>::m_buffer = StaticCircularBuffer<std::shared_ptr<typename Topic::RosType>>(5, std::shared_ptr<typename Topic::RosType>(nullptr));

}  // namespace performance_test

#endif  // COMMUNICATION_ABSTRACTIONS__ROS2_WAITSET_COMMUNICATOR_HPP_
