// File: example_transfer_test.c
// Description: An example that tests the raw acquisition speed
// Acquisition mode used is triggered streaming with internal trigger
//

#define _CRT_SECURE_NO_WARNINGS

#include "ADQAPI.h"
#include <stdio.h>

#ifndef LINUX
#include <windows.h>
#else
#include <stdlib.h>
#include <string.h>
#endif

extern "C" {
	void adq_transfer_test(void *adq_cu, int adq_num, int adq_type);
}


int main(int argc, char* argv[])
{
	unsigned int n_of_devices = 0;
	int n_of_failed = 0;
	unsigned int adq_num = 0;
	unsigned int tmp_adq_num = 0;
	int n_of_opened_devices = 0;
	unsigned int pID = 0;
	int n_of_ADQ = 0;
	int apirev = 0;
	int exit = 0;
	char* product_name;
	void* adq_cu;
	struct ADQInfoListEntry* ADQlist;
	unsigned int err;

	//START:

	apirev = ADQAPI_GetRevision();

	printf("Transfer Test Example\n");
	printf("API Revision: %6d\n", apirev);

	adq_cu = CreateADQControlUnit();	//creates an ADQControlUnit
	if (!adq_cu)
	{
		printf("Failed to create adq_cu!\n");
		return 0;
	}

	ADQControlUnit_EnableErrorTrace(adq_cu, LOG_LEVEL_WARN, ".");


	if (!ADQControlUnit_ListDevices(adq_cu, &ADQlist, &n_of_devices))
	{
		printf("ListDevices failed!\n");
		err = ADQControlUnit_GetLastFailedDeviceError(adq_cu);
		printf(" Last error reported is: %08X.\n", err);
		if (err == 0x00000001)
		{
			printf("ERROR: The linked ADQAPI is not for the correct OS, please select correct x86/x64 platform when building.\n");
		}
		return 0;
	}

	printf("Select info array entry to open.\n\n");

	adq_num = 0xFFFFFFFF;

	if (n_of_devices == 0)
	{
		printf("No devices found!\n");
		DeleteADQControlUnit(adq_cu);
		return 0;
	}

	while (adq_num >= n_of_devices)
	{
		for (tmp_adq_num = 0; tmp_adq_num < n_of_devices; tmp_adq_num++)
		{
			printf("Entry #%u - ", tmp_adq_num);
			switch (ADQlist[tmp_adq_num].ProductID)
			{
			case PID_ADQ14: printf("ADQ14"); break;
			}
			printf("    [PID %04X; Addr1 %04X; Addr2 %04X; HWIF %i; Setup %i]\n",
				ADQlist[tmp_adq_num].ProductID, ADQlist[tmp_adq_num].AddressField1, ADQlist[tmp_adq_num].AddressField2, ADQlist[tmp_adq_num].HWIFType, ADQlist[tmp_adq_num].DeviceSetupCompleted);
		}

		if (n_of_devices > 1)
		{
			printf("\nEntry to open: ");
			scanf("%d", &adq_num);
		}
		else
		{
			adq_num = 0;
			printf("\nOnly one entry found. Opening entry: %u\n", adq_num);
		}
	}

	printf("Opening device...    ");

	if (ADQControlUnit_OpenDeviceInterface(adq_cu, adq_num))
		printf("success!\n");
	else
	{
		printf("failed!\n");
		goto error;
	}

	printf("Setting up device... ");

	if (ADQControlUnit_SetupDevice(adq_cu, adq_num))
		printf("success!\n");
	else
	{
		printf("failed!\n");
		goto error;
	}

	n_of_ADQ = ADQControlUnit_NofADQ(adq_cu);

	printf("Total opened units: %i\n\n", n_of_ADQ);

	n_of_failed = ADQControlUnit_GetFailedDeviceCount(adq_cu);

	if (n_of_failed > 0)
	{
		printf("Found but failed to start %d ADQ devices.\n", n_of_failed);
		goto error;
	}

	if (n_of_devices == 0)
	{
		printf("No ADQ devices found.\n");
		goto error;
	}

	n_of_opened_devices = ADQControlUnit_NofADQ(adq_cu);
	printf("\n\nNumber of opened ADQ devices found: %d \n", n_of_opened_devices);

	for (adq_num = 1; adq_num <= (unsigned int)n_of_opened_devices; adq_num++)
	{
		product_name = ADQ_GetBoardProductName(adq_cu, adq_num);
		printf("%2u: ", adq_num);
		printf(product_name, "\n");
	}

	adq_num = 0;
	while (adq_num > (unsigned int)n_of_opened_devices || adq_num < 1)
	{
		if (n_of_opened_devices > 1)
		{
			printf("\nSelect Device to operate: ");
			scanf("%d", &adq_num);
		}
		else
		{
			adq_num = 1;
			printf("\n\nOnly one device detected. Selected device to operate: %u\n", adq_num);
		}

		if (adq_num > (unsigned int)n_of_opened_devices || adq_num < 1)
			printf("\nIncorrect number, try again!\n");
	}

	pID = ADQ_GetProductID(adq_cu, adq_num);
	switch (pID)
	{
	case PID_ADQ14: adq_transfer_test(adq_cu, adq_num, 714); break;
	case PID_ADQ7: adq_transfer_test(adq_cu, adq_num, 7); break;
	default: printf("This example does not contain code for the selected device.\n\n"); break;
	}

	DeleteADQControlUnit(adq_cu);
	//goto START;

	return 0;

error:
	printf("Type 0 and ENTER to exit.\n");
	scanf("%d", &exit);
	DeleteADQControlUnit(adq_cu);
	return 0;
}
