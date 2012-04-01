/*
 *  ReactOS Task Manager
 *
 *  perfdata.c
 *
 *  Copyright (C) 1999 - 2001  Brian Palmer  <brianp@reactos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>
#include <winnt.h>

#include "taskmgr.h"
#include "perfdata.h"

static PROCNTQSI                       NtQuerySystemInformation = NULL;
static PROCGGR                         pGetGuiResources = NULL;
static PROCGPIC                        pGetProcessIoCounters = NULL;
static CRITICAL_SECTION                PerfDataCriticalSection;
static PPERFDATA                       pPerfDataOld = NULL;    /* Older perf data (saved to establish delta values) */
static PPERFDATA                       pPerfData = NULL;    /* Most recent copy of perf data */
static ULONG                           ProcessCountOld = 0;
static ULONG                           ProcessCount = 0;
static double                          dbIdleTime;
static double                          dbKernelTime;
static double                          dbSystemTime;
static LARGE_INTEGER                   liOldIdleTime = {{0,0}};
static double                          OldKernelTime = 0;
static LARGE_INTEGER                   liOldSystemTime = {{0,0}};
static SYSTEM_PERFORMANCE_INFORMATION  SystemPerfInfo;
static SYSTEM_BASIC_INFORMATION        SystemBasicInfo;
static SYSTEM_CACHE_INFORMATION        SystemCacheInfo;
static SYSTEM_HANDLE_INFORMATION       SystemHandleInfo;
static PSYSTEM_PROCESSORTIME_INFO      SystemProcessorTimeInfo = NULL;

BOOL PerfDataInitialize(void)
{
    LONG    status;
    static const WCHAR wszNtdll[] = {'n','t','d','l','l','.','d','l','l',0};
    static const WCHAR wszUser32[] = {'u','s','e','r','3','2','.','d','l','l',0};
    static const WCHAR wszKernel32[] = {'k','e','r','n','e','l','3','2','.','d','l','l',0};

    NtQuerySystemInformation = (PROCNTQSI)GetProcAddress(GetModuleHandleW(wszNtdll), "NtQuerySystemInformation");
    pGetGuiResources = (PROCGGR)GetProcAddress(GetModuleHandleW(wszUser32), "GetGuiResources");
    pGetProcessIoCounters = (PROCGPIC)GetProcAddress(GetModuleHandleW(wszKernel32), "GetProcessIoCounters");
    
    InitializeCriticalSection(&PerfDataCriticalSection);
    
    if (!NtQuerySystemInformation)
        return FALSE;
    
    /*
     * Get number of processors in the system
     */
    status = NtQuerySystemInformation(SystemBasicInformation, &SystemBasicInfo, sizeof(SystemBasicInfo), NULL);
    if (status != NO_ERROR)
        return FALSE;
    
    return TRUE;
}

void PerfDataUninitialize(void)
{
    NtQuerySystemInformation = NULL;

    DeleteCriticalSection(&PerfDataCriticalSection);
}

void PerfDataRefresh(void)
{
    ULONG                            ulSize;
    LONG                            status;
    LPBYTE                            pBuffer;
    ULONG                            BufferSize;
    PSYSTEM_PROCESS_INFORMATION        pSPI;
    PPERFDATA                        pPDOld;
    ULONG                            Idx, Idx2;
    HANDLE                            hProcess;
    HANDLE                            hProcessToken;
    WCHAR                            wszTemp[MAX_PATH];
    DWORD                            dwSize;
    SYSTEM_PERFORMANCE_INFORMATION    SysPerfInfo;
    SYSTEM_TIME_INFORMATION            SysTimeInfo;
    SYSTEM_CACHE_INFORMATION        SysCacheInfo;
    LPBYTE                            SysHandleInfoData;
    PSYSTEM_PROCESSORTIME_INFO        SysProcessorTimeInfo;
    double                            CurrentKernelTime;


    if (!NtQuerySystemInformation)
        return;

    /* Get new system time */
    status = NtQuerySystemInformation(SystemTimeInformation, &SysTimeInfo, sizeof(SysTimeInfo), 0);
    if (status != NO_ERROR)
        return;

    /* Get new CPU's idle time */
    status = NtQuerySystemInformation(SystemPerformanceInformation, &SysPerfInfo, sizeof(SysPerfInfo), NULL);
    if (status != NO_ERROR)
        return;

    /* Get system cache information */
    status = NtQuerySystemInformation(SystemCacheInformation, &SysCacheInfo, sizeof(SysCacheInfo), NULL);
    if (status != NO_ERROR)
        return;

    /* Get processor time information */
    SysProcessorTimeInfo = HeapAlloc(GetProcessHeap(), 0,
                                sizeof(SYSTEM_PROCESSORTIME_INFO) * SystemBasicInfo.bKeNumberProcessors);
    status = NtQuerySystemInformation(SystemProcessorTimeInformation, SysProcessorTimeInfo, sizeof(SYSTEM_PROCESSORTIME_INFO) * SystemBasicInfo.bKeNumberProcessors, &ulSize);
    if (status != NO_ERROR) {
        HeapFree(GetProcessHeap(), 0, SysProcessorTimeInfo);
        return;
    }

    /* Get handle information
     * We don't know how much data there is so just keep
     * increasing the buffer size until the call succeeds
     */
    BufferSize = 0;
    do
    {
        BufferSize += 0x10000;
        SysHandleInfoData = HeapAlloc(GetProcessHeap(), 0, BufferSize);

        status = NtQuerySystemInformation(SystemHandleInformation, SysHandleInfoData, BufferSize, &ulSize);

        if (status == 0xC0000004 /*STATUS_INFO_LENGTH_MISMATCH*/) {
            HeapFree(GetProcessHeap(), 0, SysHandleInfoData);
        }

    } while (status == 0xC0000004 /*STATUS_INFO_LENGTH_MISMATCH*/);

    /* Get process information
     * We don't know how much data there is so just keep
     * increasing the buffer size until the call succeeds
     */
    BufferSize = 0;
    do
    {
        BufferSize += 0x10000;
        pBuffer = HeapAlloc(GetProcessHeap(), 0, BufferSize);

        status = NtQuerySystemInformation(SystemProcessInformation, pBuffer, BufferSize, &ulSize);

        if (status == 0xC0000004 /*STATUS_INFO_LENGTH_MISMATCH*/) {
            HeapFree(GetProcessHeap(), 0, pBuffer);
        }

    } while (status == 0xC0000004 /*STATUS_INFO_LENGTH_MISMATCH*/);

    EnterCriticalSection(&PerfDataCriticalSection);

    /*
     * Save system performance info
     */
    memcpy(&SystemPerfInfo, &SysPerfInfo, sizeof(SYSTEM_PERFORMANCE_INFORMATION));

    /*
     * Save system cache info
     */
    memcpy(&SystemCacheInfo, &SysCacheInfo, sizeof(SYSTEM_CACHE_INFORMATION));
    
    /*
     * Save system processor time info
     */
    HeapFree(GetProcessHeap(), 0, SystemProcessorTimeInfo);
    SystemProcessorTimeInfo = SysProcessorTimeInfo;
    
    /*
     * Save system handle info
     */
    memcpy(&SystemHandleInfo, SysHandleInfoData, sizeof(SYSTEM_HANDLE_INFORMATION));
    HeapFree(GetProcessHeap(), 0, SysHandleInfoData);
    
    for (CurrentKernelTime=0, Idx=0; Idx<SystemBasicInfo.bKeNumberProcessors; Idx++) {
        CurrentKernelTime += Li2Double(SystemProcessorTimeInfo[Idx].KernelTime);
        CurrentKernelTime += Li2Double(SystemProcessorTimeInfo[Idx].DpcTime);
        CurrentKernelTime += Li2Double(SystemProcessorTimeInfo[Idx].InterruptTime);
    }

    /* If it's a first call - skip idle time calcs */
    if (liOldIdleTime.QuadPart != 0) {
        /*  CurrentValue = NewValue - OldValue */
        dbIdleTime = Li2Double(SysPerfInfo.liIdleTime) - Li2Double(liOldIdleTime);
        dbKernelTime = CurrentKernelTime - OldKernelTime;
        dbSystemTime = Li2Double(SysTimeInfo.liKeSystemTime) - Li2Double(liOldSystemTime);

        /*  CurrentCpuIdle = IdleTime / SystemTime */
        dbIdleTime = dbIdleTime / dbSystemTime;
        dbKernelTime = dbKernelTime / dbSystemTime;
        
        /*  CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors */
        dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)SystemBasicInfo.bKeNumberProcessors; /* + 0.5; */
        dbKernelTime = 100.0 - dbKernelTime * 100.0 / (double)SystemBasicInfo.bKeNumberProcessors; /* + 0.5; */
    }

    /* Store new CPU's idle and system time */
    liOldIdleTime = SysPerfInfo.liIdleTime;
    liOldSystemTime = SysTimeInfo.liKeSystemTime;
    OldKernelTime = CurrentKernelTime;

    /* Determine the process count
     * We loop through the data we got from NtQuerySystemInformation
     * and count how many structures there are (until RelativeOffset is 0)
     */
    ProcessCountOld = ProcessCount;
    ProcessCount = 0;
    pSPI = (PSYSTEM_PROCESS_INFORMATION)pBuffer;
    while (pSPI) {
        ProcessCount++;
        if (pSPI->RelativeOffset == 0)
            break;
        pSPI = (PSYSTEM_PROCESS_INFORMATION)((LPBYTE)pSPI + pSPI->RelativeOffset);
    }

    /* Now alloc a new PERFDATA array and fill in the data */
    HeapFree(GetProcessHeap(), 0, pPerfDataOld);
    pPerfDataOld = pPerfData;
    pPerfData = HeapAlloc(GetProcessHeap(), 0, sizeof(PERFDATA) * ProcessCount);
    pSPI = (PSYSTEM_PROCESS_INFORMATION)pBuffer;
    for (Idx=0; Idx<ProcessCount; Idx++) {
        /* Get the old perf data for this process (if any) */
        /* so that we can establish delta values */
        pPDOld = NULL;
        for (Idx2=0; Idx2<ProcessCountOld; Idx2++) {
            if (pPerfDataOld[Idx2].ProcessId == pSPI->ProcessId) {
                pPDOld = &pPerfDataOld[Idx2];
                break;
            }
        }

        /* Clear out process perf data structure */
        memset(&pPerfData[Idx], 0, sizeof(PERFDATA));

        if (pSPI->Name.Buffer)
            lstrcpyW(pPerfData[Idx].ImageName, pSPI->Name.Buffer);
        else
        {
            WCHAR idleW[255];
            LoadStringW(hInst, IDS_SYSTEM_IDLE_PROCESS, idleW, sizeof(idleW)/sizeof(WCHAR));
            lstrcpyW(pPerfData[Idx].ImageName, idleW );
        }

        pPerfData[Idx].ProcessId = pSPI->ProcessId;

        if (pPDOld)    {
            double    CurTime = Li2Double(pSPI->KernelTime) + Li2Double(pSPI->UserTime);
            double    OldTime = Li2Double(pPDOld->KernelTime) + Li2Double(pPDOld->UserTime);
            double    CpuTime = (CurTime - OldTime) / dbSystemTime;
            CpuTime = CpuTime * 100.0 / (double)SystemBasicInfo.bKeNumberProcessors; /* + 0.5; */
            pPerfData[Idx].CPUUsage = (ULONG)CpuTime;
        }
        pPerfData[Idx].CPUTime.QuadPart = pSPI->UserTime.QuadPart + pSPI->KernelTime.QuadPart;
        pPerfData[Idx].WorkingSetSizeBytes = pSPI->TotalWorkingSetSizeBytes;
        pPerfData[Idx].PeakWorkingSetSizeBytes = pSPI->PeakWorkingSetSizeBytes;
        if (pPDOld)
            pPerfData[Idx].WorkingSetSizeDelta = labs((LONG)pSPI->TotalWorkingSetSizeBytes - (LONG)pPDOld->WorkingSetSizeBytes);
        else
            pPerfData[Idx].WorkingSetSizeDelta = 0;
        pPerfData[Idx].PageFaultCount = pSPI->PageFaultCount;
        if (pPDOld)
            pPerfData[Idx].PageFaultCountDelta = labs((LONG)pSPI->PageFaultCount - (LONG)pPDOld->PageFaultCount);
        else
            pPerfData[Idx].PageFaultCountDelta = 0;
        pPerfData[Idx].VirtualMemorySizeBytes = pSPI->TotalVirtualSizeBytes;
        pPerfData[Idx].PagedPoolUsagePages = pSPI->TotalPagedPoolUsagePages;
        pPerfData[Idx].NonPagedPoolUsagePages = pSPI->TotalNonPagedPoolUsagePages;
        pPerfData[Idx].BasePriority = pSPI->BasePriority;
        pPerfData[Idx].HandleCount = pSPI->HandleCount;
        pPerfData[Idx].ThreadCount = pSPI->ThreadCount;
        pPerfData[Idx].SessionId = pSPI->SessionId;
        
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pSPI->ProcessId);
        if (hProcess) {
            if (OpenProcessToken(hProcess, TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_IMPERSONATE, &hProcessToken)) {
                ImpersonateLoggedOnUser(hProcessToken);
                memset(wszTemp, 0, sizeof(wszTemp));
                dwSize = MAX_PATH;
                GetUserNameW(wszTemp, &dwSize);
                RevertToSelf();
                CloseHandle(hProcessToken);
            }
            if (pGetGuiResources) {
                pPerfData[Idx].USERObjectCount = pGetGuiResources(hProcess, GR_USEROBJECTS);
                pPerfData[Idx].GDIObjectCount = pGetGuiResources(hProcess, GR_GDIOBJECTS);
            }
            if (pGetProcessIoCounters)
                pGetProcessIoCounters(hProcess, &pPerfData[Idx].IOCounters);
            CloseHandle(hProcess);
        }
        pPerfData[Idx].UserTime.QuadPart = pSPI->UserTime.QuadPart;
        pPerfData[Idx].KernelTime.QuadPart = pSPI->KernelTime.QuadPart;
        pSPI = (PSYSTEM_PROCESS_INFORMATION)((LPBYTE)pSPI + pSPI->RelativeOffset);
    }
    HeapFree(GetProcessHeap(), 0, pBuffer);
    LeaveCriticalSection(&PerfDataCriticalSection);
}

ULONG PerfDataGetProcessCount(void)
{
    return ProcessCount;
}

ULONG PerfDataGetProcessorUsage(void)
{
    if( dbIdleTime < 0.0 )
        return 0;
    if( dbIdleTime > 100.0 )
        return 100;
    return (ULONG)dbIdleTime;
}

ULONG PerfDataGetProcessorSystemUsage(void)
{
    if( dbKernelTime < 0.0 )
        return 0;
    if( dbKernelTime > 100.0 )
        return 100;
    return (ULONG)dbKernelTime;
}

BOOL PerfDataGetImageName(ULONG Index, LPWSTR lpImageName, int nMaxCount)
{
    BOOL    bSuccessful;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount) {
        wcsncpy(lpImageName, pPerfData[Index].ImageName, nMaxCount);
        bSuccessful = TRUE;
    } else {
        bSuccessful = FALSE;
    }
    LeaveCriticalSection(&PerfDataCriticalSection);
    return bSuccessful;
}

ULONG PerfDataGetProcessId(ULONG Index)
{
    ULONG    ProcessId;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        ProcessId = pPerfData[Index].ProcessId;
    else
        ProcessId = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return ProcessId;
}

BOOL PerfDataGetUserName(ULONG Index, LPWSTR lpUserName, int nMaxCount)
{
    BOOL    bSuccessful;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount) {
        wcsncpy(lpUserName, pPerfData[Index].UserName, nMaxCount);
        bSuccessful = TRUE;
    } else {
        bSuccessful = FALSE;
    }

    LeaveCriticalSection(&PerfDataCriticalSection);

    return bSuccessful;
}

ULONG PerfDataGetSessionId(ULONG Index)
{
    ULONG    SessionId;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        SessionId = pPerfData[Index].SessionId;
    else
        SessionId = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return SessionId;
}

ULONG PerfDataGetCPUUsage(ULONG Index)
{
    ULONG    CpuUsage;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        CpuUsage = pPerfData[Index].CPUUsage;
    else
        CpuUsage = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return CpuUsage;
}

TIME PerfDataGetCPUTime(ULONG Index)
{
    TIME    CpuTime = {{0,0}};

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        CpuTime = pPerfData[Index].CPUTime;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return CpuTime;
}

ULONG PerfDataGetWorkingSetSizeBytes(ULONG Index)
{
    ULONG    WorkingSetSizeBytes;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        WorkingSetSizeBytes = pPerfData[Index].WorkingSetSizeBytes;
    else
        WorkingSetSizeBytes = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return WorkingSetSizeBytes;
}

ULONG PerfDataGetPeakWorkingSetSizeBytes(ULONG Index)
{
    ULONG    PeakWorkingSetSizeBytes;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        PeakWorkingSetSizeBytes = pPerfData[Index].PeakWorkingSetSizeBytes;
    else
        PeakWorkingSetSizeBytes = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return PeakWorkingSetSizeBytes;
}

ULONG PerfDataGetWorkingSetSizeDelta(ULONG Index)
{
    ULONG    WorkingSetSizeDelta;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        WorkingSetSizeDelta = pPerfData[Index].WorkingSetSizeDelta;
    else
        WorkingSetSizeDelta = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return WorkingSetSizeDelta;
}

ULONG PerfDataGetPageFaultCount(ULONG Index)
{
    ULONG    PageFaultCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        PageFaultCount = pPerfData[Index].PageFaultCount;
    else
        PageFaultCount = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return PageFaultCount;
}

ULONG PerfDataGetPageFaultCountDelta(ULONG Index)
{
    ULONG    PageFaultCountDelta;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        PageFaultCountDelta = pPerfData[Index].PageFaultCountDelta;
    else
        PageFaultCountDelta = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return PageFaultCountDelta;
}

ULONG PerfDataGetVirtualMemorySizeBytes(ULONG Index)
{
    ULONG    VirtualMemorySizeBytes;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        VirtualMemorySizeBytes = pPerfData[Index].VirtualMemorySizeBytes;
    else
        VirtualMemorySizeBytes = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return VirtualMemorySizeBytes;
}

ULONG PerfDataGetPagedPoolUsagePages(ULONG Index)
{
    ULONG    PagedPoolUsagePages;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        PagedPoolUsagePages = pPerfData[Index].PagedPoolUsagePages;
    else
        PagedPoolUsagePages = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return PagedPoolUsagePages;
}

ULONG PerfDataGetNonPagedPoolUsagePages(ULONG Index)
{
    ULONG    NonPagedPoolUsagePages;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        NonPagedPoolUsagePages = pPerfData[Index].NonPagedPoolUsagePages;
    else
        NonPagedPoolUsagePages = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return NonPagedPoolUsagePages;
}

ULONG PerfDataGetBasePriority(ULONG Index)
{
    ULONG    BasePriority;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        BasePriority = pPerfData[Index].BasePriority;
    else
        BasePriority = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return BasePriority;
}

ULONG PerfDataGetHandleCount(ULONG Index)
{
    ULONG    HandleCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        HandleCount = pPerfData[Index].HandleCount;
    else
        HandleCount = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return HandleCount;
}

ULONG PerfDataGetThreadCount(ULONG Index)
{
    ULONG    ThreadCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        ThreadCount = pPerfData[Index].ThreadCount;
    else
        ThreadCount = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return ThreadCount;
}

ULONG PerfDataGetUSERObjectCount(ULONG Index)
{
    ULONG    USERObjectCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        USERObjectCount = pPerfData[Index].USERObjectCount;
    else
        USERObjectCount = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return USERObjectCount;
}

ULONG PerfDataGetGDIObjectCount(ULONG Index)
{
    ULONG    GDIObjectCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
        GDIObjectCount = pPerfData[Index].GDIObjectCount;
    else
        GDIObjectCount = 0;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return GDIObjectCount;
}

BOOL PerfDataGetIOCounters(ULONG Index, PIO_COUNTERS pIoCounters)
{
    BOOL    bSuccessful;

    EnterCriticalSection(&PerfDataCriticalSection);

    if (Index < ProcessCount)
    {
        memcpy(pIoCounters, &pPerfData[Index].IOCounters, sizeof(IO_COUNTERS));
        bSuccessful = TRUE;
    }
    else
        bSuccessful = FALSE;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return bSuccessful;
}

ULONG PerfDataGetCommitChargeTotalK(void)
{
    ULONG    Total;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Total = SystemPerfInfo.MmTotalCommittedPages;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Total = Total * (PageSize / 1024);

    return Total;
}

ULONG PerfDataGetCommitChargeLimitK(void)
{
    ULONG    Limit;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Limit = SystemPerfInfo.MmTotalCommitLimit;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Limit = Limit * (PageSize / 1024);

    return Limit;
}

ULONG PerfDataGetCommitChargePeakK(void)
{
    ULONG    Peak;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Peak = SystemPerfInfo.MmPeakLimit;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Peak = Peak * (PageSize / 1024);

    return Peak;
}

ULONG PerfDataGetKernelMemoryTotalK(void)
{
    ULONG    Total;
    ULONG    Paged;
    ULONG    NonPaged;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Paged = SystemPerfInfo.PoolPagedBytes;
    NonPaged = SystemPerfInfo.PoolNonPagedBytes;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Paged = Paged * (PageSize / 1024);
    NonPaged = NonPaged * (PageSize / 1024);

    Total = Paged + NonPaged;

    return Total;
}

ULONG PerfDataGetKernelMemoryPagedK(void)
{
    ULONG    Paged;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Paged = SystemPerfInfo.PoolPagedBytes;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Paged = Paged * (PageSize / 1024);

    return Paged;
}

ULONG PerfDataGetKernelMemoryNonPagedK(void)
{
    ULONG    NonPaged;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    NonPaged = SystemPerfInfo.PoolNonPagedBytes;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    NonPaged = NonPaged * (PageSize / 1024);

    return NonPaged;
}

ULONG PerfDataGetPhysicalMemoryTotalK(void)
{
    ULONG    Total;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Total = SystemBasicInfo.uMmNumberOfPhysicalPages;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Total = Total * (PageSize / 1024);

    return Total;
}

ULONG PerfDataGetPhysicalMemoryAvailableK(void)
{
    ULONG    Available;
    ULONG    PageSize;

    EnterCriticalSection(&PerfDataCriticalSection);

    Available = SystemPerfInfo.MmAvailablePages;
    PageSize = SystemBasicInfo.uPageSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    Available = Available * (PageSize / 1024);

    return Available;
}

ULONG PerfDataGetPhysicalMemorySystemCacheK(void)
{
    ULONG    SystemCache;

    EnterCriticalSection(&PerfDataCriticalSection);

    SystemCache = SystemCacheInfo.CurrentSize;

    LeaveCriticalSection(&PerfDataCriticalSection);

    SystemCache = SystemCache / 1024;

    return SystemCache;
}

ULONG PerfDataGetSystemHandleCount(void)
{
    ULONG    HandleCount;

    EnterCriticalSection(&PerfDataCriticalSection);

    HandleCount = SystemHandleInfo.Count;

    LeaveCriticalSection(&PerfDataCriticalSection);

    return HandleCount;
}

ULONG PerfDataGetTotalThreadCount(void)
{
    ULONG    ThreadCount = 0;
    ULONG    i;

    EnterCriticalSection(&PerfDataCriticalSection);

    for (i=0; i<ProcessCount; i++)
    {
        ThreadCount += pPerfData[i].ThreadCount;
    }

    LeaveCriticalSection(&PerfDataCriticalSection);

    return ThreadCount;
}
