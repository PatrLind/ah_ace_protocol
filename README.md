WORK IN PROGRESS!!!

# Allen & Heath ACE network protocol information
ACE stands for *Audio Control over Ethernet* and is a proprietary protocol made by Allen & Heath Limited in the UK.

I wanted to understand more about how this protocol worked so I tried to capture some data of the link between the mixrack and the mix surface. I present my findings here in case someone else might find this useful. If you have more information than I have found or if you find any errors, please feel free tell me.

I don't currently have access to the equipment, but the data I captured was from an iLive iDR-48 mixrack and an iLive-T112 surface.

**NOTE**: Only use the provided information if you are sure you know what you are doing. This information and provided source code is for educational purposes only and should not be used to replace a proper audio recording system (like Dante). I cannot guarantee the quality of the recordings you might get from the audio stream.

## The physical ACE connection
The ACE connection is (as far as I can tell) just a normal Ethernet port that can be connected to any normal Ethernet compatible equipment. However, just because it is Ethernet compatible, you cannot expect it to work together with other network devices unless you have a very special setup. More on this in the section about the ACE protocol.
The connector is an etherCON RJ45 connector from Neutrik that allows for a secure and durable connection that is suitable for the expected live sound environment. If you find yourself in a situation where you want to connect the etherCON into a normal Ethernet switch it is usually possible to temporarily remove the outer connector “shield” by unscrewing it just like a XLR connector.

Allen & Heath claims that, unlike a normal Ethernet connection, the ACE connection can run thru cables that are up to 120m in length. Normally this is limited to 100m. I’m guessing that the can get this number because they know exactly what transceivers and chipsets they are using and have done extensive tests in order to determine that their particular setup can handle cable lengths of up to 120m. I have however read about people that have had problems, so maybe it is best to limit the length to 100m just to be on the safe side.

## ACE protocol
![Channel 0 Wireshark data](https://github.com/Ramzeus/ah_ace_protocol/blob/master/images/sample_packet_wireshark.png "Annotated data from Wireshark")

The ACE protocol is not based on “normal” IP communication but is instead just simple RAW Ethernet frames. The frames consist of the following data fields (not including preamble and start frame delimiter):
*	Destination MAC address (6 bytes) – Always ff:ff:ff:ff:ff:ff (Broadcast)
*	Source MAC address (6 bytes)
*	EtherType field (2 bytes)
*	Optionally (if a VLAN capable switch is used) also a 802.1Q tag (4 bytes)
*	65 audio channels (3 bytes each with a total of 195 bytes)
*	Then comes 26 bytes of data that is used for control data and also bridged network data.

The total data size is always 221 bytes and the total frame size is either 239 bytes (if a VLAN is used) or 235 bytes (normal packets without VLAN).
Also packet sizes of 237 and maybe 241 exist, with just two bytes of checksum appended at the end. (237 Found on iLive v1.94, intermittent) My current best theory is that they might use the occasional trailer to achieve synchronization, but it does not seem strictly necessary.

The protocol is very sensitive to other traffic on the network and will not tolerate other packets (from what I can see anyway). While testing I had a switch that had CDP (Cisco Discovery Protocol) activated and that caused glitches in the communication between the surface and the mixrack.

The protocol is designed a point to point protocol and only two devices will be able to talk to each other. The devices are always sending packets thru the network even if there is no data to transfer. In that case the packets will contain only zeroes. The data rate transferred amounts to about 48000 * 221 = 81 Mibit/s. If you include the header data the total rate amounts to about 90 Mibit/s. This is in both directions so about 181 Mibit/s if you want to capture all traffic. The wire speed is fixed to 100 Mibit/s.

## Audio
Packets/frames like this are sent 48000 times/second which also is the audio sampling frequency of the system. The three bytes per audio sample makes up the 24 bit sample rate.
The first channel (channel 0) is a sync signal (I think) that the system (presumably) uses to keep all devices synchronized. Only the least significant byte contains data, the other two bytes is always zero. The sync packets I have seen are in this byte sequence:

0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7C*, 0x40, 0x44, ...*

And the wave form looks like this (Normalized in Audacity):

![Channel 0 sync track](https://github.com/Ramzeus/ah_ace_protocol/blob/master/images/sync.png "Normalized in Audacity")

This seems like a 6 bit counter starting at the 3rd LSb, with the 2nd MSb set to one as a flag.
In comparison, some of the other A&H protocols like dSnake use the same 6bit counter, but just in the lowest bits. In general ACE and dSnake are mostly compatible, at least on the audio level. Sync and control are probably slightly different.

The rest of the channels (1 to 64) are all normal audio channels.
In order to convert the samples transported on the network to samples that are working in a PCM wave file I had to change the byte order around a bit using this C function:
```c++
uint32_t switchByteOrder24(uint32_t src)
{
	// Switch byte 0 and 2
	src = (src & 0xff0000) >> 16 | (src & 0x00ff00) | (src & 0x0000ff) << 16;
	// Switch nibble 0 and 1 in each byte
	src = (src & 0xf0f0f0) >>  4 | (src & 0x0f0f0f) <<  4;
	return src;
}
```

### Channel numbers
The mixer channel number and ACE stream channel order numbers are not the same thing. If all inputs are mapped straight to the ACE port then they will be spaced 8 channels apart like this:
* Channel  1 to ACE  1
* Channel  2 to ACE  9 
* Channel  3 to ACE 17 
* Channel  4 to ACE 25 
* Channel  5 to ACE 33 
* Channel  6 to ACE 41 
* Channel  7 to ACE 49 
* Channel  8 to ACE 57 
* Channel  9 to ACE  2 
* Channel 10 to ACE 10 
* ...

To convert the channel numbers you may use this function:
```c++
// Channel 0 meaning sync, therefore the exception
if(channel == 0) {
    return 0;
}
return (((channel-1)*8)%63)+1;
```

## Tunneled Network / Control
The last 26 bytes are a tunneled network connection, for both external connections on the network ports of both surface and mixrack, and also internal control via a different Protocol (called AH-Net on ILive devices).
The first byte is a control value with the nibbles swapped, and can be decoded like following:
```c++
uint8_t switchNibbles(uint8_t src)
{
	// Switch nibble 0 and 1
	src = (src & 0xf0) >>  4 | (src & 0x0f) <<  4;
	return src;
}
```
The resulting value will be:
* 0x00 -> No Data will be transmitted, all zeroes
* 0x59 -> Start of a new ethernet frame
* 0x80 to 0x99 -> The first bit is always high, the lower 5 bits indicate the number of valid bytes following
  * 0x99 therefor means all 25 following bytes are valid and there will be more data
  * 0x80 therefor means all 25 bytes are invalid and zero and the frame has ended
  * 0x85 therefor means the first 5 following bytes are valid and the frame ends after that.

In bitfield terms that means the two most significant bits are flags. The MSb means the data is of the same frame as the previous packet, the 2nd MSb means the data is the start of a new frame. If both are not set there will be no data following and the last frame is complete.
And the lower 5 bits are just the number of valid bits of data following.

The other 25 bytes will contain the raw ethernet frames including the standard 8 byte preamble (0x5555555555555559).
The frames can be reconstructed by appending the 25 byte chunks in order and dropping the padding zeros of the last chunck according to the control byte.

Further work will need to be done on the tunneled protocols, if one wants to understand the control of these systems.
ACE itself doesn't seem to carry any relevant control data itself.

## Sample software
TODO

## Problems
I have had some problems with packet drops and therefore I was missing some of the audio data. I'm still investigating how to best fix this issue.

I still have not figured out how they synchronize both devices. Also i still can't figure out why sometimes one or the other device will omit the two bytes of checksum on every fourth packet.

License
----

MIT


