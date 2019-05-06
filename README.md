# Arduino demo for PN7150
Arduino Uno with PN7150 NFC controller. 

NFC examples are not made for Arduino board.

# Used IDE

PlatrofmIO IDE was used for development

# Functions

Connect the PN7150 sheld to Arduino Uno. When tag is in proximity, logs are sent to serial link (115200 baudrate). This demo is only for demontration that the PN7150 is detecting any tag. At current state it cannot read/write data to tag.

# Known Bugs

Logging was causing problems, with compiling, so it was commented and replaced with simple string sending.

# References
Evaluation kit: https://www.nxp.com/products/identification-security/rfid/nfc-hf/nfc-readers/development-kits-for-pn7150-plugn-play-nfc-controller:OM5578

PN7150: https://www.nxp.com/docs/en/data-sheet/PN7150.pdf
