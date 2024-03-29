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

#include "../communication_abstractions/orocos_rtt_communicator.hpp"
#include "../communication_abstractions/buffer_communication.hpp"
#include "data_runner_factory.hpp"

#include <performance_test/for_each.hpp>

#include <string>
#include <memory>

#ifdef PERFORMANCE_TEST_CALLBACK_EXECUTOR_ENABLED
  #include "../communication_abstractions/ros2_callback_communicator.hpp"
#endif
#include "../communication_abstractions/ros2_pal_waitset_communicator.hpp"

#ifdef PERFORMANCE_TEST_POLLING_SUBSCRIPTION_ENABLED
  #include "../communication_abstractions/ros2_waitset_communicator.hpp"
#endif

#ifdef PERFORMANCE_TEST_FASTRTPS_ENABLED
  #include "../communication_abstractions/fast_rtps_communicator.hpp"
#endif

#ifdef PERFORMANCE_TEST_CONNEXTDDSMICRO_ENABLED
  #include "../communication_abstractions/connext_dds_micro_communicator.hpp"
#endif

#ifdef PERFORMANCE_TEST_OPENDDS_ENABLED
  #include "../communication_abstractions/opendds_communicator.hpp"
#endif

#ifdef PERFORMANCE_TEST_CYCLONEDDS_ENABLED
  #include "../communication_abstractions/cyclonedds_communicator.hpp"
#endif
#include "data_runner.hpp"
#include "../experiment_configuration/topics.hpp"

namespace performance_test
{

std::shared_ptr<DataRunnerBase> DataRunnerFactory::get(
  const std::string & requested_topic_name,
  CommunicationMean com_mean,
  const RunType run_type)
{
  std::shared_ptr<DataRunnerBase> ptr;
  performance_test::for_each(
    topics::TopicTypeList(),
    [&ptr, requested_topic_name, com_mean, run_type](const auto & topic) {
      using T = std::remove_cv_t<std::remove_reference_t<decltype(topic)>>;
      if (T::topic_name() == requested_topic_name) {
        if (ptr) {
          throw std::runtime_error("It seems that two topics have the same name");
        }
#ifdef PERFORMANCE_TEST_CALLBACK_EXECUTOR_ENABLED
        if (com_mean == CommunicationMean::ROS2) {
          ptr = std::make_shared<DataRunner<ROS2CallbackCommunicator<T>>>(run_type);
        }
#endif
        if (com_mean == CommunicationMean::ROS2PALPollingSubscription) {
          ptr = std::make_shared<DataRunner<ROS2PALWaitsetCommunicator<T>>>(run_type);
        }
        if (com_mean == CommunicationMean::Orocos) {
          ptr = std::make_shared<DataRunner<OrocosCommunicator<T>>>(run_type);
        }
        if (com_mean == CommunicationMean::Buffer) {
          ptr = std::make_shared<DataRunner<BufferCommunicator<T>>>(run_type);
        }
#ifdef PERFORMANCE_TEST_POLLING_SUBSCRIPTION_ENABLED
        if (com_mean == CommunicationMean::ROS2PollingSubscription) {
          ptr = std::make_shared<DataRunner<ROS2WaitsetCommunicator<T>>>(run_type);
        }
#endif
#ifdef PERFORMANCE_TEST_FASTRTPS_ENABLED
        if (com_mean == CommunicationMean::FASTRTPS) {
          ptr = std::make_shared<DataRunner<FastRTPSCommunicator<T>>>(run_type);
        }
#endif
#ifdef PERFORMANCE_TEST_CONNEXTDDSMICRO_ENABLED
        if (com_mean == CommunicationMean::CONNEXTDDSMICRO) {
          ptr = std::make_shared<DataRunner<RTIMicroDDSCommunicator<T>>>(run_type);
        }
#endif

#ifdef PERFORMANCE_TEST_CYCLONEDDS_ENABLED
        if (com_mean == CommunicationMean::CYCLONEDDS) {
          ptr = std::make_shared<DataRunner<CycloneDDSCommunicator<T>>>(run_type);
        }
#endif
#ifdef PERFORMANCE_TEST_OPENDDS_ENABLED
        if (com_mean == CommunicationMean::OPENDDS) {
          ptr = std::make_shared<DataRunner<OpenDDSCommunicator<T>>>(run_type);
        }
#endif
      }
    });
  if (!ptr) {
    throw std::runtime_error(
            "A topic with the requested name does not exist or communication mean not supported.");
  }
  return ptr;
}

}  // namespace performance_test
