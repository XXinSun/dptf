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

#include "DomainPixelClockStatus_001.h"

DomainPixelClockStatus_001::DomainPixelClockStatus_001(UIntN participantIndex, UIntN domainIndex, 
    std::shared_ptr<ParticipantServicesInterface> participantServicesInterface) :
    DomainPixelClockStatusBase(participantIndex, domainIndex, participantServicesInterface)
{
    // TODO: capture if needed
}

DomainPixelClockStatus_001::~DomainPixelClockStatus_001(void)
{
}

PixelClockCapabilities DomainPixelClockStatus_001::getPixelClockCapabilities(UIntN participantIndex, UIntN domainIndex)
{
    UInt32 uint32Value = 0;
    UInt8 uint8Value = 0;

    // PixelClockPanelType
    uint32Value = getParticipantServices()->primitiveExecuteGetAsUInt32(
        esif_primitive_type::GET_DISPLAY_PANEL_TYPE, domainIndex);
    PixelClockPanelType::Type pixelClockPanelType = static_cast<PixelClockPanelType::Type>(uint32Value);

    // Clock deviation
    UInt32 clockDeviation = getParticipantServices()->primitiveExecuteGetAsUInt32(
        esif_primitive_type::GET_DISPLAY_CLOCK_DEVIATION, domainIndex);

    // Upward deviation
    uint8Value = getParticipantServices()->primitiveExecuteGetAsUInt8(
        esif_primitive_type::GET_DISPLAY_CLOCK_UPWARD_DEVIATION, domainIndex);
    Bool upwardDeviation = (uint8Value != 0);

    // Bool Downward deviation
    uint8Value = getParticipantServices()->primitiveExecuteGetAsUInt8(
        esif_primitive_type::GET_DISPLAY_CLOCK_DOWNWARD_DEVIATION, domainIndex);
    Bool downwardDeviation = (uint8Value != 0);

    // PixelClockChannelType
    uint32Value = getParticipantServices()->primitiveExecuteGetAsUInt32(
        esif_primitive_type::GET_GRAPHICS_CHIPSET_CHANNEL_TYPE, domainIndex);
    PixelClockChannelType::Type pixelClockChannelType = static_cast<PixelClockChannelType::Type>(uint32Value);

    // SSC enabled
    uint8Value = getParticipantServices()->primitiveExecuteGetAsUInt8(
        esif_primitive_type::GET_GRAPHICS_CHIPSET_SSC_ENABLED, domainIndex);
    Bool sscEnabled = (uint8Value != 0);

    //PixelClockSpreadType::Type m_sscSpreadDirection;
    uint32Value = getParticipantServices()->primitiveExecuteGetAsUInt32(
        esif_primitive_type::GET_DISPLAY_CLOCK_SPREAD_DIRECTION, domainIndex);
    PixelClockSpreadType::Type sscSpreadDirection = static_cast<PixelClockSpreadType::Type>(uint32Value);

    // Spread Percentage
    Percentage spreadPercentage = getParticipantServices()->primitiveExecuteGetAsPercentage(
        esif_primitive_type::GET_DISPLAY_CLOCK_SPREAD_PERCENTAGE, domainIndex);

    // Check for invalid values and substitute defaults
    // TODO: should this be in ESIF instead?
    if (clockDeviation == 0)
    {
        clockDeviation = 2; // TODO: change to 20 as 10th percent.
    }
    if ((upwardDeviation == 0) && (downwardDeviation == 0))
    {
        upwardDeviation = 1;
        downwardDeviation = 1;
    }

    PixelClockCapabilities pixelClockCapabilities(pixelClockPanelType, clockDeviation, upwardDeviation,
        downwardDeviation, pixelClockChannelType, sscEnabled, sscSpreadDirection, spreadPercentage);

    return pixelClockCapabilities;
}

PixelClockDataSet DomainPixelClockStatus_001::getPixelClockDataSet(UIntN participantIndex, UIntN domainIndex)
{
    std::vector<PixelClockData> pixelClockDataVector;

    UInt32 clockCount = getParticipantServices()->primitiveExecuteGetAsUInt32(
        esif_primitive_type::GET_CLOCK_COUNT, domainIndex);

    for (UIntN i = 0; i < clockCount; i++)
    {
        Frequency panelInputFrequencySpecification = getParticipantServices()->primitiveExecuteGetAsFrequency(
            esif_primitive_type::GET_CLOCK_ORIGINAL_FREQUENCY, domainIndex, static_cast<UInt8>(i));
        if (panelInputFrequencySpecification == Frequency(0))
        {
            throw dptf_exception("Original Clock frequency has an invalid value of 0.");
        }
        Frequency sscEnabledNudgeFrequency = getParticipantServices()->primitiveExecuteGetAsFrequency(
            esif_primitive_type::GET_DISPLAY_CLOCK_SSC_ENABLED_FREQUENCY, domainIndex, static_cast<UInt8>(i));
        Frequency sscDisabledNudgeFrequency = getParticipantServices()->primitiveExecuteGetAsFrequency(
            esif_primitive_type::GET_DISPLAY_CLOCK_SSC_DISABLED_FREQUENCY, domainIndex, static_cast<UInt8>(i));

        pixelClockDataVector.push_back(
            PixelClockData(panelInputFrequencySpecification, sscEnabledNudgeFrequency, sscDisabledNudgeFrequency));
    }

    PixelClockDataSet pixelClockDataSet(pixelClockDataVector);

    return pixelClockDataSet;
}

void DomainPixelClockStatus_001::sendActivityLoggingDataIfEnabled(UIntN participantIndex, UIntN domainIndex)
{
    try
    {
        if (isActivityLoggingEnabled() == true)
        {
            EsifCapabilityData capability;
            capability.type = ESIF_CAPABILITY_TYPE_PIXELCLOCK_STATUS;
            capability.size = sizeof(capability);

            getParticipantServices()->sendDptfEvent(ParticipantEvent::DptfParticipantControlAction,
                domainIndex, Capability::getEsifDataFromCapabilityData(&capability));
        }
    }
    catch (...)
    {
        // skip if there are any issue in sending log data
    }
}

void DomainPixelClockStatus_001::clearCachedData(void)
{
    // FIXME: do we clear the cache for this control?
}

std::shared_ptr<XmlNode> DomainPixelClockStatus_001::getXml(UIntN domainIndex)
{
    // FIXME
    throw implement_me();
}

std::string DomainPixelClockStatus_001::getName(void)
{
    return "Pixel Clock Status (Version 1)";
}
