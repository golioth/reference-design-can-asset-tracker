# Golioth ODB-II / CAN Asset Tracker Reference Design

The OBD-II / CAN Asset Tracker Reference Design connects to the OBD-II
diagnostic port in a vehicle and continuously records vehicle sensor
values using the ISO 15765 (CAN bus) protocol.

The Vehicle Speed Sensor (VSS) values are combined with GPS
location/time data and uploaded to the Golioth Cloud. The timestamp from
the GPS reading is used as the timestamp for the data record in the
Golioth LightDB Stream database.

GPS readings can be received as frequently as once-per-second. When the
device is out of cellular range, the reference design firmware caches
data locally and uploads it later when connection to the cellular
network is restored.

Business use cases and hardware build details are available on [the CAN
Asset Tracker Project
Page](https://projects.golioth.io/reference-designs/can-asset-tracker/).

This repository contains the firmware source code and [pre-built release
firmware
images](https://github.com/golioth/reference-design-can-asset-tracker/releases).

## Supported Hardware

- Nordic nRF9160-DK
- Golioth Aludel Elixir
- Golioth Aludel Mini

### Additional Sensors/Components

- u-blox NEO-M9N GNSS module
- Microchip MCP2515 CAN controller
- Texas Instruments SN65HVD230 CAN Transceiver

## Golioth Features


This app implements:

  - [Device Settings
    Service](https://docs.golioth.io/firmware/golioth-firmware-sdk/device-settings-service)
  - [Remote Procedure Call
    (RPC)](https://docs.golioth.io/firmware/golioth-firmware-sdk/remote-procedure-call)
  - [Stream
    Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/stream-client)
  - [LightDB State
    Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/light-db-state/)
  - [Over-the-Air (OTA) Firmware
    Upgrade](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
  - [Backend
    Logging](https://docs.golioth.io/device-management/logging/)

### Settings Service

The following settings should be set in the Device Settings menu of the
[Golioth Console](https://console.golioth.io).

  - `LOOP_DELAY_S`
    Adjusts the delay between sensor readings. Set to an integer value
    (seconds).

    Default value is `5` seconds.

  - `GPS_DELAY_S`
    Adjusts the delay between recording GPS readings. Set to an integer
    value (seconds).

    Default value is `3` seconds.

  - `FAKE_GPS_ENABLED`
    Controls whether fake GPS position data is reported when a real GPS
    location signal is unavailable. Set to a boolean value.

    Default value is `false`.

  - `FAKE_GPS_LATITUDE`
    Sets the fake latitude value to be used when fake GPS is enabled.
    Set to a floating point value (`-90.0` to `90.0`).

    Default value is `37.789980`.

  - `FAKE_GPS_LONGITUDE`
    Sets the fake longitude value to be used when fake GPS is enabled.
    Set to a floating point value (`-180.0` to `180.0`).

    Default value is `-122.400860`.

  - `VEHICLE_SPEED_DELAY_S`
    Adjusts the delay between vehicle speed readings. Set to an integer
    value (seconds).

    Default value is `1` second.

### Remote Procedure Call (RPC) Service

The following RPCs can be initiated in the Remote Procedure Call menu of
the [Golioth Console](https://console.golioth.io).

  - `get_network_info`
    Query and return network information.

  - `reboot`
    Reboot the system.

  - `set_log_level`
    Set the log level.

    The method takes a single parameter which can be one of the
    following integer values:

      - `0`: `LOG_LEVEL_NONE`
      - `1`: `LOG_LEVEL_ERR`
      - `2`: `LOG_LEVEL_WRN`
      - `3`: `LOG_LEVEL_INF`
      - `4`: `LOG_LEVEL_DBG`

### Time-Series Stream data

Vehicle data is periodically sent to the following paths of the
LightDB Stream service:

  - `gps/lat`: Latitude (°)
  - `gps/lon`: Longitude (°)
  - `gps/fake`: `true` if GPS location data is fake, otherwise `false`
  - `vehicle/speed`: Vehicle Speed (km/h)

``` json
{
  "tracker": {
    "gps": {
      "fake": false,
      "lat": 43.081852,
      "lon": -89.305275
    },
    "time": "2024-12-13T23:32:53.000Z",
    "vehicle": {
      "speed": 15
    }
  }
}
```

If your board includes a battery, voltage and level readings
will be sent to the `battery` path.

> [!NOTE]
> Your Golioth project must have a Pipeline enabled to receive this
> data. See the [Add Pipeline to Golioth](#add-pipeline-to-golioth)
> section below.

### Stateful Data (LightDB State)

The concept of Digital Twin is demonstrated with the LightDB State
`example_int0` and `example_int1` variables that are subpaths of the
`desired` and `state` paths.

  - `desired` values may be changed from the cloud side. The device will
    recognize these, validate them for \[0..65535\] bounding, and then
    reset these values to `-1`
  - `state` values will be updated by the device to reflect the device's
    actual stored value. The cloud may read the `state` endpoints to
    determine device status. In this arrangement, only the device
    should ever write to the `state` endpoints.

``` json
{
  "desired": {
    "example_int0": -1,
    "example_int1": -1
  },
  "state": {
    "example_int0": 0,
    "example_int1": 1
  }
}
```

By default the state values will be `0` and `1`. Try updating the
`desired` values and observe how the device updates its state.

### OTA Firmware Update

This application includes the ability to perform Over-the-Air (OTA)
firmware updates. To do so, you need a binary compiled with a different
version number than what is currently running on the device.

> [!NOTE]
> If a newer release is available than what your device is currently
> running, you may download the pre-compiled binary that ends in
> `_update.bin` and use it in step 2 below.

1. Update the version number in the `VERSION` file and perform a
   pristine (important) build to incorporate the version change.
2. Upload the `build/app/zephyr/zephyr.signed.bin` file as a Package for
   your Golioth project.

   - Use `main` as the package name.
   - Use the same version number from step 1.

3. Create a Cohort and add your device to it.
4. Create a Deployment for your Cohort using the package name and
   version number from step 2.
5. Devices in your Cohort will automatically upgrade to the most
   recently deployed firmware.

Visit [the Golioth Docs OTA Firmware Upgrade
page](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
for more info.

### Further Information in Header Files

Please refer to the comments in each header file for a
service-by-service explanation of this template.

## Add Pipeline to Golioth

Golioth uses [Pipelines](https://docs.golioth.io/data-routing) to route
stream data. This gives you flexibility to change your data routing
without requiring updated device firmware.

Whenever sending stream data, you must enable a pipeline in your Golioth
project to configure how that data is handled. Add the contents of
`pipelines/json-to-lightdb.yml` as a new pipeline as follows (note that
this is the default pipeline for new projects and may already be
present):

1.  Navigate to your project on the Golioth web console.
2.  Select `Pipelines` from the left sidebar and click the `Create`
    button.
3.  Give your new pipeline a name and paste the pipeline configuration
    into the editor.
4.  Click the toggle in the bottom right to enable the pipeline and
    then click `Create`.

All data streamed to Golioth in JSON format will now be routed to
LightDB Stream and may be viewed using the web console. You may change
this behavior at any time without updating firmware simply by editing
this pipeline entry.

## Local set up

> [!IMPORTANT]
> Do not clone this repo using git. Zephyr's `west` meta tool should be
> used to set up your local workspace.

### Install the Python virtual environment (recommended)

``` shell
cd ~
mkdir golioth-reference-design-coldchain
python -m venv golioth-reference-design-coldchain/.venv
source golioth-reference-design-coldchain/.venv/bin/activate
pip install wheel west ecdsa
```

### Use `west` to initialize and install

``` shell
cd ~/golioth-reference-design-coldchain
west init -m https://github.com/golioth/reference-design-coldchain.git .
west update
west zephyr-export
pip install -r deps/zephyr/scripts/requirements.txt
```

## Building the application

Build the Zephyr sample application for the [Nordic nRF9160
DK](https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk)
(`nrf9160dk_nrf9160_ns`) from the top level of your project. After a
successful build you will see a new `build` directory. Note that any
changes (and git commits) to the project itself will be inside the `app`
folder. The `build` and `deps` directories being one level higher
prevents the repo from cataloging all of the changes to the dependencies
and the build (so no `.gitignore` is needed).

Prior to building, update `VERSION` file to reflect the firmware version
number you want to assign to this build. Then run the following commands
to build and program the firmware onto the device.

> [!WARNING]
> You must perform a pristine build (use `-p` or remove the `build`
> directory) after changing the firmware version number in the `VERSION`
> file for the change to take effect.

``` text
$ (.venv) west build -p -b nrf9160dk/nrf9160/ns --sysbuild app
$ (.venv) west flash
```

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

``` text
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:~$ kernel reboot cold
```

## External Libraries

The following code libraries are installed by default. If you are not
using the custom hardware to which they apply, you can safely remove
these repositories from `west.yml` and remove the includes/function
calls from the C code.

  - [golioth-zephyr-boards](https://github.com/golioth/golioth-zephyr-boards)
    includes the board definitions for the Golioth Aludel-Mini
  - [libostentus](https://github.com/golioth/libostentus) is a helper
    library for controlling the Ostentus ePaper faceplate
  - [zephyr-network-info](https://github.com/golioth/zephyr-network-info)
    is a helper library for querying, formatting, and returning network
    connection information via Zephyr log or Golioth RPC

## Have Questions?

Please get in touch with Golioth engineers by starting a new thread on
the [Golioth Forum](https://forum.golioth.io/).
