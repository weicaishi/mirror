#include <ntddk.h>


#define DEV_NAME			L"\\Device\\BrviewNTDriverDevice"
#define SYN_LINKE_NAME		L"\\??\\BrviewNTDriver"
#define MAX_FILE_LENGTH		100

/// @brief 设备扩展结构
typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT PDeviceObject;
	UNICODE_STRING DeviceName; ///< 设备名称
	UNICODE_STRING SymLinkName; ///< 符号链接名

	//模拟的文件缓冲区
	unsigned char* pFileBuffer;
	//文件长度
	ULONG FileLength;

} DEVICE_EXTENSION, * PDEVICE_EXTENSION;



NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING devName;
	UNICODE_STRING symLinkName;
	PDEVICE_OBJECT pDevObj = NULL;
	PDEVICE_EXTENSION pDevExt = NULL;
	NTSTATUS status;

	// 创建设备名称
	RtlInitUnicodeString(&devName, DEV_NAME);
	// 创建设备
	status = IoCreateDevice(
		pDriverObject,
		sizeof(DEVICE_EXTENSION),
		&devName,
		FILE_DEVICE_UNKNOWN,
		0,
		TRUE,
		&pDevObj);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	pDevObj->Flags |= DO_BUFFERED_IO;
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->PDeviceObject = pDevObj;
	pDevExt->DeviceName = devName;

	// 创建符号链接
	// 设备名称只在内核态中可见
	// 符号链接, 链接应用程序和设备名称
	RtlInitUnicodeString(&symLinkName, SYN_LINKE_NAME);
	pDevExt->SymLinkName = symLinkName;
	status = IoCreateSymbolicLink(&symLinkName, &devName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}

	return STATUS_SUCCESS;

}


void NtDvr_Unload(IN PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);



	DEVICE_EXTENSION* pDevExt = (PDEVICE_EXTENSION)pDriverObject->DriverExtension;
	// 删除符号链接
	IoDeleteSymbolicLink(&pDevExt->SymLinkName);

	KdPrint(("Enter HelloWDMUnload\n"));
	KdPrint(("Leave HelloWDMUnload\n"));
}


NTSTATUS NtDvr_Create(IN PDEVICE_OBJECT pDevObject, IN PIRP pIrp)
{
	KdPrint(("Enter NtDvr_Create\n"));
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;

	DEVICE_EXTENSION* pDevExt = (PDEVICE_EXTENSION)pDevObject->DeviceExtension;
	do
	{
		if (NULL != pDevExt->pFileBuffer)
		{
			status = STATUS_ALREADY_COMPLETE;
			break;
		}



		pDevExt->pFileBuffer = (unsigned char*)ExAllocatePool(NonPagedPool, MAX_FILE_LENGTH);
		if (NULL == pDevExt->pFileBuffer)
		{
			status = STATUS_BUFFER_ALL_ZEROS;
			break;
		}
		pDevExt->FileLength = 0;
		KdPrint(("BUFFER ALLOC\n"));

	} while (FALSE);




	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave NtDvr_Create\n"));

	return status;
}

NTSTATUS NtDvr_Close(IN PDEVICE_OBJECT pDevObject, IN PIRP pIrp)
{
	KdPrint(("Enter NtDvr_Close\n"));
	NTSTATUS status = STATUS_SUCCESS;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;

	DEVICE_EXTENSION* pDevExt = (PDEVICE_EXTENSION)pDevObject->DeviceExtension;
	// 释放空间
	if (NULL != pDevExt->pFileBuffer)
	{
		ExFreePool(pDevExt->pFileBuffer);
		pDevExt->pFileBuffer = NULL;
		KdPrint(("BUFFER FREE\n"));
	}


	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave NtDvr_Close\n"));

	return status;
}

NTSTATUS NtDvr_Write(IN PDEVICE_OBJECT pDevObject, IN PIRP pIrp)
{
	KdPrint(("Enter NtDvr_Write\n"));
	NTSTATUS status = STATUS_SUCCESS;
	ULONG completeLength = 0;


	do
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObject->DeviceExtension;
		PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

		// 获取要写入的字节数以及偏移
		ULONG writeLength = pIoStack->Parameters.Write.Length;
		ULONG writeOffset = (ULONG)pIoStack->Parameters.Write.ByteOffset.QuadPart;


		if ((writeLength + writeOffset) > MAX_FILE_LENGTH)
		{
			status = STATUS_FILE_INVALID;
			completeLength = 0;
			break;
		}

		// 写入数据
		memcpy(pDevExt->pFileBuffer + writeOffset, pIrp->AssociatedIrp.SystemBuffer, writeLength);
		if ((writeLength + writeOffset) > pDevExt->FileLength)
		{
			pDevExt->FileLength = writeOffset + writeLength;
		}
		status = STATUS_SUCCESS;
		completeLength = writeLength;

	} while (FALSE);


	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = completeLength;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("Leave NtDvr_Write\n"));

	return status;
}

NTSTATUS NtDvr_Read(IN PDEVICE_OBJECT pDevObject, IN PIRP pIrp)
{
	KdPrint(("Enter NtDvr_Read\n"));
	NTSTATUS status = STATUS_SUCCESS;
	ULONG completeLength = 0;

	do {
		PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObject->DeviceExtension;

		// 获取需要读设备的字节数和偏移
		ULONG readLength = pStack->Parameters.Read.Length;
		ULONG readOffset = (ULONG)pStack->Parameters.Read.ByteOffset.QuadPart;


		if (readOffset + readLength > MAX_FILE_LENGTH)
		{
			status = STATUS_FILE_INVALID;
			completeLength = 0;
			break;
		}

		memcpy(pIrp->AssociatedIrp.SystemBuffer, pDevExt->pFileBuffer + readOffset, readLength);
		status = STATUS_SUCCESS;
		completeLength = readLength;

	} while (FALSE);

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = completeLength;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("Leave NtDvr_Read\n"));

	return status;
}


/// @brief 初始化驱动程序
/// @param[in] pDriverObject 驱动对象
/// @param[in] pRegPath 驱动程序在注册表中的路径
/// @return 初始化驱动状态
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegPath)
{
	KdPrint(("Enter DriverEntry\n"));

	// 注册驱动调用函数入口
	// 这些函数不是由驱动程序本身负责调用, 而是由操作系统负责调用
	pDriverObject->DriverUnload = NtDvr_Unload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = NtDvr_Create;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = NtDvr_Close;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = NtDvr_Write;
	pDriverObject->MajorFunction[IRP_MJ_READ] = NtDvr_Read;

	// 创建驱动设备对象
	NTSTATUS status = CreateDevice(pDriverObject);

	KdPrint(("Leave DriverEntry\n"));

	return status;
}