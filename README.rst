..
   Copyright (c) 2022-2023 Golioth, Inc.
   SPDX-License-Identifier: Apache-2.0

OBD-II / CAN Asset Tracker Reference Design
###########################################

This repository contains the firmware source code and `pre-built release
firmware images <releases_>`_ for the Golioth OBD-II / CAN Asset Tracker
reference design.

The full project details are available on the `OBD-II / CAN Asset Tracker
Project Page`_, including follow-along guides for building an OBD-II / CAN Asset
Tracker yourself using widely available off-the-shelf development boards.

We call this **Follow-Along Hardware**, and we think it's one of the quickest
and easiest ways to get started building an IoT proof-of-concept with Golioth.
In the follow-along guides, you will learn how to assemble the hardware, flash a
pre-built firmware image onto the device, and connect to the Golioth cloud in
minutes.

Once you have completed a follow-along guide for one of our supported hardware
platforms, the instructions below will walk you through how to build and
configure the firmware yourself.

Supported Hardware
******************

This firmware can be built for a variety of supported hardware platforms.

.. pull-quote::
   [!IMPORTANT]

   In Zephyr, each of these different hardware variants is given a unique
   "board" identifier, which is used by the build system to generate firmware
   for that variant.

   When building firmware using the instructions below, make sure to use the
   correct Zephyr board identifier that corresponds to your follow-along
   hardware platform.

.. list-table:: **Follow-Along Hardware**
   :header-rows: 1

   * - Hardware
     - Zephyr Board
     - Follow-Along Guide

   * - .. image:: images/can_asset_tracker_fah_nrf9160_dk.jpg
          :width: 240
     - ``nrf9160dk_nrf9160_ns``
     - `nRF9160 DK Follow-Along Guide`_

.. list-table:: **Custom Golioth Hardware**
   :header-rows: 1

   * - Hardware
     - Zephyr Board
     - Project Page
   * - .. image:: images/can_asset_tracker_aludel_mini_v1_photo_top.jpg
          :width: 240
     - ``aludel_mini_v1_sparkfun9160_ns``
     - `OBD-II / CAN Asset Tracker Project Page`_

Firmware Overview
*****************

The OBD-II / CAN Asset Tracker Reference Design connects to the OBD-II
diagnostic port in a vehicle and continuously records vehicle sensor values
using the ISO 15765 (CAN bus) protocol.

Specifically, the following OBD-II vehicle sensor "PIDs" are supported:

* ``0x0D``: Vehicle Speed Sensor (VSS)

The vehicle sensor values are combined with GPS location/time data and uploaded
to the Golioth Cloud. The timestamp from the GPS reading is used as the
timestamp for the data record in the Golioth LightDB Stream database.

GPS readings can be received as frequently as once-per-second. When the device
is out of cellular range, the reference design firmware caches data locally and
uploads it later when connection to the cellular network is restored.

Supported Golioth Zephyr SDK Features
=====================================

This firmware implements the following features from the Golioth Zephyr SDK:

- `Device Settings Service <https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service>`_
- `LightDB State Client <https://docs.golioth.io/firmware/zephyr-device-sdk/light-db/>`_
- `LightDB Stream Client <https://docs.golioth.io/firmware/zephyr-device-sdk/light-db-stream/>`_
- `Logging Client <https://docs.golioth.io/firmware/zephyr-device-sdk/logging/>`_
- `Over-the-Air (OTA) Firmware Upgrade <https://docs.golioth.io/firmware/device-sdk/firmware-upgrade>`_
- `Remote Procedure Call (RPC) <https://docs.golioth.io/firmware/zephyr-device-sdk/remote-procedure-call>`_

Device Settings Service
-----------------------

The following settings should be set in the Device Settings menu of the `Golioth
Console`_.

``LOOP_DELAY_S``
   Adjusts the delay between sensor readings. Set to an integer value (seconds).

   Default value is ``5`` seconds.

``GPS_DELAY_S``
   Adjusts the delay between recording GPS readings. Set to an integer value
   (seconds).

   Default value is ``3`` seconds.

``FAKE_GPS_ENABLED``
   Controls whether fake GPS position data is reported when a real GPS location
   signal is unavailable. Set to a boolean value.

   Default value is ``false``.

``FAKE_GPS_LATITUDE``
   Sets the fake latitude value to be used when fake GPS is enabled. Set to a
   floating point value (``-90.0`` to ``90.0``).

   Default value is ``37.789980``.

``FAKE_GPS_LONGITUDE``
   Sets the fake longitude value to be used when fake GPS is enabled. Set to a
   floating point value (``-180.0`` to ``180.0``).

   Default value is ``-122.400860``.

``VEHICLE_SPEED_DELAY_S``
   Adjusts the delay between vehicle speed readings. Set to an integer value
   (seconds).

   Default value is ``1`` second.

LightDB Stream Service
----------------------

Vehicle data is periodically sent to the following endpoints of the LightDB
Stream service:

* ``gps/lat``: Latitude (°)
* ``gps/lon``: Longitude (°)
* ``gps/fake``: ``true`` if GPS location data is fake, otherwise ``false``
* ``vehicle/speed``: Vehicle Speed (km/h)

Battery voltage and level readings are periodically sent to the following
endpoints:

* ``battery/batt_v``: Battery Voltage (V)
* ``battery/batt_lvl``: Battery Level (%)

LightDB State Service
---------------------

The concept of Digital Twin is demonstrated with the LightDB State
``example_int0`` and ``example_int1`` variables that are members of the
``desired`` and ``state`` endpoints.

* ``desired`` values may be changed from the cloud side. The device will
  recognize these, validate them for [0..65535] bounding, and then reset these
  endpoints to ``-1``

* ``state`` values will be updated by the device whenever a valid value is
  received from the ``desired`` endpoints. The cloud may read the ``state``
  endpoints to determine device status, but only the device should ever write to
  the ``state`` endpoints.

Remote Procedure Call (RPC) Service
-----------------------------------

The following RPCs can be initiated in the Remote Procedure Call menu of the
`Golioth Console`_.

``get_network_info``
   Query and return network information.

``reboot``
   Reboot the system.

``set_log_level``
   Set the log level.

   The method takes a single parameter which can be one of the following integer
   values:

   * ``0``: ``LOG_LEVEL_NONE``
   * ``1``: ``LOG_LEVEL_ERR``
   * ``2``: ``LOG_LEVEL_WRN``
   * ``3``: ``LOG_LEVEL_INF``
   * ``4``: ``LOG_LEVEL_DBG``

Building the firmware
*********************

The firmware build instructions below assume you have already set up a Zephyr
development environment and have some basic familiarity with building firmware
using the Zephyr Real Time Operating System (RTOS).

If you're brand new to building firmware with Zephyr, you will need to follow
the `Zephyr Getting Started Guide`_ to install the Zephyr SDK and related
dependencies.

We also provide free online `Developer Training`_ for Zephyr at:

https://training.golioth.io/docs/zephyr-training

.. pull-quote::
   [!IMPORTANT]

   Do not clone this repo using git. Zephyr's ``west`` meta-tool should be used
   to set up your local workspace.

Create a Python virtual environment (recommended)
=================================================

.. code-block:: shell

   cd ~
   mkdir golioth-reference-design-can-asset-tracker
   python -m venv golioth-reference-design-can-asset-tracker/.venv
   source golioth-reference-design-can-asset-tracker/.venv/bin/activate

Install ``west`` meta-tool
==========================

.. code-block:: shell

   pip install wheel west

Use ``west`` to initialize the workspace and install dependencies
=================================================================

.. code-block:: shell

   cd ~/golioth-reference-design-can-aset-tracker
   west init -m git@github.com:golioth/reference-design-can-asset-tracker.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Build the firmware
==================

Build the Zephyr firmware from the top-level workspace of your project. After a
successful build you will see a new ``build/`` directory.

Note that this git repository was cloned into the ``app`` folder, so any changes
you make to the application itself should be committed inside this repository.
The ``build`` and ``deps`` directories in the root of the workspace are managed
outside of this git repository by the ``west`` meta-tool.

.. pull-quote::
   [!IMPORTANT]

   When running the commands below, make sure to replace the placeholder
   ``<your_zephyr_board_id>`` with the actual Zephyr board from the table above
   that matches your follow-along hardware.

   In addition, replace ``<your.semantic.version>`` with a `SemVer`_-compliant
   version string (e.g. ``1.2.3``) that will be used by the DFU service when
   checking for firmware updates.

.. code-block:: text

   $ (.venv) west build -p -b <your_zephyr_board_id> app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"

For example, to build firmware version ``1.2.3`` for the `Nordic nRF9160 DK`_-based follow-along hardware:

.. code-block:: text

   $ (.venv) west build -p -b nrf9160dk_nrf9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"1.2.3\"

Flash the firmware
==================

.. code-block:: text

   $ (.venv) west flash

Provision the device
====================

In order for the device to securely authenticate with the Golioth Cloud, we need
to provision the device with a pre-shared key (PSK). This key will persist
across reboots and only needs to be set once after the device firmware has been
programmed. In addition, flashing new firmware images with ``west flash`` should
not erase these stored settings unless the entire device flash is erased.

Configure the PSK-ID and PSK using the device UART shell and reboot the device:

.. code-block:: text

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

External Libraries
******************

The following code libraries are installed by default. If you are not using the
custom hardware to which they apply, you can safely remove these repositories
from ``west.yml`` and remove the includes/function calls from the C code.

* `golioth-zephyr-boards`_ includes the board definitions for the Golioth
  Aludel-Mini
* `libostentus`_ is a helper library for controlling the Ostentus ePaper
  faceplate
* `zephyr-network-info`_ is a helper library for querying, formatting, and
  returning network connection information via Zephyr log or Golioth RPC

Pulling in updates from the Reference Design Template
*****************************************************

This reference design was forked from the `Reference Design Template`_ repo. We
recommend the following workflow to pull in future changes:

* Setup

  * Create a ``template`` remote based on the Reference Design Template
    repository

* Merge in template changes

  * Fetch template changes and tags
  * Merge template release tag into your ``main`` (or other branch)
  * Resolve merge conflicts (if any) and commit to your repository

.. code-block:: shell

   # Setup
   git remote add template https://github.com/golioth/reference-design-template.git
   git fetch template --tags

   # Merge in template changes
   git fetch template --tags
   git checkout your_local_branch
   git merge template_v1.0.0

   # Resolve merge conflicts if necessary
   git add resolved_files
   git commit

.. _Golioth Console: https://console.golioth.io
.. _Nordic nRF9160 DK: https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
.. _libostentus: https://github.com/golioth/libostentus
.. _MikroE Arduino UNO click shield: https://www.mikroe.com/arduino-uno-click-shield
.. _MikroE CAN SPI Click 3.3V: https://www.mikroe.com/can-spi-33v-click
.. _MikroE GNSS 7 Click: https://www.mikroe.com/gnss-7-click
.. _Reference Design Template: https://github.com/golioth/reference-design-template
.. _OBD-II / CAN Asset Tracker Project Page: https://projects.golioth.io/reference-designs/can-asset-tracker/
.. _nRF9160 DK Follow-Along Guide: https://projects.golioth.io/reference-designs/can-asset-tracker/guide-nrf9160-dk
.. _releases: https://github.com/golioth/reference-design-can-asset-tracker/releases
.. _Zephyr Getting Started Guide: https://docs.zephyrproject.org/latest/develop/getting_started/
.. _Developer Training: https://training.golioth.io
.. _SemVer: https://semver.org
.. _zephyr-network-info: https://github.com/golioth/zephyr-network-info
