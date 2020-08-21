#include <ntddk.h>


#define DEV_NAME			L"\\Device\\BrviewNTDriverDevice"
#define SYN_LINKE_NAME		L"\\??\\BrviewNTDriver"
#define MAX_FILE_LENGTH		100

/// @brief �豸��չ�ṹ
typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT PDeviceObject;
	UNICODE_STRING DeviceName; ///< �豸����
	UNICODE_STRING SymLinkName; ///< ����������

	//ģ����ļ�������
	unsigned char* pFileBuffer;
	//�ļ�����
	ULONG FileLength;

} DEVICE_EXTENSION, * PDEVICE_EXTENSION;



NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING devName;
	UNICODE_STRING symLinkName;
	PDEVICE_OBJECT pDevObj = NULL;
	PDEVICE_EXTENSION pDevExt = NULL;
	NTSTATUS status;

	// �����豸����
	RtlInitUnicodeString(&devName, DEV_NAME);
	// �����豸
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

	// ������������
	// �豸����ֻ���ں�̬�пɼ�
	// ��������, ����Ӧ�ó�����豸����
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
	// ɾ����������
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
	// �ͷſռ�
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

		// ��ȡҪд����ֽ����Լ�ƫ��
		ULONG writeLength = pIoStack->Parameters.Write.Length;
		ULONG writeOffset = (ULONG)pIoStack->Parameters.Write.ByteOffset.QuadPart;


		if ((writeLength + writeOffset) > MAX_FILE_LENGTH)
		{
			status = STATUS_FILE_INVALID;
			completeLength = 0;
			break;
		}

		// д������
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

		// ��ȡ��Ҫ���豸���ֽ�����ƫ��
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


/// @brief ��ʼ����������
/// @param[in] pDriverObject ��������
/// @param[in] pRegPath ����������ע����е�·��
/// @return ��ʼ������״̬
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegPath)
{
	KdPrint(("Enter DriverEntry\n"));

	// ע���������ú������
	// ��Щ�������������������������, �����ɲ���ϵͳ�������
	pDriverObject->DriverUnload = NtDvr_Unload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = NtDvr_Create;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = NtDvr_Close;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = NtDvr_Write;
	pDriverObject->MajorFunction[IRP_MJ_READ] = NtDvr_Read;

	// ���������豸����
	NTSTATUS status = CreateDevice(pDriverObject);

	KdPrint(("Leave DriverEntry\n"));

	return status;
}