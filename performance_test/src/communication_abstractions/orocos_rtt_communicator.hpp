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

#ifndef COMMUNICATION_ABSTRACTIONS__OROCOS_COMMUNICATOR_HPP_
#define COMMUNICATION_ABSTRACTIONS__OROCOS_COMMUNICATOR_HPP_

#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "../experiment_configuration/topics.hpp"
#include "../experiment_configuration/qos_abstraction.hpp"
#include <rclcpp/wait_set.hpp>
#include "communicator.hpp"
#include "resource_manager.hpp"

#include <rtt/InputPort.hpp>
#include <rtt/OutputPort.hpp>
namespace performance_test
{

/// Communication plugin for ROS 2 using waitsets for the subscription side.
template<class Topic>
class OrocosCommunicator : public Communicator
{
public:
  /// The data type to publish and subscribe to.
  using DataType = typename Topic::RosType;

  /// Constructor which takes a reference \param lock to the lock to use.
  explicit OrocosCommunicator(SpinLock & lock)
  : Communicator(lock){}

  template<class T>
  void callback(const T & data)
  {
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

  void publish(DataType & data, const std::chrono::nanoseconds time)
  {
    if (!m_output_port) {
      m_output_port = std::make_unique<RTT::OutputPort<DataType>>(Topic::topic_name() + "Output", true);
    }
    lock();
    data.id = next_sample_id();
    data.time = time.count();
    increment_sent();  // We increment before publishing so we don't have to lock twice.
    unlock();
    m_output_port->write(data);
  }

  void publish(const DataType & data, const std::chrono::nanoseconds time)
  {
    *m_data_copy = data;
    publish(*m_data_copy, time);
  }

  /// Reads received data from ROS 2 using waitsets
  void update_subscription()
  {
    if (!m_input_port) {
      if (!m_output_port)
      {
        // Ignore subscription because publisher still not available
        std::cerr << "ignoring subscription because orocos output port not available yet " << std::endl;
        return;
      }
      m_input_port = std::make_unique<RTT::InputPort<DataType>>(Topic::topic_name() + "Input",
                                                                 RTT::ConnPolicy::data());

      m_output_port->createConnection(*m_input_port);
    }
    {
    DataType msg;
    const std::lock_guard<decltype(this->get_lock())> lock(this->get_lock());
    bool success = m_input_port->read(msg);
      if (success) {
        this->template callback(msg);
      }
    }
  }

  std::size_t data_received()
  {
    return num_received_samples() * sizeof(DataType);
  }


private:
  std::unique_ptr<RTT::InputPort<DataType>> m_input_port;
  static std::unique_ptr<RTT::OutputPort<DataType>> m_output_port;
  std::unique_ptr<DataType> m_data_copy;
};
template <class Topic>
std::unique_ptr<RTT::OutputPort<typename Topic::RosType>> OrocosCommunicator<Topic>::m_output_port = {};

}  // namespace performance_test

#endif  // COMMUNICATION_ABSTRACTIONS__ROS2_WAITSET_COMMUNICATOR_HPP_
