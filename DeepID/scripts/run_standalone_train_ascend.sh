#!/bin/bash
# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

if [ $# != 2 ]
then
    echo "Usage: bash eval_ascend.sh [DEVICE_NUM] [DEVICE_ID]"
    exit 1
fi
export DEVICE_NUM=$1
export DEVICE_ID=$2

echo "start training for device $DEVICE_ID"
env > env.log
python train.py --device_num=$DEVICE_NUM --device_id=$DEVICE_ID --device_target="Ascend" > log_train_standalone.txt 2>&1 &

cd ..
