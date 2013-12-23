/******************************************************************************
** Copyright (c) 2013 Intel Corporation All Rights Reserved
**
** Licensed under the Apache License, Version 2.0 (the "License"); you may not
** use this file except in compliance with the License.
**
** You may obtain a copy of the License at
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
** WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**
** See the License for the specific language governing permissions and
** limitations under the License.
**
******************************************************************************/
#include "PerformanceControlStaticCaps.h"
#include "StatusFormat.h"
#include "XmlNode.h"

PerformanceControlStaticCaps::PerformanceControlStaticCaps(Bool dynamicPerformanceControlStates) :
    m_dynamicPerformanceControlStates(dynamicPerformanceControlStates)
{
}

Bool PerformanceControlStaticCaps::supportsDynamicPerformanceControlStates(void) const
{
    return m_dynamicPerformanceControlStates;
}

XmlNode* PerformanceControlStaticCaps::getXml(void)
{
    XmlNode* root = XmlNode::createWrapperElement("performance_control_static_caps");

    root->addChild(XmlNode::createDataElement("dynamic_performance_control_states", StatusFormat::friendlyValue(m_dynamicPerformanceControlStates)));

    return root;
}