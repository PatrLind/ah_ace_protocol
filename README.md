WORK IN PROGRESS!!!

# Allen & Heath ACE network protocol information
ACE stands for *Audio Control over Ethernet* and is a proprietary protocol made by Allen & Heath Limited in the UK.

I wanted to understand more about how this protocol worked so I tried to capture some data of the link between the mix rack and the mix surface. I present my findings here in case someone else might find this useful. If you have more information than I have found, please feel free to help me add it here.

**NOTE**: Only use the provided information if you are sure you know what you are doing. This information and provided source code is for educational purposes only and should not be used to replace a proper audio recording system (like Dante). I cannot guarantee the quality of the recordings you might get from the audio stream.

## The physical ACE connection
The ACE connection is (as far as I can tell) just a normal Ethernet port that can be connected to any normal Ethernet compatible equipment. However, just because it is Ethernet compatible, you cannot expect it to work together with other network devices unless you have a very special setup. More on this in the section about the ACE protocol.
The connector is an etherCON RJ45 connector from Neutrik that allows for a secure and durable connection that is suitable for the expected live sound environment. If you find yourself in a situation where you want to connect the etherCON into a normal Ethernet switch it is usually possible to temporarily remove the outer connector “shield” by unscrewing it just like a XLR connector.

Allen & Heath claims that, unlike a normal Ethernet connection, the ACE connection can run thru cables that are up to 120m in length. Normally this is limited to 100m. I’m guessing that the can get this number because they know exactly what transceivers and chipsets they are using and have done extensive tests in order to determine that their particular setup can handle cable lengths of up to 120m. I have however read about people that have had problems, so maybe it is best to limit the length to 100m just to be on the safe side.

## ACE protocol
The ACE protocol is not based on “normal” IP communication but is instead just simple RAW Ethernet frames. The frames consist of the following data fields (not including preamble and start frame delimiter):
*	Destination MAC address (6 bytes) – Always ff:ff:ff:ff:ff:ff (Broadcast)
*	Source MAC address (6 bytes)
*	EtherType field (2 bytes)
*	Optionally (if a VLAN capable switch is used) also a 802.1Q tag (4 bytes)
*	65 audio channels (3 bytes each with a total of 195 bytes) ???
*	Then comes 25 bytes of data (???) that is used for control data and also bridged network data.

The total data size is always 221 bytes and the total frame size is either 239 bytes (if a VLAN is used) or 235 bytes (normal packets without VLAN).
The protocol is very sensitive to other traffic on the network and will not tolerate other packets (from what I can see anyway). While testing I had a switch that had CDP (Cisco Discovery Protocol) activated and that caused glitches in the communication between the surface and the mix rack.

Packets/frames like this are sent 48000 times/second which also is the audio sampling frequency of the system. The three bytes per audio sample makes up the 24 bit sample rate.
The first channel (channel 0) is a sync signal (I think) that the system (presumably) uses to keep all devices synchronized. Only the least significant byte contains data, the other two bytes is always zero. The sync packets I have seen are in this byte sequence:

XX, XX, XX, XX…

And the wave form looks like this:

IMAGE

The rest of the channels (1 to 64) are all normal audio channels.
In order to convert the samples transported on the network to samples that are working in a PCM wave file I had to change the byte order around a bit using this C function:
```
C-Code
```
The last 25 (???) bytes of data I have not yet tried to analyze fully but the first byte seems to be a data stream type designator and the other 24 bytes seems to be the data stream. It is in this data stream that the bridged network data is transferred (???).

The protocol is designed a point to point protocol and only two devices will be able to talk to each other. The devices are always sending packets thru the network even if there is no data to transfer. In that case the packets will contain only zeroes. The data rate transferred amounts to about 48000 * 221 = 81 Mibit/s. If you include the header data the total rate amounts to about 90 Mibit/s. This is in both directions so about 181 Mibit/s if you want to capture all traffic. The wire speed is fixed to 100 Mibit/s.

License
----

MIT


