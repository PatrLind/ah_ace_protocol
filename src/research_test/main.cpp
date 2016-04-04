#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <winsock2.h>
#include "pcap.h"
#include <string>

using namespace std;

typedef struct pcap_hdr_s {
	uint32_t magic_numbr;
	uint16_t version_major;
	uint16_t version_minor;
	int32_t thiszone;
	uint32_t sigfigs;
	uint32_t snaplen;
	uint32_t network;
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
	uint32_t ts_sec;
	uint32_t ts_usec;
	uint32_t incl_len;
	uint32_t orig_len;
} pcaprec_hdr_t;

bool g_doAbort = false;

bool read_packet(pcaprec_hdr_t& packetHeader, uint8_t* packetData, uint32_t dataSizeMax, uint32_t& packetDataSize, FILE* fp)
{
	size_t bytesRead = 0;
	bytesRead = fread(&packetHeader, 1, sizeof(packetHeader), fp);
	if (bytesRead == 0) {
		return false;
	} else if (bytesRead != sizeof(packetHeader)) {
		printf("Error reading packet header! (%d != %d)\n", bytesRead, sizeof(packetHeader));
		return false;
	}
	if (packetHeader.incl_len > dataSizeMax) {
		printf("Packet too large! (%d > %d)\n", packetHeader.incl_len, dataSizeMax);
		return false;
	}
	bytesRead = fread(packetData, 1, packetHeader.incl_len, fp);
	if (bytesRead != packetHeader.incl_len) {
		printf("Could only read %d bytes of the stated %d bytes in packet!\n", bytesRead, packetHeader.incl_len);
		return false;
	}
	packetDataSize = bytesRead;
	return true;
}

bool openOutFile(char* baseFileName, FILE* outFile[], size_t channel, size_t ch, char* sourceName)
{
	if (outFile[ch] == NULL) {
		char fname[64] = {};
		sprintf(fname, "%s_%s_%02d.wav", baseFileName, sourceName, channel);
		outFile[ch] = fopen(fname, "wb+");
		if (!outFile[ch]) {
			printf("Failed to open output file '%s': %d\n", fname, errno);
			return false;
		}
		uint32_t temp32 = 0;
		uint32_t temp16 = 0;
		// Main chunk
		fwrite("RIFF", 1, 4, outFile[ch]);
		temp32 = 36; // This is a placeholder size!
		fwrite(&temp32, 1, 4, outFile[ch]); // size
		fwrite("WAVE", 1, 4, outFile[ch]); // format

		// fmt  chunk
		fwrite("fmt ", 1, 4, outFile[ch]);
		temp32 = 16;
		fwrite(&temp32, 1, 4, outFile[ch]); // size
		temp16 = 1; // PCM
		fwrite(&temp16, 1, 2, outFile[ch]); // audio format
		temp16 = 1; // 1 channel
		fwrite(&temp16, 1, 2, outFile[ch]); // channel count
		temp32 = 48000; // Sample frequency
		fwrite(&temp32, 1, 4, outFile[ch]); // Sample frequency
		temp32 = 48000 * 1 * 3; // freq * channels * bitsPerSample/8
		fwrite(&temp32, 1, 4, outFile[ch]); // Byte rate
		temp16 = 1 * 3; // channels * bitsPerSample/8
		fwrite(&temp16, 1, 2, outFile[ch]); // Block align
		temp16 = 24; // Bits per sample
		fwrite(&temp16, 1, 2, outFile[ch]); // Bits per sample

		// data chunk
		fwrite("data", 1, 4, outFile[ch]);
		temp32 = 16;
		temp32 = 0; // This is a placeholder size!
		fwrite(&temp32, 1, 4, outFile[ch]); // size
	}
	return true;
}

uint8_t reverse(uint8_t b)
{
	b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
	//b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
	//b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
	return b;
}

uint32_t switchByteOrder24(uint32_t src)
{
	// Switch byte 0 and 2
	src = (src & 0xff0000) >> 16 | (src & 0x00ff00) | (src & 0x0000ff) << 16;
	// Switch nibble 0 and 1 in each byte
	src = (src & 0xf0f0f0) >>  4 | (src & 0x0f0f0f) <<  4;
	return src;
}

uint32_t setNextSync(uint32_t syncSample)
{
	if (syncSample == 0) {
		return 0;
	}
	uint8_t nextSyncSample = (syncSample & 0xff) + 0x04;
	if ((nextSyncSample & 0x0f) == 0x00) {
		if ((nextSyncSample & 0xf0) == 0x80) {
			nextSyncSample = 0x40 + (nextSyncSample & 0x0f);
		}
	}
	return nextSyncSample;
}

std::string getDeviceNameFromUser()
{
	char errbuf[PCAP_ERRBUF_SIZE] = {};
	pcap_if_t* device = nullptr;
	/* Retrieve the device list */
	pcap_if_t* alldevs = nullptr;
	if(pcap_findalldevs(&alldevs, errbuf) == -1) {
		fprintf(stderr,"Error in pcap_findalldevs: %s\n", errbuf);
		return "";
	}

	/* Print the list */
	size_t interfaceIndex = 0;
	for (device=alldevs; device; device=device->next) {
		printf("%d. %s", ++interfaceIndex, device->name);
		if (device->description) {
			printf(" (%s)\n", device->description);
		} else {
			printf(" (No description available)\n");
		}
	}
	size_t interfaceCount = interfaceIndex;

	if (interfaceCount == 0)
	{
		printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
		return "";
	}

	printf("Enter the interface number to capture from (1-%d):", interfaceCount);
	scanf("%d", &interfaceIndex);

	if (interfaceIndex < 1 || interfaceIndex > interfaceCount)
	{
		printf("\nInterface number out of range.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return "";
	}

	std::string ret;
	/* Jump to the selected adapter */
	size_t i = 0;
	for (device=alldevs; i<interfaceIndex-1 ;device=device->next, i++) {}
	ret = device->name;
	pcap_freealldevs(alldevs);
	return ret;
}

BOOL WINAPI AbortHandler(DWORD ctrlType)
{
	g_doAbort = true;
	return 1;
}

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: %s [file.pcap] [out_file.raw]\n", argv[0]);
		return -1;
	}

	std::string deviceName = getDeviceNameFromUser();
	if (deviceName.empty()) {
		return -1;
	}

	/* Open the adapter */
	char errbuf[PCAP_ERRBUF_SIZE] = {};
	pcap_t* pcapHandle = nullptr;
	if ((pcapHandle = pcap_open_live(deviceName.c_str(), 65536, 0, 1000, errbuf)) == nullptr)
	{
		fprintf(stderr, "\nUnable to open the adapter. %s is not supported by WinPcap\n", deviceName.c_str());
		return -1;
	}
	pcap_setbuff(pcapHandle, 1024*1024*20);
	pcap_setmintocopy(pcapHandle, 1024*1024*5);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	SetConsoleCtrlHandler(AbortHandler, TRUE);

	/*FILE* inFile = fopen(argv[1], "rb");
	if (!inFile) {
		printf("Failed to open input file '%s': %d\n", argv[1], errno);
		return -1;
	}*/

	const size_t outFileMaxCount = 66*16;
	FILE* outFile[outFileMaxCount] = {};

	/*// Read PCAP header
	pcap_hdr_t header;
	size_t bytesRead = 0;
	bytesRead = fread(&header, 1, sizeof(header), inFile);
	if (!bytesRead) {
		printf("Failed to read header\n");
		return 1;
	}
	if (header.magic_numbr != 0xa1b2c3d4) {
		printf("Wrong magic number\n");
		return 1;
	}
	printf("PCAP file version: %d.%d\n", header.version_major, header.version_minor);
	printf("Snapshot length: %d\n", header.snaplen);
*/
	//pcaprec_hdr_t packetHeader;
	//uint8_t packetData[65535] = {};
	uint32_t packetSize = 0;
	const size_t channelBufferCount = outFileMaxCount;
	const size_t channelBufferSize = 1024 * 1024;
	uint32_t channelBuffersOffset[channelBufferCount] = {};
	uint32_t channelTotalSize[channelBufferCount] = {};
	uint8_t* channelBuffers[channelBufferCount] = {};
	bool channelActive[channelBufferCount] = {};
	for (size_t i=0; i<channelBufferCount; i++) {
		channelBuffers[i] = (uint8_t*)calloc(1, channelBufferSize);
		channelActive[i] = true;
	}
	char sourceNames[16][20] = {};
	uint64_t sourceList[16] = {};
	uint32_t nextSync[16] = {};
	timeval firstPacketTime;
	uint64_t i = 0;

	struct pcap_pkthdr* packetHeader;
	const u_char* packetData;

	//while (read_packet(packetHeader, packetData, sizeof(packetData), packetSize, inFile)) {
	int res = 0;
	time_t lastTime = time(NULL);
	while((res = pcap_next_ex(pcapHandle, &packetHeader, &packetData)) >= 0 && !g_doAbort) {
		packetSize = packetHeader->caplen;
		if (time(NULL) - lastTime >= 60) {
			lastTime = time(NULL);
			pcap_stat stat;
			if (pcap_stats(pcapHandle, &stat) == 0) {
				printf("Capture stats - Dropped: %d, IF-Dropped: %d\n", stat.ps_drop, stat.ps_ifdrop);
			}
		}
		if (res == 0) {
			continue;
		}
		if (i == 0) {
			firstPacketTime = packetHeader->ts;
			printf("%ld\n", firstPacketTime.tv_sec);
		}
		i++;
		if (packetSize != 239 && packetSize != 235)
			continue;
		bool vlan = packetSize == 239;
		char* currentSourceName = NULL;
		size_t currentSourceId = 0;
		uint64_t destinationAddress = *((uint64_t*)(packetData + 6)) & 0x0000ffffffffffffL;
		for (size_t j=0; j<sizeof(sourceList)/sizeof(sourceList[0]); j++) {
			if (sourceList[j] == destinationAddress) {
				currentSourceName = sourceNames[j];
				currentSourceId = j;
				break;
			} else if (sourceList[j] == 0) {
				sourceList[j] = destinationAddress;
				sprintf(sourceNames[j], "%012I64x", destinationAddress);
				currentSourceName = sourceNames[j];
				currentSourceId = j;
				break;
			} else {
			}
		}
		//printf("%s - %d\n", currentSourceName, currentSourceId);
		const size_t channelDataSize = 3;
		size_t offset = vlan ? 18 : 14;
		size_t channel = 0;
		size_t ch = 0;
		for (channel = 0; channel<65; channel++) {
			if (channelActive[channel]) {
				ch = channel + (66*currentSourceId);
				openOutFile(argv[2], outFile, channel, ch, currentSourceName);
				if (channelBuffersOffset[ch] + channelDataSize > channelBufferSize) {
					fwrite(channelBuffers[ch], 1, channelBuffersOffset[ch], outFile[ch]);
					channelBuffersOffset[ch] = 0;
				}
				uint32_t srcSample = 0;
				uint32_t dstSample = 0;
				srcSample = *((uint32_t*)(packetData + offset));
				dstSample = switchByteOrder24(srcSample);
				if (channel == 0) {
					if (dstSample != nextSync[currentSourceId] && nextSync[currentSourceId] != 0) {
						fprintf(stderr, "Sync error in packet %I64d (%ld.%lds) (SyncData: %02x, Expected: %02x)\n", i, packetHeader->ts.tv_sec - firstPacketTime.tv_sec, packetHeader->ts.tv_usec,  dstSample, nextSync[currentSourceId]);
					}
					nextSync[currentSourceId] = setNextSync(dstSample);
				}
				memcpy(channelBuffers[ch] + channelBuffersOffset[ch], &dstSample, 3);
				channelBuffersOffset[ch] += channelDataSize;
				channelTotalSize[ch] += channelDataSize;
			}
			offset += channelDataSize;
		}
		// Write the rest
		//if (!channelActive[channel])
			continue;
		ch = channel + (66*currentSourceId);
		size_t sizeRest = packetSize - offset;
		openOutFile(argv[2], outFile, channel, ch, currentSourceName);
		if (channelBuffersOffset[ch] + sizeRest > channelBufferSize) {
			fwrite(channelBuffers[ch], 1, channelBuffersOffset[ch], outFile[ch]);
			channelBuffersOffset[ch] = 0;
		}
		memcpy(channelBuffers[ch] + channelBuffersOffset[ch], packetData + offset, sizeRest);
		offset += sizeRest;
		channelBuffersOffset[ch] += sizeRest;
		channelTotalSize[ch] += sizeRest;
	}

	for (size_t i=0; i<channelBufferCount; i++) {
		if (outFile[i] != NULL) {
			fwrite(channelBuffers[i], 1, channelBuffersOffset[i], outFile[i]);
		}
	}

	//fclose(inFile);
	//inFile = NULL;
	pcap_close(pcapHandle);
	for (size_t x=0; x<outFileMaxCount; x++) {
		if (outFile[x] != NULL) {
			uint32_t temp32 = 0;
			fseek(outFile[x], 4, SEEK_SET);
			temp32 = 36 + channelTotalSize[x];
			fwrite(&temp32, 1, 4, outFile[x]);
			fseek(outFile[x], 40, SEEK_SET);
			temp32 = channelTotalSize[x];
			fwrite(&temp32, 1, 4, outFile[x]);
			fclose(outFile[x]);
			outFile[x] = NULL;
		}
	}
	for (size_t i=0; i<channelBufferCount; i++) {
		free(channelBuffers[i]);
	}

	printf("Done! %I64d packets processed\n", i);
	return 0;
}

