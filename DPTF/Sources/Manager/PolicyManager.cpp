/******************************************************************************
** Copyright (c) 2013-2016 Intel Corporation All Rights Reserved
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

#include "PolicyManager.h"
#include "DptfManager.h"
#include "WorkItemQueueManagerInterface.h"
#include "WIPolicyCreateAll.h"
#include "WIPolicyDestroy.h"
#include "EsifFileEnumerator.h"
#include "EsifServicesInterface.h"
#include "StatusFormat.h"
#include "MapOps.h"
#include "Utility.h"

using namespace StatusFormat;

PolicyManager::PolicyManager(DptfManagerInterface* dptfManager) : m_dptfManager(dptfManager),
    m_supportedPolicyList(dptfManager)
{
}

PolicyManager::~PolicyManager(void)
{
    destroyAllPolicies();
}

void PolicyManager::reloadAllPolicies(const std::string& dptfPolicyDirectoryPath)
{
    m_supportedPolicyList.update();
    createAllPolicies(dptfPolicyDirectoryPath);
}

void PolicyManager::createAllPolicies(const std::string& dptfPolicyDirectoryPath)
{
    try
    {
        // Queue up a work item and wait for the return.
        WorkItem* workItem = new WIPolicyCreateAll(m_dptfManager, dptfPolicyDirectoryPath);
        m_dptfManager->getWorkItemQueueManager()->enqueueImmediateWorkItemAndWait(workItem);
    }
    catch (...)
    {
        ManagerMessage message = ManagerMessage(
            m_dptfManager, FLF, "Failed while trying to enqueue and wait for WIPolicyCreateAll.");
        m_dptfManager->getEsifServices()->writeMessageError(message);
    }

    if (getPolicyCount() == 0)
    {
        throw dptf_exception("DPTF was not able to load any policies.");
    }
}

void PolicyManager::createPolicy(const std::string& policyFileName)
{
    UIntN firstAvailableIndex = Constants::Invalid;

    try
    {

        auto indexesInUse = MapOps<UIntN, std::shared_ptr<Policy>>::getKeys(m_policies);
        firstAvailableIndex = getFirstAvailableIndex(indexesInUse);
        m_policies[firstAvailableIndex] = std::make_shared<Policy>(m_dptfManager);

        // Create the policy.  This will end up calling the functions in the .dll/.so and will throw an
        // exception if it doesn't find a valid policy to load.
        m_policies[firstAvailableIndex]->createPolicy(policyFileName, firstAvailableIndex, m_supportedPolicyList);

        ManagerMessage message = ManagerMessage(m_dptfManager, FLF, "Policy has been created.");
        message.setPolicyIndex(firstAvailableIndex);
        message.addMessage("Policy Index", firstAvailableIndex);
        message.addMessage("Policy File Name", policyFileName);
        m_dptfManager->getEsifServices()->writeMessageInfo(message);
    }
    catch (policy_not_in_idsp_list ex)
    {
        destroyPolicy(firstAvailableIndex);
    }
    catch (...)
    {
        destroyPolicy(firstAvailableIndex);
        throw;
    }
}

void PolicyManager::destroyAllPolicies(void)
{
    auto policyIndexes = MapOps<UIntN, std::shared_ptr<Policy>>::getKeys(m_policies);
    for(auto index = policyIndexes.begin(); index != policyIndexes.end(); ++index)
    {
        if (m_policies[*index] != nullptr)
        {
            try
            {
                // Queue up a work item and wait for the return.
                WorkItem* workItem = new WIPolicyDestroy(m_dptfManager, *index);
                m_dptfManager->getWorkItemQueueManager()->enqueueImmediateWorkItemAndWait(workItem);
            }
            catch (...)
            {
                ManagerMessage message = ManagerMessage(m_dptfManager, FLF, "Failed while trying to enqueue and wait for WIPolicyDestroy.");
                message.addMessage("Policy Index", *index);
                m_dptfManager->getEsifServices()->writeMessageError(message);
            }
        }
    }
}

void PolicyManager::destroyPolicy(UIntN policyIndex)
{
    auto matchedPolicy = m_policies.find(policyIndex);
    if ((matchedPolicy != m_policies.end()) && (matchedPolicy->second != nullptr))
    {
        try
        {
            matchedPolicy->second->destroyPolicy();
        }
        catch (...)
        {
            ManagerMessage message = ManagerMessage(m_dptfManager, FLF, "Failed while trying to destroy policy.");
            message.addMessage("Policy Index", policyIndex);
            m_dptfManager->getEsifServices()->writeMessageError(message);
        }

        m_policies.erase(matchedPolicy);
    }
}

UIntN PolicyManager::getPolicyListCount(void) const
{
    return static_cast<UIntN>(m_policies.size());
}

Policy* PolicyManager::getPolicyPtr(UIntN policyIndex)
{
    auto matchedPolicy = m_policies.find(policyIndex);
    if ((matchedPolicy == m_policies.end()) || (matchedPolicy->second == nullptr))
    {
        throw policy_index_invalid();
    }

    return matchedPolicy->second.get();
}

void PolicyManager::registerEvent(UIntN policyIndex, PolicyEvent::Type policyEvent)
{
    if ((m_registeredEvents.test(policyEvent) == false) &&
        (PolicyEvent::RequiresEsifEventRegistration(policyEvent)))
    {
        // Let ESIF know since the first policy is registering
        FrameworkEvent::Type frameworkEvent = PolicyEvent::ToFrameworkEvent(policyEvent);
        m_dptfManager->getEsifServices()->registerEvent(frameworkEvent);
    }

    m_registeredEvents.set(policyEvent);
    getPolicyPtr(policyIndex)->registerEvent(policyEvent);
}

void PolicyManager::unregisterEvent(UIntN policyIndex, PolicyEvent::Type policyEvent)
{
    if (isAnyPolicyRegisteredForEvent(policyEvent) == true)
    {
        getPolicyPtr(policyIndex)->unregisterEvent(policyEvent);
        m_registeredEvents.set(policyEvent, isAnyPolicyRegisteredForEvent(policyEvent));

        if ((m_registeredEvents.test(policyEvent) == false) &&
            (PolicyEvent::RequiresEsifEventRegistration(policyEvent)))
        {
            FrameworkEvent::Type frameworkEvent = PolicyEvent::ToFrameworkEvent(policyEvent);
            m_dptfManager->getEsifServices()->unregisterEvent(frameworkEvent);
        }
    }
}

std::shared_ptr<XmlNode> PolicyManager::getStatusAsXml(void)
{
    auto status = XmlNode::createWrapperElement("policy_manager");
    status->addChild(getEventsInXml());

    for (auto policy = m_policies.begin(); policy != m_policies.end(); ++policy)
    {
        try
        {
            if (policy->second != nullptr)
            {
                std::string name = policy->second->getName();
                auto policyStatus = XmlNode::createWrapperElement("policy_status");
                auto policyName = XmlNode::createDataElement("policy_name", name);
                policyStatus->addChild(policyName);
                policyStatus->addChild(getEventsXmlForPolicy(policy->first));
                status->addChild(policyStatus);
            }
        }
        catch (...)
        {
            // Policy not available, do not add.
        }
    }
    return status;
}

Bool PolicyManager::isAnyPolicyRegisteredForEvent(PolicyEvent::Type policyEvent)
{
    Bool policyRegistered = false;

    for (auto policy = m_policies.begin(); policy != m_policies.end(); ++policy)
    {
        if ((policy->second != nullptr) && (policy->second->isEventRegistered(policyEvent) == true))
        {
            policyRegistered = true;
            break;
        }
    }

    return policyRegistered;
}

UIntN PolicyManager::getPolicyCount(void)
{
    UIntN policyCount = 0;

    for (auto policy = m_policies.begin(); policy != m_policies.end(); ++policy)
    {
        if (policy->second != nullptr)
        {
            policyCount++;
        }
    }

    return policyCount;
}

std::shared_ptr<XmlNode> PolicyManager::getEventsXmlForPolicy(UIntN policyIndex)
{
    auto status = XmlNode::createWrapperElement("event_values");
    auto eventCount = PolicyEvent::Max;
    auto policy = getPolicyPtr(policyIndex);
    for (auto eventIndex = 1; eventIndex < eventCount; eventIndex++)   // Skip the "Invalid" event
    {
        auto event = XmlNode::createWrapperElement("event");
        auto eventName = XmlNode::createDataElement("event_name", PolicyEvent::toString((PolicyEvent::Type)eventIndex));
        event->addChild(eventName);
        auto eventStatus = XmlNode::createDataElement("event_status",
            friendlyValue(policy->isEventRegistered((PolicyEvent::Type)eventIndex)));
        event->addChild(eventStatus);
        status->addChild(event);
    }
    return status;
}

std::shared_ptr<XmlNode> PolicyManager::getEventsInXml()
{
    auto status = XmlNode::createWrapperElement("events");
    auto eventCount = PolicyEvent::Max;
    for (auto eventIndex = 1; eventIndex < eventCount; eventIndex++)   // Skip the "Invalid" event
    {
        auto event = XmlNode::createWrapperElement("event");
        auto eventName = XmlNode::createDataElement("event_name", PolicyEvent::toString((PolicyEvent::Type)eventIndex));
        event->addChild(eventName);
        status->addChild(event);
    }
    return status;
}
