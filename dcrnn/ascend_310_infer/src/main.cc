/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/time.h>
#include <gflags/gflags.h>
#include <dirent.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <iosfwd>
#include <vector>
#include <fstream>
#include <sstream>

#include "include/api/model.h"
#include "include/api/context.h"
#include "include/api/serialization.h"
#include "include/api/types.h"
#include "include/dataset/vision.h"
#include "include/dataset/execute.h"
#include "acl/acl.h"
#include "../inc/utils.h"

using mindspore::Context;
using mindspore::Serialization;
using mindspore::Model;
using mindspore::Status;
using mindspore::ModelType;
using mindspore::Graph;
using mindspore::GraphCell;
using mindspore::kSuccess;
using mindspore::MSTensor;
using mindspore::DataType;
using mindspore::dataset::Execute;
using mindspore::dataset::InterpolationMode;
using mindspore::dataset::vision::Decode;
using mindspore::dataset::vision::Resize;
using mindspore::dataset::vision::Normalize;
using mindspore::dataset::vision::HWC2CHW;
using mindspore::dataset::vision::SwapRedBlue;
using mindspore::dataset::transforms::TypeCast;
DEFINE_string(mindir_path, "../../DCRNN_mindir.mindir", "mindir path");
DEFINE_string(fusion_switch_path, ".", "fusion switch path");
DEFINE_string(dataset_path, "../../preprocess_Result/data", "dataset path");
DEFINE_int32(device_id, 0, "device id");

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (RealPath(FLAGS_mindir_path).empty()) {
        std::cout << "Invalid mindir." << std::endl;
        return 1;
    }
    if (RealPath(FLAGS_fusion_switch_path).empty()) {
        std::cout << "Invalid fusion switch path" << std::endl;
        return 1;
    }
    auto context = std::make_shared<Context>();
    auto ascend310 = std::make_shared<mindspore::Ascend310DeviceInfo>();
    ascend310->SetDeviceID(FLAGS_device_id);
    context->MutableDeviceInfo().push_back(ascend310);
    mindspore::Graph graph;
    Serialization::Load(FLAGS_mindir_path, ModelType::kMindIR, &graph);
    if (!FLAGS_fusion_switch_path.empty()) {
        ascend310->SetFusionSwitchConfigPath(FLAGS_fusion_switch_path);
    }
    Model model;
    Status ret = model.Build(GraphCell(graph), context);
    if (ret != kSuccess) {
        std::cout << "ERROR: Build failed." << std::endl;
        return 1;
    }
    auto all_files = GetAllFiles(FLAGS_dataset_path);
    if (all_files.empty()) {
        std::cout << "ERROR: no input data." << std::endl;
        return 1;
    }
    std::vector<MSTensor> modelInputs = model.GetInputs();
    std::map<double, double> costTime_map;
    size_t size = all_files.size();
    for (size_t i = 0; i < size; ++i) {
        struct timeval start = {0};
        struct timeval end = {0};
        double startTimeMs = 0;
        double endTimeMs = 0;
        std::vector<MSTensor> inputs;
        std::vector<MSTensor> outputs;
        std::cout << "==> Image: " << all_files[i] << std::endl;
        MSTensor image = ReadFileToTensor(all_files[i]);
        inputs.emplace_back(modelInputs[0].Name(), modelInputs[0].DataType(), modelInputs[0].Shape(),
                            image.Data().get(), image.DataSize());
        gettimeofday(&start, nullptr);
        std::cout<< "==>Input Successfully" << std::endl;
        ret = model.Predict(inputs, &outputs);
        gettimeofday(&end, nullptr);
        if (ret != kSuccess) {
            std::cout << "Predict " << all_files[i] << " failed." << std::endl;
            return 1;
        }
        startTimeMs = (1.0 * start.tv_sec * 1000000 + start.tv_usec) / 1000;
        endTimeMs = (1.0 * end.tv_sec * 1000000 + end.tv_usec) / 1000;
        costTime_map.insert(std::pair<double, double>(startTimeMs, endTimeMs));
        WriteResult(all_files[i], outputs);
    }
    double average = 0.0;
    int inferCount = 0;
    for (auto iter = costTime_map.begin(); iter != costTime_map.end(); iter++) {
        average += iter->second - iter->first;
        inferCount++;
    }
    average = average / inferCount;
    std::stringstream timeCost;
    timeCost << "NN inference cost average time: " << average << " ms of infer_count " << inferCount << std::endl;
    std::cout << "NN inference cost average time: " << average << "ms of infer_count " << inferCount << std::endl;
    std::string fileName = "./time_Result" + std::string("/test_perform_static.txt");
    std::ofstream fileStream(fileName.c_str(), std::ios::trunc);
    fileStream << timeCost.str();
    fileStream.close();
    costTime_map.clear();
    return 0;
}
