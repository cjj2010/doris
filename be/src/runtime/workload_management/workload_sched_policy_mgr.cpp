// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "runtime/workload_management/workload_sched_policy_mgr.h"

#include <memory>

#include "runtime/fragment_mgr.h"
#include "runtime/workload_management/resource_context.h"

namespace doris {

void WorkloadSchedPolicyMgr::start(ExecEnv* exec_env) {
    _stop_latch.reset(1);
    _exec_env = exec_env;

    Status st;
    st = Thread::create(
            "workload", "workload_scheduler", [this]() { this->_schedule_workload(); }, &_thread);
    if (!st.ok()) {
        LOG(WARNING) << "create workload scheduler thread failed";
    } else {
        LOG(INFO) << "start workload scheduler ";
    }
}

void WorkloadSchedPolicyMgr::stop() {
    std::lock_guard<std::shared_mutex> write_lock(_stop_lock);
    if (_stop_latch.count() == 0) {
        LOG(INFO) << "workload schedule manager is already stopped. ";
        return;
    }
    _stop_latch.count_down();
    if (_thread) {
        _thread->join();
    }
    LOG(INFO) << "workload schedule manager stopped, thread is finished. ";
}

void WorkloadSchedPolicyMgr::update_workload_sched_policy(
        std::map<uint64_t, std::shared_ptr<WorkloadSchedPolicy>> new_policy_map) {
    std::lock_guard<std::shared_mutex> write_lock(_policy_lock);
    // 1 upsert
    for (const auto& [id, policy] : new_policy_map) {
        if (_id_policy_map.find(id) == _id_policy_map.end()) {
            _id_policy_map.emplace(id, policy);
        } else {
            if (policy->version() > _id_policy_map.at(id)->version()) {
                _id_policy_map[id] = policy;
            }
        }
    }

    // 2 delete
    for (auto iter = _id_policy_map.begin(); iter != _id_policy_map.end();) {
        if (new_policy_map.find(iter->first) == new_policy_map.end()) {
            iter = _id_policy_map.erase(iter);
        } else {
            iter++;
        }
    }
}

void WorkloadSchedPolicyMgr::_schedule_workload() {
    while (!_stop_latch.wait_for(std::chrono::milliseconds(500))) {
        // 1 get query resource context
        std::vector<std::weak_ptr<ResourceContext>> list;
        _exec_env->fragment_mgr()->get_runtime_query_info(&list);
        // todo: add timer
        if (list.empty()) {
            continue;
        }

        for (const auto& i : list) {
            auto resource_ctx = i.lock();
            if (resource_ctx == nullptr) {
                continue;
            }
            WorkloadAction::RuntimeContext action_runtime_ctx(resource_ctx);

            // 2 get matched policy
            std::map<WorkloadActionType, std::shared_ptr<WorkloadSchedPolicy>> matched_policy_map;
            {
                std::shared_lock<std::shared_mutex> read_lock(_policy_lock);
                for (auto& entity : _id_policy_map) {
                    auto& new_policy = entity.second;
                    if (new_policy->is_match(&action_runtime_ctx)) {
                        WorkloadActionType new_policy_type = new_policy->get_first_action_type();
                        if (matched_policy_map.find(new_policy_type) == matched_policy_map.end() ||
                            new_policy->priority() >
                                    matched_policy_map.at(new_policy_type)->priority()) {
                            matched_policy_map[new_policy_type] = new_policy;
                        }
                    }
                }
            }

            if (matched_policy_map.empty()) {
                continue;
            }
            LOG(INFO) << "[workload_schedule] matched policy size=" << matched_policy_map.size();
            // 3 check action conflicts
            if (matched_policy_map.find(WorkloadActionType::MOVE_QUERY_TO_GROUP) !=
                        matched_policy_map.end() &&
                matched_policy_map.find(WorkloadActionType::CANCEL_QUERY) !=
                        matched_policy_map.end()) {
                // compare priority
                int move_prio =
                        matched_policy_map.at(WorkloadActionType::MOVE_QUERY_TO_GROUP)->priority();
                int cancel_prio =
                        matched_policy_map.at(WorkloadActionType::CANCEL_QUERY)->priority();
                if (cancel_prio >= move_prio) {
                    matched_policy_map.erase(WorkloadActionType::MOVE_QUERY_TO_GROUP);
                } else {
                    matched_policy_map.erase(WorkloadActionType::CANCEL_QUERY);
                }
            }

            // 4 exec policy action
            for (const auto& [key, value] : matched_policy_map) {
                value->exec_action(&action_runtime_ctx);
            }
        }
    }
}

} // namespace doris