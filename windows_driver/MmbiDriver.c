/*++

Module Name:

    MmbiDriver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include "MmbiDriver.h"

//
// Global shared context/buffer representation
//
#define RING_BUFFER_SIZE (1024 * 1024) // 1 MB Buffer to absorb bursts

typedef struct _MMBI_RING_BUFFER {
    UCHAR Buffer[RING_BUFFER_SIZE];
    ULONG WriteIdx;
    ULONG ReadIdx;
    ULONG DataAvailable; // Track explicit count for simplicity
} MMBI_RING_BUFFER, *PMMBI_RING_BUFFER;

typedef struct _MMBI_SHARED_CONTEXT {
    WDFSPINLOCK Lock;
    MMBI_RING_BUFFER HostToBmc; // Written by 0, Read by 1
    MMBI_RING_BUFFER BmcToHost; // Written by 1, Read by 0
} MMBI_SHARED_CONTEXT, *PMMBI_SHARED_CONTEXT;

MMBI_SHARED_CONTEXT GlobalContext;
// ... (Driver Entry etc remains same until Device Creation) ...
// (We just update the Read/Write functions)

VOID
MmbiEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
}

// ... CreateMmbiDevice DeviceAdd helper remains same ...
// Skipping to I/O Handlers

VOID
RingBufferWrite(
    PMMBI_RING_BUFFER Ring,
    PVOID Data,
    ULONG Length,
    PULONG BytesWritten
)
{
    ULONG spaceFree = RING_BUFFER_SIZE - Ring->DataAvailable;
    ULONG toWrite = Length;
    
    if (toWrite > spaceFree) {
        // Return 0 or partial? 
        // If 0 space, return 0.
        // Let's implement partial write or fail. 
        // Typical behavior: if not enough space, fail or partial.
        toWrite = spaceFree;
    }

    if (toWrite == 0) {
        *BytesWritten = 0;
        return;
    }

    // Two step write
    ULONG firstChunk = RING_BUFFER_SIZE - Ring->WriteIdx;
    if (toWrite <= firstChunk) {
        RtlCopyMemory(&Ring->Buffer[Ring->WriteIdx], Data, toWrite);
        Ring->WriteIdx = (Ring->WriteIdx + toWrite) % RING_BUFFER_SIZE;
    } else {
        RtlCopyMemory(&Ring->Buffer[Ring->WriteIdx], Data, firstChunk);
        ULONG secondChunk = toWrite - firstChunk;
        RtlCopyMemory(&Ring->Buffer[0], (PUCHAR)Data + firstChunk, secondChunk);
        Ring->WriteIdx = secondChunk;
    }
    
    Ring->DataAvailable += toWrite;
    *BytesWritten = toWrite;
}

VOID
RingBufferRead(
    PMMBI_RING_BUFFER Ring,
    PVOID Buffer,
    ULONG Length,
    PULONG BytesRead
)
{
    ULONG toRead = Length;
    if (toRead > Ring->DataAvailable) {
        toRead = Ring->DataAvailable;
    }

    if (toRead == 0) {
        *BytesRead = 0;
        return;
    }

    ULONG firstChunk = RING_BUFFER_SIZE - Ring->ReadIdx;
    if (toRead <= firstChunk) {
        RtlCopyMemory(Buffer, &Ring->Buffer[Ring->ReadIdx], toRead);
        Ring->ReadIdx = (Ring->ReadIdx + toRead) % RING_BUFFER_SIZE;
    } else {
        RtlCopyMemory(Buffer, &Ring->Buffer[Ring->ReadIdx], firstChunk);
        ULONG secondChunk = toRead - firstChunk;
        RtlCopyMemory((PUCHAR)Buffer + firstChunk, &Ring->Buffer[0], secondChunk);
        Ring->ReadIdx = secondChunk;
    }

    Ring->DataAvailable -= toRead;
    *BytesRead = toRead;
}


VOID
MmbiEvtIoWrite(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DeviceContext(device);
    NTSTATUS status;
    PVOID buffer;
    ULONG bytesWritten = 0;

    status = WdfRequestRetrieveInputBuffer(Request, Length, &buffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    WdfSpinLockAcquire(GlobalContext.Lock);
    
    if (context->DeviceID == 0) {
        RingBufferWrite(&GlobalContext.HostToBmc, buffer, (ULONG)Length, &bytesWritten);
    } else {
        RingBufferWrite(&GlobalContext.BmcToHost, buffer, (ULONG)Length, &bytesWritten);
    }
    
    WdfSpinLockRelease(GlobalContext.Lock);
    
    if (bytesWritten == 0 && Length > 0) {
        // Buffer full
        WdfRequestComplete(Request, STATUS_DEVICE_BUSY);
    } else {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytesWritten);
    }
}

VOID
MmbiEvtIoRead(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DeviceContext(device);
    NTSTATUS status;
    PVOID buffer;
    ULONG bytesRead = 0;

    status = WdfRequestRetrieveOutputBuffer(Request, Length, &buffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    WdfSpinLockAcquire(GlobalContext.Lock);
    
    if (context->DeviceID == 0) {
        // Reading from BmcToHost ring
        RingBufferRead(&GlobalContext.BmcToHost, buffer, (ULONG)Length, &bytesRead);
    } else {
        // Reading from HostToBmc ring
        RingBufferRead(&GlobalContext.HostToBmc, buffer, (ULONG)Length, &bytesRead);
    }

    WdfSpinLockRelease(GlobalContext.Lock);

    // If 0 bytes read, simulation returns SUCCESS with 0 bytes (non-blocking poll)
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytesRead);
}
