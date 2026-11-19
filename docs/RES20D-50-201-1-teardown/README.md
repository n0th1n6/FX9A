# RES20D-50-201-1 Optical Encoder Teardown

## Purpose

This directory documents the teardown and attempted repair of the original Nidec / Copal `RES20D-50-201-1` optical rotary encoder used in the FX-9A.

The replacement encoder is being installed first. Once the replacement is confirmed working, the original encoder will be opened, documented, repaired if possible, and kept as a spare.

## Encoder identification

The original encoder is marked:

`RES20D50-201-1`

This corresponds to the catalog part:

`RES20D-50-201-1`

Known characteristics:

- Optical incremental encoder
- 2-channel A/B quadrature output
- 50 PPR
- 5 V operation
- 6 mm shaft diameter
- 9 mm shaft projection
- Approximately 20 mm body diameter
- No mechanical detents

## What happened

During removal, the shaft was pulled out because the encoder was initially assumed to be assembled like a conventional mechanical rotary encoder.

The shaft separated from the optical-disc hub.

Inspection showed that:

- The shaft is retained by a splined press-fit into the disc hub.
- The splines can be aligned with the original spline impressions in the hub.
- The shaft has a flange / shoulder.
- There is axial slack between the rear cover and the optical-disc hub.
- When the shaft is pushed back in from the front, the splines engage correctly at first.
- However, the hub moves rearward through the available axial clearance before the shaft can fully seat.
- The flange then prevents further insertion.
- Because of this, the shaft only goes in partway even when the splines are correctly aligned.

The problem therefore appears to be mechanical rather than electronic.

## Planned repair

After the replacement encoder is installed and verified:

1. Carefully remove the rear plastic cover from the original encoder.
2. Photograph the internal arrangement before disturbing any parts.
3. Inspect the optical disc, central hub, shaft interface, sensor PCB, and rear-side clearance.
4. Support the central hub directly from behind.
5. Align the shaft splines with their original tracks.
6. Press the shaft straight back into the hub while the hub is supported.
7. Do not allow the pressing force to pass through the thin optical disc.
8. Reassemble and check for free rotation and correct axial position.
9. Avoid adhesive unless the press-fit has become too loose.
10. If adhesive is eventually needed, keep it strictly on the shaft/hub interface and away from the optical disc, sensor, and bearing surfaces.

## Electrical verification after repair

After reassembly:

- Power the encoder from 5 V.
- Observe channels A and B with an oscilloscope.
- Confirm clean square-wave outputs.
- Confirm approximately 90-degree phase offset between A and B.
- Confirm 50 pulses per revolution per channel.
- Rotate slowly through a full revolution and check for missing pulses or bad sectors.

## Teardown photo plan

The teardown should be documented step by step.

Suggested sequence:

1. Complete encoder and label
2. Rear cover before removal
3. Rear cover removed
4. PCB / emitter / detector arrangement
5. Optical disc in original installed position
6. Disc hub and shaft interface
7. Shaft splines and flange
8. Rear side of the hub showing axial clearance
9. Individual parts laid out in assembly order
10. Reassembled encoder
11. Oscilloscope capture of A/B quadrature after repair

Suggested filenames:

- `res20d-exterior.jpg`
- `res20d-rear-cover.jpg`
- `res20d-rear-cover-removed.jpg`
- `res20d-optical-disc.jpg`
- `res20d-shaft-hub.jpg`
- `res20d-shaft-splines.jpg`
- `res20d-hub-axial-clearance.jpg`
- `res20d-pcb-sensor.jpg`
- `res20d-parts-layout.jpg`
- `res20d-reassembled.jpg`
- `res20d-ab-quadrature.png`

## Why document it

A search for internal teardown images of the exact `RES20D-50-201-1` did not turn up a convincing public example.

Publishing the teardown here may therefore be useful for:

- FX-9A owners
- repair work on equipment using the same encoder
- understanding the shaft/hub retention method
- documenting the internal optical construction
- preserving repair information for a relatively uncommon encoder

## Current status

- Direct replacement identified: `RES20D-50-201-1`
- Replacement pending installation / verification
- Original encoder reserved for teardown and repair
- Mechanical fault understood: splined press-fit shaft plus hub axial slack prevents full reseating from the front
