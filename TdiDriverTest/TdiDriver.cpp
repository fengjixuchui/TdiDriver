#include "TdiDriver.h"

#ifdef __cplusplus
extern "C"
{
#endif

PDEVICE_OBJECT g_TcpFltObj = NULL;
PDEVICE_OBJECT g_UdpFltObj = NULL;
PDEVICE_OBJECT g_TcpOldObj = NULL;
PDEVICE_OBJECT g_UdpOldObj = NULL;


typedef struct _IP_ADDRESS
{
	union
	{
		ULONG ipInt;
		UCHAR ipUChar[4];
	};
}IP_ADDRESS, *PIP_ADDRESS;

USHORT
TdiFilter_Ntohs(IN USHORT v)
{
	return (((UCHAR)(v >> 8)) | (v & 0xff) << 8);
};

ULONG
TdiFilter_Ntohl(IN ULONG v)					//�ߵ�IP��ַ��˳��
{
	return ((v & 0xff000000) >> 24 |
		(v & 0xff0000) >> 8 |
		(v & 0xff00) << 8 |
		((UCHAR)v) << 24);
};

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	KdPrint(("TdiDriver: ����ж��\n"));
	DetachAndDeleteDevie(DriverObject, g_TcpFltObj, g_TcpOldObj);
	// DetachAndDeleteDevie(DriverObject, g_UdpFltObj, g_UdpOldObj);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	int i = 0;

	KdPrint(("TdiDriver: ��������\n"));
	DriverObject->DriverUnload = DriverUnload;

	for (i=0; i<IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = TdiPassThrough;
	}
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = TdiInternalDeviceControl;			//��Ҫ���������������

	//��ʼ���������豸
	CreateAndAttachDevice(DriverObject, &g_TcpFltObj, &g_TcpOldObj, TCP_DEVICE_NAME);
//	CreateAndAttachDevice(DriverObject, &g_UdpFltObj, &g_UdpOldObj, UDP_DEVICE_NAME);

	return status;
}



NTSTATUS CreateAndAttachDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT * fltObj, PDEVICE_OBJECT * oldObj, PWCHAR deviceName)
{
	NTSTATUS status;
	UNICODE_STRING deviceNameStr;

	status = IoCreateDevice(DriverObject,
		0,
		NULL,
		FILE_DEVICE_UNKNOWN,
		0,
		TRUE,
		fltObj);

	if(!NT_SUCCESS(status))
	{
		KdPrint(("TdiDriver: �����豸ʧ��\n"));
		return status;
	}

	(*fltObj)->Flags |= DO_DIRECT_IO;

	//��ʼ��ָ�����豸

	RtlInitUnicodeString(&deviceNameStr, deviceName);

	status = IoAttachDevice(*fltObj, &deviceNameStr, oldObj);

	if(!NT_SUCCESS(status))
	{
		KdPrint(("TdiDriver: ���豸%wZʧ��\n", &deviceNameStr));
		return status;
	}

	return STATUS_SUCCESS;

}

VOID DetachAndDeleteDevie(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT fltObj, PDEVICE_OBJECT oldObj)
{
	IoDetachDevice(oldObj);
	IoDeleteDevice(fltObj);
}



NTSTATUS TdiInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

	NTSTATUS status;

	PTRANSPORT_ADDRESS transportAddress = NULL;

	PTDI_REQUEST_KERNEL_CONNECT tdiReqKelConnect = NULL;			
	IP_ADDRESS ipAddress;
	switch (irpStack->MinorFunction)
	{
	case TDI_CONNECT:
		{
			
			KdPrint(("Tdi Driver: TDI_CONNECT!\n"));

			tdiReqKelConnect = (PTDI_REQUEST_KERNEL_CONNECT)(&irpStack->Parameters);

			if(!tdiReqKelConnect->RequestConnectionInformation)
			{
				KdPrint(("Tdi Driver: no request!\n"));
				IoSkipCurrentIrpStackLocation(Irp);
				status = IoCallDriver(g_TcpOldObj, Irp);
			}

			if(tdiReqKelConnect->RequestConnectionInformation->RemoteAddressLength == 0)
			{
				KdPrint(("Tdi Driver: RemoteAddressLength=0\n"));
				IoSkipCurrentIrpStackLocation(Irp);
				status = IoCallDriver(g_TcpOldObj, Irp);
			}


			transportAddress = (PTRANSPORT_ADDRESS)(tdiReqKelConnect->RequestConnectionInformation->RemoteAddress);

			switch (transportAddress->Address->AddressType)
			{
			case TDI_ADDRESS_TYPE_IP:
			{
				PTDI_ADDRESS_IP tdiAddressIp = (PTDI_ADDRESS_IP)(transportAddress->Address->Address);
				ipAddress.ipInt = tdiAddressIp->in_addr;
				USHORT port = TdiFilter_Ntohs(tdiAddressIp->sin_port);
				KdPrint(("TdiDriver: ip:%d.%d.%d.%d, port:%d\n", ipAddress.ipUChar[0], ipAddress.ipUChar[1], ipAddress.ipUChar[2], ipAddress.ipUChar[3], port));
				break;
			}
			case TDI_ADDRESS_TYPE_IP6:
				break;
			default:
				break;
			}

			IoSkipCurrentIrpStackLocation(Irp);
			status = IoCallDriver(g_TcpOldObj, Irp);
			break;
		}

	default:
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(g_TcpOldObj, Irp);
		break;
	}

	return status;
}

NTSTATUS TdiPassThrough(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	//����豸����������Ҫ���˵ģ�ֱ�Ӵ����²���豸����
	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(g_TcpOldObj, Irp);
	return status;
}


#ifdef __cplusplus
}
#endif