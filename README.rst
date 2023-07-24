..
   Copyright (c) 2023 Golioth, Inc.
   SPDX-License-Identifier: Apache-2.0

CAN Asset Tracker Reference Design
##################################

Overview
********

The CAN Asset Tracker Reference Design connects to the OBD-II diagnostic port
in a vehicle and continuously records vehicle sensor values using the ISO 15765
(CAN bus) protocol.

Specifically, the following OBD-II vehicle sensor "PIDs" are supported:

* ``0x0D``: Vehicle Speed Sensor (VSS)

The vehicle sensor values are combined with GPS location/time data and uploaded
to the Golioth Cloud. The timestamp from the GPS reading is used as the
timestamp for data record in the Golioth LightDB Stream database.

GPS readings can be received as frequently as once-per-second. When the device
is out of cellular range, the reference design firmware caches data locally and
uploads it later when connection to the cellular network is restored.

Local set up
************

Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: console

   cd ~
   mkdir golioth-reference-design-can-asset-tracker
   python -m venv golioth-reference-design-can-asset-tracker/.venv
   source golioth-reference-design-can-asset-tracker/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: console

   cd ~/golioth-reference-design-can-aset-tracker
   west init -m git@github.com:golioth/reference-design-can-asset-tracker.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Building the application
************************

Build Zephyr sample application for Golioth Aludel-Mini
(``aludel_mini_v1_sparkfun9160_ns``) from the top level of your project. After a
successful build you will see a new ``build`` directory. Note that any changes
(and git commmits) to the project itself will be inside the ``app`` folder. The
``build`` and ``deps`` directories being one level higher prevents the repo from
cataloging all of the changes to the dependencies and the build (so no
``.gitignore`` is needed)

During building, replace ``<your.semantic.version>`` to utilize the DFU
functionality on this Reference Design.

.. code-block:: console

   $ (.venv) west build -p -b aludel_mini_v1_sparkfun9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: console

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Golioth Features
****************

This app currently implements Over-the-Air (OTA) firmware updates, Settings
Service, Logging, RPC, and both LightDB State and LightDB Stream data.

Settings Service
================

The following settings should be set in the Device Settings menu of the
`Golioth Console`_.

``LOOP_DELAY_S``
   Adjusts the delay between sensor readings. Set to an integer value (seconds).

   Default value is ``5`` seconds.

``GPS_DELAY_S``
   Adjusts the delay between recording GPS readings. Set to an integer value
   (seconds).

   Default value is ``3`` seconds.

``VEHICLE_SPEED_DELAY_S``
   Adjusts the delay between vehicle speed readings. Set to an integer value
   (seconds).

   Default value is ``1`` second.

Remote Procedure Call (RPC) Service
===================================

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

LightDB State and LightDB Stream data
=====================================

Time-Series Data (LightDB Stream)
---------------------------------

Vehicle data is periodicaly sent to the following endpoints of the LightDB
Stream service:

* ``gps/lat``: Latitude (°)
* ``gps/lon``: Longitude (°)
* ``vehicle/speed``: Vehicle Speed (km/h)

Battery voltage and level readings are periodically sent to the following
endpoints:

* ``battery/batt_v``: Battery Voltage (V)
* ``battery/batt_lvl``: Battery Level (%)

Stateful Data (LightDB State)
-----------------------------

In the case where a GPS signal can not be received, a fake latitude & longitude
can be set for the device in LightDB State. Setting the ``desired`` values in
the Golioth Console will update the actual ``state`` values on the device.

``fake_gps_enabled``
   Set to a boolean value (``true`` or ``false``) to enable or disable the
   fake GPS functionality.

   Default value is ``false``.

``fake_latitude``
   Set to a string latitude value (``"-90.0"`` to ``"90.0"``).

   Default value is ``"37.789980"``.

``fake_longitude``
   Set to a string longitude value (``"-180.0"`` to ``"180.0"``).

   Default value is ``"-122.400860"``.

Further Information in Header Files
===================================

Please refer to the comments in each header file for a service-by-service
explanation of this template.

Hardware Variations
*******************

Nordic nRF9160 DK
=================

This reference design may be built for the `Nordic nRF9160 DK`_, with the
`MikroE Arduino UNO click shield`_ to interface the two click boards.

* Position the `MikroE CAN SPI Click 3.3V`_ in Slot 1
* Position the `MikroE GNSS 7 Click`_ in Slot 2

The click boards must be in this order for the GPS UART to work.

Use the following commands to build and program. (Use the same console commands
from above to provision this board after programming the firmware.)

.. code-block:: console

   $ (.venv) west build -p -b nrf9160dk_nrf9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

External Libraries
******************

The following code libraries are installed by default. If you are not using the
custom hardware to which they apply, you can safely remove these repositories
from ``west.yml`` and remove the includes/function calls from the C code.

* `golioth-zephyr-boards`_ includes the board definitions for the Golioth
  Aludel-Mini
* `libostentus`_ is a helper library for controlling the Ostentus ePaper
  faceplate

Using this template to start a new project
******************************************

This reference design was forked from the `Reference Design Template`_ repo. We
recommend the following workflow to pull in future changes:

* Setup

  * Create a ``template`` remote based on the Reference Design Template repository

* Merge in template changes

  * Fetch template changes and tags
  * Merge template release tag into your ``main`` (or other branch)
  * Resolve merge conflicts (if any) and commit to your repository

.. code-block:: console

   # Setup
   git remote add template https://github.com/golioth/reference-design-template.git
   git fetch template --tags

   # Merge in template changes
   git fetch template --tags
   git checkout your_local_branch
   git merge template_v1.0.0

   # Resolve merge conflicts if necessry
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
