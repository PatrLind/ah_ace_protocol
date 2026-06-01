meta:
  id: ace_link
  endian: be
seq:
  - id: dest_mac
    size: 6
    doc: 'Destination MAC Adress like regular ehternet frame.'
  - id: src_mac
    size: 6
    doc: 'Source MAC Adress like regular ehternet frame.'
  - id: ethertype_1
    type: u2
    doc: 'Ethertype like in regular ethernet frame. Either vlan marker or length of packet.'
  - id: vlan_tag
    type: vlan
    if: ethertype_1 == 0x0081
    doc: 'Vlan tag like in regular ethernet frame.'
  - id: ethertype_2
    type: u2
    if: ethertype_1 == 0x0081
    doc: 'Ethertype is always length of packet, like in LLC traffic.'
  - id: audio_data
    type: sample_raw
    repeat: expr
    repeat-expr: 65
    doc: 'Encoded audio data for 64+1 channels. First channel is sync signal.'
  - id: network_data
    type: network_frame
    size: 26
    doc: 'Network frames tunneled through ace for control and networking.'
  - id: trailer
    type: u1
    repeat: eos
    doc: 'Standard ethernet crc32 trailer using only the first two bytes. May be omitted sometimes.'
    
types:
  vlan:
    seq:
      - id: priority
        type: b3
      - id: drop_elegible
        type: b1
      - id: vlan_id
        type: b12
  sample:
    params:
      - id: index
        type: u4
    instances:
      target_index:
        value: 'index == 0 ? 0 : (((index - 1) * 8) % 63) + 1'
        doc: 'Converted index due to complex ordering over network.'
      b0:
        value: _parent.audio_data[target_index].b0
      b1:
        value: _parent.audio_data[target_index].b1
      b2:
        value: _parent.audio_data[target_index].b2
      r3:
        value: b0 | (b1 << 8) | (b2 << 16)
        
      value:
        value: ((r3 & 0xf0f0f0) >> 4) | ((r3 & 0x0f0f0f) << 4)
        doc: 'Actual sample value as 24bit signed.'
  sample_raw:
    seq:
     - id: data
       size: 3
    instances:
      b0:
        value: data[0]
      b1:
        value: data[1]
      b2:
        value: data[2]
  network_frame:
    seq:
      - id: len_raw
        type: u1
      - id: data
        size: len
    instances:
      len:
        value: 'len_raw == 0 ? 0 : ((len_raw & 0xf0) >> 4) | ((len_raw & 0x03) << 4)'
        doc: 'Length of the valid tunneled network data.'
      is_start:
        value: (len_raw & 0b0100) > 0
        doc: 'True if the data is the start of a network frame.'
      is_continue:
        value: (len_raw & 0b1000) > 0
        doc: 'True if the data is part of the currently transmitted frame and sould be appended to the last message.'
      
instances:
  length:
    value: 'ethertype_1 == 0x0081 ? ethertype_2 : ethertype_1'
    doc: 'Length of this packet.'
  samples:
    type: sample(_index)
    repeat: expr
    repeat-expr: 64
    