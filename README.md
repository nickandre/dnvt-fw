# DNVT Phone Switch

This repo contains the code for the DNVT military phone switch. 

## Release History

### Version 0.30

Initial public release. Includes usb-enabled firmware and host application. New features:

- USB interrupt endpoint for control
- Initial host application release
- USB vendor specific control EP command to initiate firmware upload mode
- Line supervision: lines without connected phone will show `---` on the display under status. The supervision check is run for all lines on boot and then every 10 minutes therafter. The switch initiates a "cue" command to the phone to prompt it to exit the idle state. If the phone replies we go back to idle. If the phone is determined idle, it will enter the "unreachable" state which will send a continuous cue command until receiving a reply. The 10 minute line supervisory check produces an audible click and visible blink and can be disabled by setting DIP switch 1 to the ON position. This will disable the periodic checking of idle phones, though the system will still do the initial check on boot and then mark phones as unreachable if a ring attempt fails.
- Calling your own number in Line Simulator mode produces a "connection failed" repeated beep instead of doing nothing.
- The connection has been refactored to queue to support tx and rx queues for USB mode. RX queue function similarly to rx_data in previous versions.
- Several USB states added.
- process phones switches behavior to USB states based upon usb activity.
- USB mode on display now shows 'n/c' for not enumerated, 'inact' for inactive but enumerated, and 'activ' for actively detecting heartbeats from host application.
- DIP switch code now does things. DIP 1 disables supervisory, DIP 2 now disables line simulator mode.