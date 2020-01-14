#include <ntddk.h>
#include "util.h"
#include "main.h"

PEPROCESS Process;
NTSTATUS Status;
ULONG BytesIO = 0;
PIO_STACK_LOCATION stack;
ULONG ControlCode;
PKERNEL_REQUEST request;
uint64_t* OutPut;
SIZE_T File = 0;

typedef struct readStruct
{
	ULONGLONG UserBufferAdress;
	ULONGLONG GameAddressOffset;
	ULONGLONG ReadSize;
	ULONG     UserPID;
	ULONG     GamePID;
	BOOLEAN   WriteOrRead;
	UINT32	  ProtocolMsg;
} ReadStruct, *pReadStruct;


NTSTATUS KeReadVirtualMemory(PEPROCESS Process_, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size)
{
	File = 0;
	NTSTATUS status;
	try {
		if (NT_SUCCESS(MmCopyVirtualMemory(Process_, SourceAddress, PsGetCurrentProcess(), TargetAddress, Size, KernelMode, &File)))
			status = STATUS_SUCCESS;
		else
			status = STATUS_ACCESS_DENIED;
	}except(EXCEPTION_EXECUTE_HANDLER) {
		status = STATUS_ACCESS_DENIED;
	}
	return status;
}

NTSTATUS DevioctlDispatch(
	_In_ struct _DEVICE_OBJECT *DeviceObject,
	_Inout_ struct _IRP *Irp
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	BytesIO = 0;
	stack = IoGetCurrentIrpStackLocation(Irp);
	pReadStruct readStruct;
	try {
		readStruct = (pReadStruct)Irp->AssociatedIrp.SystemBuffer;
		if (readStruct->ProtocolMsg == 0)
		{
			if (NT_SUCCESS(PsLookupProcessByProcessId(readStruct->GamePID, &Process))) {
				Status = KeReadVirtualMemory(Process, readStruct->GameAddressOffset, readStruct->UserBufferAdress, readStruct->ReadSize);
				ObDereferenceObject(Process);
			}
			else {
				Status = STATUS_INVALID_PARAMETER;
			}
			BytesIO = sizeof(KERNEL_REQUEST);
		}
		else
		{
			Status = STATUS_INVALID_PARAMETER;
			BytesIO = 0;
		}
	}except(EXCEPTION_EXECUTE_HANDLER) {
		STATUS_INVALID_PARAMETER;
	}
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = BytesIO;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}


NTSTATUS UnsupportedDispatch( _In_ struct _DEVICE_OBJECT *DeviceObject, _Inout_ struct _IRP *Irp )
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}

NTSTATUS CreateDispatch( _In_ struct _DEVICE_OBJECT *DeviceObject, _Inout_ struct _IRP *Irp )
{
	UNREFERENCED_PARAMETER(DeviceObject);

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}

NTSTATUS CloseDispatch( _In_ struct _DEVICE_OBJECT *DeviceObject, _Inout_ struct _IRP *Irp )
{
	UNREFERENCED_PARAMETER(DeviceObject);

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}

NTSTATUS DriverInitialize( _In_  struct _DRIVER_OBJECT *DriverObject, _In_  PUNICODE_STRING RegistryPath )
{
	NTSTATUS        status;
	UNICODE_STRING  SymLink, DevName;
	PDEVICE_OBJECT  devobj;
	ULONG           t;

	UNREFERENCED_PARAMETER(RegistryPath);

	RtlInitUnicodeString(&DevName, L"\\Device\\driverIOCTL");
	status = IoCreateDevice(DriverObject, 0, &DevName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, TRUE, &devobj);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	RtlInitUnicodeString(&SymLink, L"\\DosDevices\\driverIOCTL");

	status = IoCreateSymbolicLink(&SymLink, &DevName);

	devobj->Flags |= DO_BUFFERED_IO;

	for (t = 0; t <= IRP_MJ_MAXIMUM_FUNCTION; t++)
		DriverObject->MajorFunction[t] = &UnsupportedDispatch;

	DriverObject->MajorFunction[IRP_MJ_WRITE] = &DevioctlDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = &CreateDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = &CloseDispatch;
	DriverObject->DriverUnload = NULL;

	devobj->Flags &= ~DO_DEVICE_INITIALIZING;

	return status;
}

NTSTATUS DriverEntry( _In_  struct _DRIVER_OBJECT *DriverObject,  _In_  PUNICODE_STRING RegistryPath)
{

	NTSTATUS        status;
	UNICODE_STRING  drvName;

	RtlInitUnicodeString(&drvName, L"\\Driver\\driverIOCTL");

	status = IoCreateDriver(&drvName, &DriverInitialize);

	return status;
}
