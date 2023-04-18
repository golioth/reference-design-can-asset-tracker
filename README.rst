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
   pip install wheel west pre-commit

Use ``west`` to initialize and install
======================================

.. code-block:: console

   cd ~/golioth-reference-design-can-asset-tracker
   west init -m git@github.com:golioth/reference-design-can-asset-tracker.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt
   source deps/zephyr/zephyr-env.sh

This will also install the `golioth-zephyr-boards`_ definitions for the Golioth
Aludel-Mini.

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

   $ (.venv) west build -b aludel_mini_v1_sparkfun9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: console

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Golioth Features
****************

This app implements:

* Over-the-Air (OTA) firmware updates
* LightDB Stream for recording periodic GPS and can frame readings to the
  ``gps`` endpoint.
* Settings Service to adjust the delay between recording GPS readings, and the
  delay between sending cached readings to Golioth
* Remote Logging
* Remote Procedure call (RPC) to reboot the device

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

Hardware Variations
*******************

Nordic nRF9160 DK
=================

This reference design may be built for the `Nordic nRF9160 DK`_, with the
`MikroE Arduino UNO click shield`_ to interface the two click boards.

* Position the CAN click in Slot 1
* Position the GNSS 7 click in Slot 2

The click boards must be in this order for the GPS UART to work.

Use the following commands to build and program. (Use the same console commands
from above to provision this board after programming the firmware.)

.. code-block:: console

   $ (.venv) west build -b nrf9160dk_nrf9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

.. _Golioth Console: https://console.golioth.io
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
.. _Nordic nRF9160 DK: https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk
.. _MikroE Arduino UNO click shield: https://www.mikroe.com/arduino-uno-click-shield
