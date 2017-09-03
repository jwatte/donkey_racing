Node Communication Protocol
===========================

This is the authoritative description of the protocol between 
CPU and MCU (or, more generically, between collaborating nodes.)

There are three layers to the protocol:

    - Framing level
      How do I send and receive PDUs over various communication 
      channels. Serial, UDP, I2C, ...

    - Encoding level
      How do I know what it is I send/receive (bytes vs floats 
      vs strings, commands vs data)

    - Semantic level
      What does it all mean? Can I find out what it means in some 
      kind of introspection?

Terminology
-----------

The device on each end of a communication channel is called a 
"node." The individually controllable properties of "nodes" are 
called "properties." Collections of properties within nodes are 
arranged into "endpoints" which form a logical tree.


Channels
========

A "channel" is a communication method between two nodes. There 
is typically only one channel being used between each pair of 
nodes.

For channels that are not reliable, or that do not separate 
between the end of one PDU and the beginning of another PDU, 
the framing is wrapped in a header and a trailer.

The following protocols use this wrapper:
    - Serial (is not 100% reliable, doesn't separate PDUs)
    - TCP (does not separate PDUs)
    - UDP (is not 100% reliable)
    - USB (USB is electrically not 100% reliable under vibration)

The following protocols do not use the framing wrapper:
    - I2C (each transaction is easily identifiable, and implementation
      is intended to be 100% reliable in hardware)
    - SPI (each transaction is easily identifiable, and implementation
      is intended to be 100% reliable in hardware)

For the protocols that don't use the wrapper, the only data in the 
channel is the packet_payload.

    Wrapper = start_of_header packet_length header_data packet_payload trailer_data

    start_of_header = 0xAA 0x55      ; Easy to scan for in payload for
                                       potential start of packet; also 
                                       screens out random line noise on 
                                       serial ports.

    packet_length   = len16_l8 len16_h8
                                     ; The world is little-endian; no 
                                       PDU is bigger than 64kB.

    header_data     = your_last_u8 my_current_u8
                                     ; Indication of link quality; what 
                                       were the packet ids last received?

    packet_payload  = <bytes> (packet_length - sizeof(header_data) - sizeof(trailer_data))

    trailer_data    = crc16_l8 crc16_h8
                                     ; Little-endian "CCITT-16" of 
                                       packet_length, header_data, packet_payload


Payload data
============

For all channels, the packet payload contains a number of key/value
tuples. Even if the semantic meaning of a particular key is not known 
by the other end, the physical encoding of all data is well known, so 
the size of the key/value can be skipped to the next one in packet.

The data model within the "other end" is a recursive tree, where each 
endpoint can have more endpoints. An endpoint can have up to 128 data 
values and up to 128 sub-endpoints. The meaning of endpoints are 
determined by convention and schema. One such schema is shown further
down.

Requests
--------

Each payload PDU contains zero or more requests. A request consists of 
a request ID, a request identifier, and perhaps node address and 
data values based on the specific request. The format is:
`request_byte` `request_id` `<optional-address>` `<optional-data>`
A request that contains an address has the 0x80 bit set.
A request that contains data has the 0x40 bit set.
A `request_id` of 0 means the sender does not care about ACK/NAK.

Requests are:

    - DESCRIBE Address      ; Tell remote end to send description of 
                              the given address (response is DESCRIPTION.)
                              0x81
    - NAK Id (u8)           ; Tell the remote end that a given request 
                              was not completed
                              0x42
    - ACK Id (u8)           ; Tell the remote end that a given request 
                              was completed
                              0x43
    - SUBSCRIBE Address Frequency (u16)
                            ; Subscribe to data updates for the address
                              at the approximate frequency (ms)
                              0xC4
    - STOP Address          ; Stop a subscription
                              0x85
    - READDATA Address      ; Request data once for a given address
                              0x86
    - WRITEDATA Address Data
                            ; Write data to an address
                              0xC7
    - DESCRIPTION Address Struct
                            ; Description of an endpoint or a property
                              0xC8
    - ERROR Struct          ; Some bad error has occurred, and any state 
                              that has been established in the protocol 
                              should no longer be relied upon.
                              0x49
    - NOTE String           ; Something of note to perhaps log / tell user
                              0x4A

    - REQUEST_ADDR          ; Set this bit if the request contains an 
                              address (already included in the above values.)
                              0x80
    - REQUEST_DATA          ; Set this bit if the request contains a data 
                              element (already included in the above values.)
                              0x40
    - REQUEST_ID            ; Set this bit if the request byte is followed
                              by a request ID, which implies a desire for 
                              NAK/ACK response to this request.
                              0x20

Note that, on the wire, each request code is followed immediately by a 
request id if the request has the REQUEST_ID bit set, and then the indicated
address or data item follows.

To correctly skip past a request in a buffer, even if that request is of some 
request code that the node doesn't understand, the logic looks like this:

    unsigned char *skip_request(unsigned char *ptr) {
        unsigned char rcode = *ptr++;
        if (rcode & 0x20) {
            ++ptr;  /* skip the request id */
        }
        if (rcode & 0x80) {
            do {
              ++ptr;
            } while ((ptr[-1] != 0xff) && (ptr[-1] & 0x80)); /* skip an address */
        }
        if (rcode & 0x40) {
            ptr = skip_typed_data(ptr);
        }
        return ptr;
    }

Address
-------

An address for an endpoint is a sequence of bytes, where the high bit
is set to address a sub-endpoint, and clear to address a property 
(which terminates the address.) For example, an address might be the 
byte 0x03, which would address property 3 of the top level node. 
Another address might be 0x86 0x92 0x7f, which would address property
127 of sub-endpoint 18 of sub-endpoint 6. There is in theory no limit 
to how far the endpoint tree can go, but by convention, software does 
not expect endpoint recursion to be more than three, followed by a 
property identifier, for a total maximum address size of 4 bytes.

Where addresses are used in the control protocol above, they are not 
prefixed by a type id. Where addresses are exposed as properties (if 
at all,) they are prefixed by a type id.

The special "sub-endpoint" 0xFF refers to the endpoint itself, and 
terminates the address. This means an endpoint can have 128 
sub-properties and 127 sub-endpoints. This also means that the address
consisting of a single byte 0xFF references the "root endpoint" of the 
node itself.


Data Types
==========

The data type of a property is provided when data for that property 
is sent/received. The data type can also be queried through the 
introspection mechanism. Data types supported include:

Atomic Types
------------

    - the null value (0x0)
    - strings of maximum length 255 bytes, utf-8 encoding, prefixed by
      length byte (0x1)
    - binary blocks of data of maximum length 255, prefixed by length
      byte (0x2)
    - binary blocks of data of maximum length 65535, prefixed by 
      two length bytes (little endian) (0x3)
    - unsigned byte (0-255) (0x4)
    - signed byte (-128 - 127) (0x5)
    - unsigned short (0 - 65535) sent little-endian (0x6)
    - signed short (-32768 - 32767) sent little-endian (0x7)
    - unsigned int (4 bytes, little endian) (0x8)
    - signed int (4 bytes, little-endian) (0x9)
    - unsigned long (8 bytes, little-endian) (0xA)
    - signed long (8 bytes, little-endian) (0xB)
    - float32 (4 bytes, little-endian) (0xC)
    - float64 (8 bytes, little-endian) (0xD)
    - address (N bytes, the last of which is 0xFF or has bit 0x80 clear) (0xE)

Aggregate Types
---------------

    - Single value (0x0)
    - 2-tuples of each of the above (0x10)      ; 2D vector
    - 3-tuples of each of the above (0x20)      ; 3D vector
    - 4-tuples of each of the above (0x30)      ; quaternion
    - 6-tuples of each of the above (0x40)      ; 2x3 matrix
    - 8-tuples of each of the above (0x50)      ; 2x4 matrix
    - 9-tuples of each of the above (0x60)      ; 3x3 matrix
    - 12-tuples of each of the above (0x70)     ; 3x4 matrix
    - 16-tuples of each of the above (0x80)     ; 4x4 matrix
    - arrays of maximum 255 elements of each of the above (0x90)
    - arrays of maximum 65535 elements of each of the above (0xA0)
    - A structure with up to 255 members (0xFF)

The data type prefixes each property value sent over the wire. This 
is potentially redundant compared to a protocol where the node has to 
first introspect the endpoints it wants to work with, but it is much 
more robust and easier to work with in practice, and thus worth the 
small increase in packet size.

Aggregate tuples do not re-encode the type for each instance. For 
eaxmple, a 4-tuple of the float32 data type would have the type code 
0x3C, and would be followed by four four-byte values, for a total 
encoded size of 17 bytes. (Everything is little-endian.)

Arrays encode the number of elements, but do not re-encode the type 
of each element of the array. For example, an array of signed bytes
of length 48 would be encoded as 0x95 0x30 `48 bytes go here` with 
a total encoded size of 50 bytes. However, an array of string, or 
array of binary, will re-encode the length of each sub-string/binary, 
because they may be of different length.

For example, an array of signed shorts with max length <= 255, and
currently containing nothing (empty) would be sent as: `0x97` `0x00`

As another example, an array with max length <= 255 of strings,
containing the two strings `"hello" and `"world!"`, would be sent as:
`0x91` `0x02` `0x05` `hello` `0x06` `world!`

Finally, a structure containing a 3-tuple of float32 and a 4-tuple of 
float32, would be encoded as:
`0xFF` `0x02` `0x2C` `<X>` `<Y>` `<Z>` `0x3C` `<X>` `<Y>` `<Z>` `<W>` 
where `<X>` and friends are 4-byte `float` values in little-endian 
format.


The null type is a special case; it is always encoded as `0x00` and
there are no aggregates of null (this wouldn't make sense!) Whether a 
particular property is nullable or not is not knowable other than by 
convention. Most properties are not nullable, and empty arrays or 
empty strings should be represented as zero-element items, not as 
nulls.

The "struct" type is also a special case; it is always encoded as 0xff 
and is followed by a byte of the number of fields in the struct. Each 
field is then encoded as normal.

Typeids do not include the length or contents of values. Typeids are 
returned from introspection calls (DESCRIBE.) Introspecting a struct
will not tell you how many fields are in the struct, nor what their
types are.  Introspecting an array will not tell you how many elements
are in the array. (There is a separate field in the introspection
return struct that actually does tell you, but that's not part of the
type code.)


Data type encoding
------------------

Each data type is described by a type ID, which fits in a byte. The low 4 bits of
the byte describe the atomic type. The high 4 bits describe aggregation, if any.
Aggregate types with variable length are followed by the length, followed by that
many instances.  Struct types are followed by a count of fields, followed by that
many typed values.

Endpoint and Property IDs
-------------------------

There is no fixed property ID such that "property 3 is always the name of 
something." The only rule is that property and endpoint IDs are sequential, there 
is no gap. Thus, if you know that an endpoint has 8 properties, you know that they 
are numbered 0 through 7. (A property may have the null type/value, and an endpoint 
may have zero properties or sub-endpoints.)


Endpoint and Property Introspection
-----------------------------------

Starting with the node (which can be thought of as an implicit endpoint,) the 
tree of endpoints and properties can be introspected by the node on the other side
by sending DESCRIBE request packets, and receiving DESCRIPTION responses.

The description for an endpoint is a structure with the following fields:

    - name (string)                 ; user-visible brief name
    - semantic (byte)               ; known meaning (see table)
    - num properties (byte)         ; how many child properties
    - num child endpoints (byte)    ; how many child endpoints


The description for a property is a structure with the following fields:

    - name (string)                 ; user-visible brief name
    - semantic (byte)               ; known meaning (see table)
    - unit (string)                 ; user-visible unit (see table)
    - type (byte)                   ; type accepted for value
    - maxcount (uint16)             ; for arrays, strings, and structs, else 0
    - readwrite (byte)              ; whether it can be read/written/subscribed
    - frequency (uint16)            ; how often it is expected to update (in ms)

The structures may be expanded in the future; because of the way structures are 
encoded, such expansion will be backwards compatible.

`readwrite` contains a bit mask of which read and write parameters make sense

    - PROPERTY_READ 0x01            ; you can read a regular value
    - PROPERTY_WRITE 0x02           ; you can write a regular value
    - PROPERTY_SUBSCRIBE 0x4        ; you can subscribe to updates for changing values
    - PROPERTY_EPHEMERAL 0x8        ; after writing, it may change
    - PROPERTY_READ_IS_PACKET 0x10  ; used to expose the receive of serial ports etc
    - PROPERTY_WRITE_IS_PACKET 0x20 ; used to expose the send of serial ports etc

Names are intended to be used in configuration files, and thus should be short but 
descriptive, preferrably in the form of valid C identifiers (ASCII character 
followed by ASCII characters, digits 0-9, and underscores.) Examples of good 
identifiers might be `PowerOn` or `roboclaw_81` or `steeringpin3". Configuration 
will then attempt to parse compound names like "PWM.motorControl.neutral_value" 
into endpoint and parameter names (hence, don't use spaces, periods, slashes, or 
other punctuation in names.)


Semantics
=========

In the node introspection, there are semantics for endpoints and properties. 
These can be thought of as "protocols" of a sort -- a property or endpoint that 
implements a given known semantic, will be expected to conform to some specific 
set of rules specific to that semantic value.

For example, the "BATTER_VOLTAGE" semantic is expected to report the voltage of 
a battery in 1/1000th of a volt. Which particular battery among many batteries 
this is is undefined, but the name ought to be a clue. What constitutes good 
high or low values for this voltage is undefined.

Units
-----

The unit is, where possible, well matched to the SI units. The following units 
are used where possible:

    - m/s           ; speed
    - mm/s          ; speed
    - m/s/s         ; acceleration
    - mm/s/s        ; acceleration
    - s             ; duration
    - ms            ; duration
    - us            ; duration (PWM width)
    - C             ; temperature
    - mC            ; temperature
    - rad           ; angle (absolute)
    - mrad          ; angle (absolute)
    - rad/s         ; spin
    - mrad/s        ; spin
    - cnt           ; counts (absolute)
    - cnt/s         ; rate
    - A             ; current
    - mA            ; current
    - V             ; voltage
    - mV            ; voltage
    - deg           ; degrees (use only for GPS)
    - %             ; fraction
    - N             ; force
    - mN            ; force
    - Nm            ; torque
    - mNm           ; torque
    - W             ; power
    - Pa            ; pressure (N/m2)

Note that the string for unit may be parsed by nodes, and thus using these specific 
units adds value. Using the ASCII variants rather than perhaps better Unicode 
specific values makes semantic interpretation by nodes easier, because there may 
be many valid Unicode encodings of some particular unit glyph.

Orientations and coordinates
============================

For some properties, there is a "chassis origin." This may be the center of gravity 
of the chassis, or it may be the center of the bounding box of the chassis, or it 
may be the center of the ground contacts of the chassis resting on the ground. For 
the cases where it's not center-of-gravity, the chassis should expose a 
CENTER_OF_GRAVITY semantic property that estimates the COG of the chassis at rest, 
in chassis coordinates.

Points are expressed preferentially as three-tuples of float32, which represent the
X, Y and Z coordinates in order.

The coordinate system of the chassis is X forward, Y right, Z down. This is a right 
handed coordinate system. Rotations for "heading" are around the downwards-facing 
Z axis, which means that rotations match the direction of a conventional Compass -- 
pi/2 rotation from front means pointing to the right. A compass heading is expressed 
as a single float32, assumed to be around the downwards-facing Z axis.

Rotations are expressed preferentially as four-tuples of float32, which represent a 
quatention in X, Y, Z, W order. (Note: about 45% of material on quaternions use the 
distinct W, X, Y, Z order -- make sure to get it right!) The representation of a 
nine-tuple of float32, representing a rotation matrix, is also acceptable where 
necessary. Euler angles are advised against, because the order of rotation needs to 
be specified, and the amount of "actual" rotation is non-linear, and the 
representation singularities of "Gimbal lock" make them harder to reason correctly
about.

Endpoint Semantics
==================

Specific endpoints may expose certain well-known protocols. These are assigned a 
specific well-defined "semantic" identifier, which a node can use to make 
assumptions about how the endpoint will behave. The semantic 0 means "no specific 
semantic."

    - MISC                      ; No well-defined protocol exists for this endpoint
    - SERIAL                    ; This endpoint represents a serial port I/O
    - LOCOMOTION_CONTROL        ; This endpoint represents desired locomotion control
    - LOCOMOTION_ENCODERS       ; This endpoint represents encoders reading locomotion
    - POWER_CONTROL             ; This endpoint represents battery/power management
    - THERMAL_CONTROL           ; This endpoint represents temperature management
    - IMU                       ; This endpoint represents an IMU sensor
    - GPS                       ; This endpoint represents an IMU sensor
    - USER_INTERFACE            ; This endpoint represents a user interface
                                  (buttons, display)

A particular endpoint semantic will define what, in turn, sub-endpoints/parameter 
semantics mean. Parameter semantics so defined will have their 0x80 bit set, to 
separate them from regular "unattached" parameter semantics. It's important to note 
that a parameter of semantic 0x82 under a LOCOMOTION_CONTROL endpoint will have a 
totally different meaning than a parameter of semantic 0x82 under a THERNAL_CONTROL 
endpoint. Meanwhile, all parameters of semantic 0x02 share the same meaning. The 
high-bit-set semantic values cannot be used under an endpoint with a MISC semantic.

Names are recommended for certain endpoint semantics and certain property semantics. 
Those names are recommended to be adhered to for user friendliness, but software 
should be using the semantic id, not the name, to find what it's looking for, unless 
the name is explicitly configured by the user.

MISC Endpoint
-------------

The MISC endpoint has semantic ID 0, and does not define endpoint-specific semantics.
The semantics defined for MISC are potentially available everywhere, if it makes 
sense for some particular endpoint to expose them.

There is no preferred name for the MISC endpoint type.

Any endpoint semantic can exist under a MISC endpoint, although none of the specific
sub-endpoints would exist directly under MISC (they are only valid under the endpoint 
semantic type that defines them.)

The property semantics defined under MISC (and any other endpoint) include:

    - MISC_VALUE    - any type - any name - any value
    - MISC_DISABLE  - type uint8 - name "disable" - value 0 for an available endpoint, value 0xff for 
                    an endpoint that is excluded because of other configurations, 
                    value 1 for an endpoint that's manually disabled.
    - MISC_NOTE     - type string - name "note" - a comment/documentation about the endpoint
    - MISC_VOLTAGE  - type float32/uint8 (V) or uint16/uint32 (mV), name "voltage"
    - MISC_CURRENT  - type float32/uint8 (A) or uint16/uint32 (mA), name "current"
    - MISC_POWER    - type float32/uint8 (W) or uint16/uint32 (mW), name "power"
    - MISC_TEMPERATURE  - type float32/uint8 (C) or uint32 (mC), name "temp"
    - MISC_PRESSURE - type float32/uint16/uint32 (Pa) or uint8 (kPa), name "pressure"
    - MISC_ANGLE    - type float32 (Deg or rad), uint16 or uint32 (mDeg or mRad), name "angle"
    - MISC_POSITION - type 3-tuple of float32 (m), name "pos"
    - MISC_ERROR    - type (some integer), no unit, name "error", if non-0, an error
    - MISC_WARNING  - type (some integer), no unit, name "warning", if non-0, warnings, 
                      if unsigned, is a bit field

SERIAL Endpoint
---------------

The SERIAL endpoint semantic defines a serial interface, typically a UART but perhaps 
other serial protocols can also be exposed as SERIAL.

The name of the SERIAL endpoint is a device name on Linux (`ttyACM0` or similar) and 
a Wiring serial class name on Arduino-derived platforms (`Serial1`, `Serial2` and so 
forth) and a COM port name on Windows (e g `COM3`).

A SERIAL endpoint must define the SERIAL_DATA semantic property. It may also define 
other properties, as follows:

    - SERIAL_DATA   - Type binary8, name "data", read-packet, write-packet, subscribable.
                    This property is the input/output of the serial port. Serial 
                    ports that only support reading, or writing, only expose the 
                    appropriate packet bits.
    - SERIAL_BAUD   - Type uint32, name "baud", the bits-per-second rate of the interface.
    - SERIAL_BITS   - Type uint8, name "bits", the number of bits-per-word (typically 8 or perhaps 7)
    - SERIAL_STOP   - Type uint8, name "stop", number of stop bits (typically 1 or 2)
    - SERIAL_PARITY - Type uint8, name "parity", parity for the port, 0: none, 1: odd,
                    2: even, 3: low, 4: high
    - SERIAL_FLOW   - Type uint8, name "flow", Flow control on the port, 0: none, 1: CTS in,
                    2: XON/XOFF in, 4: RTS out, 8: XON/XOFF out, 64: single duplex

LOCOMOTION_CONTROL Endpoint
---------------------------

The LOCOMOTION_CONTROL endpoint controls a particular motor or other movement mechanism.
The name is specific to the control type. "pwmesc5" can control a RC-style PWM-controlled 
Electronic Speed Controller on pin 5, "roboclaw80" can control a RoboClaw unit with ID 0x80 
on a serial bus, "dynamixel18" can control a Dynamixel servo on a (shared) RS-485 bus, 
"pwmservo12" controls a PWM RC servo on pin 12, and so on. (These names are suggested, not 
required.)

A LOCOMOTION_CONTROL endpoint must define the "speed" property or the "position" property.
It can optionally define other properties.

    - LOCOMOTION_CONTROL_SPEED  - Type float32 or float64 (m/s) or type int16 or int32 (mm/s or
                        cnt/s)
                        name "speed", writable. The target speed of the controlled node.
    - LOCOMOTION_CONTROL_PRESENT_SPEED - Type float32 or float64 or int16 or int32 (in the 
                        same meanings as for `_SPEED`). If there is a current reading on how 
                        fast the controlled system is going, it can go here. Recommended name 
                        is "p_speed".
    - LOCOMOTION_CONTROL_POSITION   - Type float32 or float64 (m, rad) or type int16 or int32
                        (mm, mrad) or type int16/int32/int64 (cnt). 0 is generally center for 
                        whatever that means. Name "pos", writable. The target position of the 
                        controlled node.
    - LOCOMOTION_CONTROL_PRESENT_POSITION   - Same types as `_POSITION`, returns an actual 
                        position if separately known from the desired target. Name "p_position"
    - LOCOMOTION_POWER  - name "power", type float32, float64, uin8, uint16 or uint32 of "W".
                        This may be readable to tell how strong an actuator is, or it may be 
                        writable to tell the system how much power to apply. Attempting to write 
                        this above the maximum limit may silently truncate to the limit.
    - LOCOMOTION_PRESENT_POWER  - name "effort", same types and meanings as `_POWER`
    - LOCOMOTION_MAX_POWER  - Name "max_power", read-only version of `_POWER` that documents the 
                        maximum configurable limit (if it is configurable.)
    - LOCOMOTION_CHASSISPOS - Name "c_pos", type 3-tuple of float32, unit "m", location of the 
                        actuator relative to the chassis origin.

If a single locomotion controller has more than one channel, the LOCOMOTION_CONTROL endpoint 
will have LOCOMOTION_CONTROL sub-endpoints, with names based on the channels, and a semantic 
taken from the following list of semantics:

    - 0x80 -- unspecified (use ordering)
    - 0x81 .. 0xef -- indexed by ID (mask off the high bit)
    - 0xf0 -- left
    - 0xf1 -- right
    - 0xf2 -- frontleft
    - 0xf3 -- frontright
    - 0xf4 -- rearleft
    - 0xf5 -- rearright
    - 0xf6 -- frontcenter
    - 0xf7 -- rearcenter
    - 0xff -- centered

Note that the "semantic" is the 0x80 .. 0xff value, not the `LOCOMOTION_CONTROL` value, but the 
protocol exposed by these sub-endpoints is the same as `LOCOMOTION_CONTROL`.

LOCOMOTION_ENCODERS Endpoint
----------------------------

An Encoders endpoint may be a sub-endpoint of a LOCOMOTION_CONTROL endpoint, in which case 
the encoder is directly related to the control of that motor, or it may be a freestanding value, 
in which case perhaps it's not directly related to another controller.

If there are multiple encoders for a given locomotion controller, those are all sub-endpoints of 
the same locomotion controller. For example, four wheel encoders on a car with a single throttle 
speed controller, would have a top locomotion controller for the speed control, and that top 
controller endpoint would have four locomotion_encoders endpoint children, one for each wheel.

The only required property is `LOCATION_ENCODER_POSITION` but the other properties are often 
implemented to make the encoder value more useful.

    - LOCOMOTION_ENCODER_POSITION   - Name "position", type float32 or float64 (m), or type
                        sint8 or sint16 or sint32 or sint64 (mm or cnt).
    - LOCOMOTION_ENCODER_SPEED  - Name "speed", type float32 or float64 (m/s), or type sint8 
                        or sint16 or sint32 or sint64 (mm/s or cnt/s).
    - LOCOMOTION_ENCODER_PID    - Name "pid", type 3-tuple of float32 or float64 or int16 or
                        int32 or int64, where the integer values are fixed-point values in N.N
                        format (8.8, 16.16, 32.32) The values are unitless. The order is P,I,D.
                        The order in your physical controller may be different.
    - LOCOMOTION_ENCODER_RESOLUTION - Name "res", any numeric type, "cnt", number of ticks per
                        revolution. If the type is "rad" or "mrad," value is angular displacement
                        per tick.
    - LOCOMOTION_CHASSISPOS - Name "pos", type 3-tuple of float32, unit "m", location of the 
                        encoder relative to the chassis origin.
    - LOCOMOTION_CHASSISDIR - Name "dir", type 3-tuple of float32, unit "m", direction of the 
                        forward movement of the encoder in chassis space.

POWER_CONTROL Endpoint
----------------------

THERMAL_CONTROL Endpoint
------------------------

IMU Endpoint
------------

GPS Endpoint
------------

USER_INTERFACE Endpoint
-----------------------


