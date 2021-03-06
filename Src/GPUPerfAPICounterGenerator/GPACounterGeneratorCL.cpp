//==============================================================================
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Class for CL counter generation
//==============================================================================

#include "GPACounterGeneratorCL.h"
#include "Logging.h"

#ifdef EXTRA_COUNTER_INFO
    #include <sstream>
#endif

#include "PublicCounterDefsCLGfx6.h"
#include "PublicCounterDefsCLGfx7.h"
#include "PublicCounterDefsCLGfx8.h"

#include "InternalCountersCLGfx6.h"
#include "InternalCountersCLGfx7.h"
#include "InternalCountersCLGfx8.h"

#include "GPACounterGeneratorSchedulerManager.h"

GPA_CounterGeneratorCL::GPA_CounterGeneratorCL()
{
    SetAllowedCounters(true, true, false);

    for (int gen = GDT_HW_GENERATION_FIRST_AMD; gen < GDT_HW_GENERATION_LAST; gen++)
    {
        CounterGeneratorSchedulerManager::Instance()->RegisterCounterGenerator(GPA_API_OPENCL, static_cast<GDT_HW_GENERATION>(gen), this);
    }
}

GPA_Status GPA_CounterGeneratorCL::GeneratePublicCounters(GDT_HW_GENERATION desiredGeneration, GPA_PublicCounters* pPublicCounters)
{
    if (desiredGeneration == GDT_HW_GENERATION_SOUTHERNISLAND)
    {
        AutoDefinePublicCountersCLGfx6(*pPublicCounters);
    }
    else if (desiredGeneration == GDT_HW_GENERATION_SEAISLAND)
    {
        AutoDefinePublicCountersCLGfx7(*pPublicCounters);
    }
    else if (desiredGeneration == GDT_HW_GENERATION_VOLCANICISLAND)
    {
        AutoDefinePublicCountersCLGfx8(*pPublicCounters);
    }
    else
    {
        GPA_LogError("Unrecognized or unhandled hardware generation.");
        return GPA_STATUS_ERROR_HARDWARE_NOT_SUPPORTED;
    }

    return GPA_STATUS_OK;
}

GPA_Status GPA_CounterGeneratorCL::GenerateHardwareCounters(GDT_HW_GENERATION desiredGeneration, GPA_HardwareCounters* pHardwareCounters)
{
    if (desiredGeneration == GDT_HW_GENERATION_SOUTHERNISLAND)
    {
        pHardwareCounters->m_ppCounterGroupArray = CLCounterGroupArrayGfx6;
        pHardwareCounters->m_pGroups             = HWCLGroupsGfx6;
        pHardwareCounters->m_groupCount          = HWCLGroupCountGfx6;
        pHardwareCounters->m_pSQCounterGroups    = HWCLSQGroupsGfx6;
        pHardwareCounters->m_sqGroupCount        = HWCLSQGroupCountGfx6;
    }
    else if (desiredGeneration == GDT_HW_GENERATION_SEAISLAND)
    {
        pHardwareCounters->m_ppCounterGroupArray = CLCounterGroupArrayGfx7;
        pHardwareCounters->m_pGroups             = HWCLGroupsGfx7;
        pHardwareCounters->m_groupCount          = HWCLGroupCountGfx7;
        pHardwareCounters->m_pSQCounterGroups    = HWCLSQGroupsGfx7;
        pHardwareCounters->m_sqGroupCount        = HWCLSQGroupCountGfx7;
    }
    else if (desiredGeneration == GDT_HW_GENERATION_VOLCANICISLAND)
    {
        pHardwareCounters->m_ppCounterGroupArray = CLCounterGroupArrayGfx8;
        pHardwareCounters->m_pGroups            = HWCLGroupsGfx8;
        pHardwareCounters->m_groupCount        = HWCLGroupCountGfx8;
        pHardwareCounters->m_pSQCounterGroups   = HWCLSQGroupsGfx8;
        pHardwareCounters->m_sqGroupCount      = HWCLSQGroupCountGfx8;
    }
    else
    {
        GPA_LogError("Unrecognized or unhandled hardware generation.");
        return GPA_STATUS_ERROR_HARDWARE_NOT_SUPPORTED;
    }

    // need to count total number of internal counters, since split into groups
    if (!pHardwareCounters->m_countersGenerated)
    {
        pHardwareCounters->m_counters.clear();

        GPA_HardwareCounterDescExt counter;
#if defined(_DEBUG) && defined(_WIN32) && defined(AMDT_INTERNAL)
        // Debug builds will generate a file that lists the counter names in a format that can be
        // easily copy/pasted into the GPUPerfAPIUnitTests project
        FILE* pFile = nullptr;
        fopen_s(&pFile, "HardwareCounterNamesCL.txt", "w");
#endif

        for (gpa_uint32 i = 0; i < pHardwareCounters->m_groupCount; i++)
        {
            GPA_HardwareCounterDesc* pClGroup = (*(pHardwareCounters->m_ppCounterGroupArray + i));
            const int numGroupCounters = (int)pHardwareCounters->m_pGroups[i].m_numCounters;

            for (int j = 0; j < numGroupCounters; j++)
            {
                counter.m_pHardwareCounter = &(pClGroup[j]);
                counter.m_groupIndex      = i;
                counter.m_groupIdDriver   = i;
                counter.m_counterIdDriver = 0;

#if defined(_DEBUG) && defined(_WIN32) && defined(AMDT_INTERNAL)

                if (nullptr != pFile)
                {
                    fwrite("    \"", 1, 5, pFile);
                    std::string tmpName(counter.m_pHardwareCounter->m_pName);
                    size_t size = tmpName.size();
                    fwrite(counter.m_pHardwareCounter->m_pName, 1, size, pFile);
                    fwrite("\",", 1, 2, pFile);
#ifdef EXTRA_COUNTER_INFO
                    // this can be useful for debugging counter definitions
                    std::stringstream ss;
                    ss << " " << counter.m_groupIndex << ", " << counter.m_groupIdDriver << ", " << counter.m_pHardwareCounter->m_counterIndexInGroup << ", " << counter.m_counterIdDriver;
                    std::string tmpCounterInfo(ss.str());
                    size = tmpCounterInfo.size();
                    fwrite(tmpCounterInfo.c_str(), 1, size, pFile);
#endif
                    fwrite("\n", 1, 1, pFile);
                }

#endif
                pHardwareCounters->m_counters.push_back(counter);
            }
        }

#if defined(_DEBUG) && defined(_WIN32) && defined(AMDT_INTERNAL)

        if (nullptr != pFile)
        {
            fclose(pFile);
        }

#endif

        pHardwareCounters->m_countersGenerated = true;
    }

    // resize the counts that indicate how many counters from each group are being used
    unsigned int uGroupCount = pHardwareCounters->m_groupCount;
    pHardwareCounters->m_currentGroupUsedCounts.resize(uGroupCount);

    return GPA_STATUS_OK;
}

GPA_Status GPA_CounterGeneratorCL::GenerateSoftwareCounters(GDT_HW_GENERATION desiredGeneration, GPA_SoftwareCounters* pSoftwareCounters)
{
    UNREFERENCED_PARAMETER(desiredGeneration);
    UNREFERENCED_PARAMETER(pSoftwareCounters);
    return GPA_STATUS_OK;
}
